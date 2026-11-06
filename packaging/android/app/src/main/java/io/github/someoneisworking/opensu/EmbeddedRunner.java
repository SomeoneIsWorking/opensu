package io.github.someoneisworking.opensu;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.graphics.PixelFormat;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.Message;
import android.os.Messenger;
import android.os.RemoteException;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.ViewGroup;

/**
 * The runner in {@link RetroService}, drawing into a SurfaceView under the shell's own, whose clear
 * pixels show it. Every message to it is oneway, so a hung game cannot stall the shell.
 */
final class EmbeddedRunner implements RunnerHost, ServiceConnection, SurfaceHolder.Callback {
    private final ShellActivity shell;
    private final Messenger inbox;
    private volatile boolean bound;
    private volatile String[] arguments;
    private volatile String[] environment;
    // These belong to the UI thread.
    private SurfaceView view;
    private Messenger runner;
    private Surface surface;
    private int width;
    private int height;

    EmbeddedRunner(ShellActivity shell) {
        this.shell = shell;
        inbox = new Messenger(new Handler(Looper.getMainLooper(), message -> {
            if (message.what == RetroService.MENU) {
                shell.requestMenu();
            }
            return true;
        }));
    }

    @Override
    public String start(String[] arguments, String[] environment) {
        this.arguments = arguments;
        this.environment = environment;
        Intent service = new Intent(shell, RetroService.class);
        int flags = Context.BIND_AUTO_CREATE | Context.BIND_IMPORTANT;
        try {
            if (!shell.bindService(service, this, flags)) {
                shell.unbindService(this);
                return "the runner's service cannot be bound";
            }
        } catch (SecurityException refused) {
            return refused.getMessage();
        }
        bound = true;
        shell.runOnUiThread(this::showView);
        return null;
    }

    @Override
    public boolean standing() {
        return bound;
    }

    @Override
    public boolean resume() {
        return bound;
    }

    @Override
    public void showShell() {
    }

    @Override
    public void stop() {
    }

    @Override
    public void ended() {
        shell.runOnUiThread(this::close);
    }

    private void showView() {
        if (!bound || view != null) {
            return;
        }
        view = new SurfaceView(shell);
        view.getHolder().setFormat(PixelFormat.OPAQUE);
        view.getHolder().addCallback(this);
        view.setKeepScreenOn(true);
        shell.contentLayout().addView(view, 0, new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
    }

    private void close() {
        if (view != null) {
            view.getHolder().removeCallback(this);
            shell.contentLayout().removeView(view);
            view = null;
        }
        surface = null;
        runner = null;
        if (bound) {
            bound = false;
            shell.unbindService(this);
        }
    }

    @Override
    public void onServiceConnected(ComponentName name, IBinder service) {
        runner = new Messenger(service);
        Message start = Message.obtain(null, RetroService.START);
        Bundle launch = new Bundle();
        launch.putStringArray(RetroActivity.EXTRA_ARGUMENTS, arguments);
        launch.putStringArray(RetroActivity.EXTRA_ENVIRONMENT, environment);
        start.setData(launch);
        start.replyTo = inbox;
        send(start);
        if (surface != null) {
            sendSurface();
        }
    }

    // Its process died; unbinding keeps Android from starting the service again for this binding.
    @Override
    public void onServiceDisconnected(ComponentName name) {
        close();
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        surface = holder.getSurface();
        sendSurface();
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        if (width != this.width || height != this.height) {
            this.width = width;
            this.height = height;
            send(Message.obtain(null, RetroService.SIZE));
        }
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        surface = null;
        sendSurface();
    }

    private void sendSurface() {
        send(Message.obtain(null, RetroService.SURFACE, surface));
    }

    private void send(Message message) {
        if (runner == null) {
            return;
        }
        try {
            runner.send(message);
        } catch (RemoteException gone) {
            Log.w("openSU", "the runner did not take a message: " + gone.getMessage());
        }
    }
}
