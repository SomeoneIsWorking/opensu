package io.github.someoneisworking.opensu;

import android.app.Activity;
import android.content.Context;
import android.os.Message;
import android.os.Messenger;
import android.os.RemoteException;
import android.util.Log;
import android.view.Surface;

/**
 * What {@link RetroService} gives SDL as its context, and what the runner's native code calls. SDL
 * declares its context an Activity; this one never attaches, so it has no window.
 */
public final class RunnerContext extends Activity {
    private Messenger shell;
    private Surface surface;
    private int surfaceSerial;
    private int sizeSerial;

    RunnerContext(Context service) {
        attachBaseContext(service);
    }

    synchronized void reportsTo(Messenger shell) {
        this.shell = shell;
    }

    synchronized void surface(Surface surface) {
        this.surface = surface;
        ++surfaceSerial;
        notifyAll();
    }

    synchronized void resized() {
        ++sizeSerial;
    }

    /** Called from the hosted window: returns once the shell has given a surface. */
    public synchronized void awaitSurface() throws InterruptedException {
        while (surface == null) {
            wait();
        }
    }

    /** Called from the hosted window: the surface now, null while there is none. */
    public synchronized Surface surface() {
        return surface;
    }

    /** Called from the hosted window: changes with each surface given or taken. */
    public synchronized int surfaceSerial() {
        return surfaceSerial;
    }

    /** Called from the hosted window: changes with each new size. */
    public synchronized int sizeSerial() {
        return sizeSerial;
    }

    /** Called from the runner's shell handoff: the shell's Activity is the one to turn. */
    public synchronized void turnScreen(boolean portrait) {
        if (shell != null) {
            RetroService.tellTurn(shell, portrait);
        }
    }

    /** Called from the runner's shell handoff: the shell opens its Guide menu over the game. */
    public synchronized void openShellMenu() {
        if (shell == null) {
            return;
        }
        try {
            shell.send(Message.obtain(null, RetroService.MENU));
        } catch (RemoteException gone) {
            Log.w("openSU", "the shell did not take the menu: " + gone.getMessage());
        }
    }
}
