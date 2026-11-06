package io.github.someoneisworking.opensu;

import android.app.Service;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.Message;
import android.os.Messenger;
import android.os.Process;
import android.os.RemoteException;
import android.util.Log;
import android.view.Surface;

import io.github.someoneisworking.android.AndroidStdioLog;

import org.libsdl.app.SDL;
import org.libsdl.app.SDLActivity;

/**
 * openSU's libretro runner with no window of its own, one game per `:retro` process: it draws on
 * the shell's {@link Surface} and hears its input over the control channel.
 */
public final class RetroService extends Service {
    static final int START = 1;
    static final int SURFACE = 2;
    static final int SIZE = 3;
    static final int MENU = 4;
    /** To the shell: Rotate screen, arg1 1 for portrait. */
    static final int TURN = 5;
    private static final String RUNNER = "opensu-retro";

    private final Messenger inbox = new Messenger(new Handler(Looper.getMainLooper(), message -> {
        receive(message);
        return true;
    }));
    private RunnerContext context;

    /** The shell stands as the game does, over it and with the game in its window. */
    static void tellTurn(Messenger shell, boolean portrait) {
        try {
            shell.send(Message.obtain(null, TURN, portrait ? 1 : 0, 0));
        } catch (RemoteException gone) {
            Log.w("openSU", "the shell did not take the screen's turn: " + gone.getMessage());
        }
    }

    @Override
    public void onCreate() {
        super.onCreate();
        AndroidStdioLog.redirect(getPackageName());
        context = new RunnerContext(this);
    }

    @Override
    public IBinder onBind(Intent intent) {
        return inbox.getBinder();
    }

    // The shell is gone or done with the game, and the process is the game's.
    @Override
    public void onDestroy() {
        super.onDestroy();
        Process.killProcess(Process.myPid());
    }

    private void receive(Message message) {
        switch (message.what) {
            case START:
                context.reportsTo(message.replyTo);
                start(message.getData());
                break;
            case SURFACE:
                context.surface((Surface) message.obj);
                break;
            case SIZE:
                context.resized();
                break;
            default:
                break;
        }
    }

    private void start(Bundle launch) {
        RunnerEnvironment.apply(launch.getStringArray(RetroActivity.EXTRA_ENVIRONMENT));
        System.loadLibrary("SDL3");
        System.loadLibrary(RUNNER);
        SDL.setupJNI();
        SDL.initialize();
        SDL.setContext(context);
        String[] arguments = launch.getStringArray(RetroActivity.EXTRA_ARGUMENTS);
        new Thread(() -> run(arguments), "SDLThread").start();
    }

    private void run(String[] arguments) {
        Process.setThreadPriority(Process.THREAD_PRIORITY_DISPLAY);
        SDLActivity.nativeInitMainThread();
        SDLActivity.nativeRunMain("lib" + RUNNER + ".so", "SDL_main",
                new GameStaging(this).resolve(arguments));
        SDLActivity.nativeCleanupMainThread();
        Process.killProcess(Process.myPid());
    }
}
