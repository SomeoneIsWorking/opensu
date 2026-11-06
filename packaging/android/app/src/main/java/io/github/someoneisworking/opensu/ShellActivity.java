package io.github.someoneisworking.opensu;

import android.content.Context;
import android.content.Intent;
import android.graphics.PixelFormat;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.Messenger;
import android.provider.Settings;
import android.util.Log;
import android.view.ViewGroup;

import org.libsdl.app.SDLSurface;

import java.io.IOException;

/** The shell: libmain.so on SDL, with the launches native code asks for. */
public final class ShellActivity extends LandscapeActivity {
    private Launches launches;
    private MusicVolume volume;
    private DocumentTree documents;
    /** The LAN announcement and browsing while LAN is on, else null. */
    private LanDiscovery lan;
    private WifiHold wifiHold;
    private static final int PICK_FOLDER = 0x5af;
    /** The folder picker's answer until native code takes it: a tree URI, "" when cancelled. */
    private String pickedFolder;
    /** The shell came to the front over a running game, so its Guide menu opens; taken once. */
    private boolean menuRequested;
    /** The game opened the shell and nothing has come between them since. */
    private volatile boolean overGame;
    private final Messenger runnerInbox = new Messenger(new Handler(Looper.getMainLooper(), message -> {
        if (message.what == RetroService.MENU) {
            requestMenu();
        } else if (message.what == RetroService.TURN) {
            turnScreen(message.arg1 != 0);
        }
        return true;
    }));

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        try {
            AssetInstall.ensure(this);
        } catch (IOException error) {
            // The shell reports the missing typeface itself; this says why it is missing.
            Log.e("openSU", "the shipped assets could not be installed: " + error.getMessage());
        }
        launches = new Launches(this);
        volume = new MusicVolume(this);
        documents = new DocumentTree(this);
        super.onCreate(savedInstanceState);
        seeThrough(false);
    }

    // Its alpha reaches the compositor, so what it leaves clear shows the game behind it.
    @Override
    protected SDLSurface createSDLSurface(Context context) {
        SDLSurface surface = super.createSDLSurface(context);
        surface.getHolder().setFormat(PixelFormat.TRANSLUCENT);
        // Above the SurfaceView a game in this window draws on.
        surface.setZOrderMediaOverlay(true);
        return surface;
    }

    /** Where the runner's process tells the shell what it cannot hear; every message is oneway. */
    Messenger runnerInbox() {
        return runnerInbox;
    }

    /** What SDL's surface sits in. */
    ViewGroup contentLayout() {
        return (ViewGroup) getContentView();
    }

    /** Whether Android keeps what is behind the shell on screen; below API 30 it always does. */
    void seeThrough(boolean through) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            runOnUiThread(() -> setTranslucent(through));
        }
    }

    @Override
    protected void onPause() {
        launches.shellPaused();
        super.onPause();
    }

    @Override
    protected void onResume() {
        super.onResume();
        launches.shellResumed();
        // Back from the launcher the game goes first, or the home screen would show behind the menu.
        if (launches.running() && (overGame || !launches.resume())) {
            synchronized (this) {
                menuRequested = true;
            }
        }
    }

    @Override
    protected void onUserLeaveHint() {
        super.onUserLeaveHint();
        overGame = false;
    }

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        overGame = intent.getBooleanExtra(RetroActivity.EXTRA_FROM_GAME, false);
    }

    /** Called from the intent ProcessLauncher backend: null once started, else why not. */
    public String launch(String program, String[] args, String[] environment) {
        synchronized (this) {
            menuRequested = false;
        }
        return launches.start(program, args, environment);
    }

    /** The game in this window asked for the Guide menu. */
    synchronized void requestMenu() {
        menuRequested = true;
    }

    /** Called from the Android game overlay: whether the Guide menu was asked for since last time. */
    public synchronized boolean takeMenuRequest() {
        boolean requested = menuRequested;
        menuRequested = false;
        return requested;
    }

    /** Called from the Android game overlay: the shell comes to the front, the game over. */
    public void showShell() {
        launches.showShell();
    }

    /** Called from the Android game overlay: the running game comes back to the front. */
    public boolean resumeGame() {
        overGame = false;
        return launches.resume();
    }

    /** Whether the last launch is still running. */
    public boolean launchRunning() {
        return launches.running();
    }

    /** How the last launch ended: 128 plus the signal, or 0 for a plain end. */
    public int launchExitStatus() {
        return launches.exitStatus();
    }

    /** Ends the last launch where it is ours to end (the runner), and returns once it is gone. */
    public void stopLaunch() {
        launches.stop();
    }

    /** Called from the AudioManager volume backend: {percent, muted ? 1 : 0}. */
    public int[] musicVolume() {
        return volume.read();
    }

    public boolean setMusicVolume(int percent) {
        return volume.setPercent(percent);
    }

    public boolean setMusicMuted(boolean muted) {
        return volume.setMuted(muted);
    }

    /** Called from the folder picker backend: opens the system's folder picker. */
    public void pickFolder() {
        runOnUiThread(() -> {
            Intent pick = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
            pick.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION
                    | Intent.FLAG_GRANT_WRITE_URI_PERMISSION
                    | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
            startActivityForResult(pick, PICK_FOLDER);
        });
    }

    /** The picked folder once the picker closed ("" when cancelled), else null; taken once. */
    public synchronized String takePickedFolder() {
        String answer = pickedFolder;
        pickedFolder = null;
        return answer;
    }

    /** Called from the emulator search: packages with a launcher activity. */
    public String[] installedApps() {
        Intent launchers = new Intent(Intent.ACTION_MAIN).addCategory(Intent.CATEGORY_LAUNCHER);
        java.util.Set<String> packages = new java.util.TreeSet<>();
        for (android.content.pm.ResolveInfo info
                : getPackageManager().queryIntentActivities(launchers, 0)) {
            packages.add(info.activityInfo.packageName);
        }
        return packages.toArray(new String[0]);
    }

    /** Called from the document RomTree: a picked folder's name. */
    public String documentLabel(String location) {
        return documents.label(location);
    }

    /** Called from the document RomTree: the children of a picked folder or a folder in it. */
    public String[] listDocuments(String location) {
        return documents.children(location);
    }

    /** Called from the document RomTree: a document open for reading, its descriptor the caller's. */
    public int openDocument(String location) {
        return documents.open(location);
    }

    /** Called from the document RomTree: whether openSU may write in a picked folder. */
    public boolean documentWritable(String location) {
        return documents.writable(location);
    }

    /** Called from the document RomTree: gives back the grant on a folder no longer used. */
    public void releaseDocument(String location) {
        documents.release(location);
    }

    /** Called from the document RomTree: puts a staged file in a picked folder; null, else why not. */
    public String placeDocument(String location, String relative, String file) {
        return documents.place(location, relative, file);
    }

    /** Called from the Android DNS-SD backend: announces and browses; null, else why it cannot. */
    public synchronized String lanStart(String id, String name, int port) {
        if (lan != null) {
            lan.stop();
        }
        lan = new LanDiscovery(this, id, name, port);
        String refusal = lan.start();
        if (refusal != null) {
            lan = null;
        }
        return refusal;
    }

    /** Called from native code while a remote game streams here. */
    public synchronized void holdWifi(boolean held) {
        if (wifiHold == null) {
            wifiHold = new WifiHold(this);
        }
        wifiHold.set(held);
    }

    public synchronized void lanStop() {
        if (lan != null) {
            lan.stop();
            lan = null;
        }
    }

    /** The other openSUs found: "id TAB name TAB address TAB port" each. */
    public synchronized String[] lanSightings() {
        return lan != null ? lan.sightings() : new String[0];
    }

    public synchronized String lanAnnouncedName() {
        return lan != null ? lan.announcedName() : "";
    }

    /** The name the player gave this device, else its model. */
    public String lanMachineName() {
        String named = Settings.Global.getString(getContentResolver(), Settings.Global.DEVICE_NAME);
        return named != null && !named.isEmpty() ? named : Build.MODEL;
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (requestCode != PICK_FOLDER) {
            super.onActivityResult(requestCode, resultCode, data);
            return;
        }
        Uri tree = resultCode == RESULT_OK && data != null ? data.getData() : null;
        if (tree != null) {
            // Kept across restarts, so the library reads the folder at every start and ROMs
            // imported over the LAN can be written into it.
            getContentResolver().takePersistableUriPermission(tree,
                    Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
        }
        synchronized (this) {
            pickedFolder = tree != null ? tree.toString() : "";
        }
    }
}
