package io.github.someoneisworking.opensu;

import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;

import java.io.IOException;

/** The shell: libmain.so on SDL, with the launches native code asks for. */
public final class ShellActivity extends LandscapeActivity {
    private Launches launches;
    private MusicVolume volume;
    private DocumentTree documents;
    private static final int PICK_FOLDER = 0x5af;
    /** The folder picker's answer until native code takes it: a tree URI, "" when cancelled. */
    private String pickedFolder;

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
    }

    /** Called from the intent ProcessLauncher backend: null once started, else why not. */
    public String launch(String program, String[] args, String[] environment) {
        return launches.start(program, args, environment);
    }

    /** Whether the last launch is still running. */
    public boolean launchRunning() {
        return launches.running();
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

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (requestCode != PICK_FOLDER) {
            super.onActivityResult(requestCode, resultCode, data);
            return;
        }
        Uri tree = resultCode == RESULT_OK && data != null ? data.getData() : null;
        if (tree != null) {
            // Kept across restarts, so the library can read the folder at every start.
            getContentResolver().takePersistableUriPermission(tree,
                    Intent.FLAG_GRANT_READ_URI_PERMISSION);
        }
        synchronized (this) {
            pickedFolder = tree != null ? tree.toString() : "";
        }
    }
}
