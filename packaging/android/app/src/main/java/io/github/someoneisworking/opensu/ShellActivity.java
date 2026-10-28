package io.github.someoneisworking.opensu;

import android.os.Bundle;
import android.util.Log;

import java.io.IOException;

/** The shell: libmain.so on SDL, with the launches native code asks for. */
public final class ShellActivity extends LandscapeActivity {
    private Launches launches;
    private MusicVolume volume;

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
}
