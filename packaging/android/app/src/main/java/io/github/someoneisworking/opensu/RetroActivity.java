package io.github.someoneisworking.opensu;

import android.os.Bundle;
import android.system.ErrnoException;
import android.system.Os;
import android.util.Log;


/** openSU's libretro runner, one game per `:retro` process, as opensu-retro is on the desktop. */
public final class RetroActivity extends LandscapeActivity {
    static final String EXTRA_ARGUMENTS = "io.github.someoneisworking.opensu.ARGUMENTS";
    static final String EXTRA_ENVIRONMENT = "io.github.someoneisworking.opensu.ENVIRONMENT";
    static final String PROCESS_SUFFIX = ":retro";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        // The launch's environment, read by the runner's config before anything else.
        String[] environment = getIntent().getStringArrayExtra(EXTRA_ENVIRONMENT);
        if (environment != null) {
            for (String entry : environment) {
                int equals = entry.indexOf('=');
                if (equals <= 0) {
                    continue;
                }
                try {
                    Os.setenv(entry.substring(0, equals), entry.substring(equals + 1), true);
                } catch (ErrnoException error) {
                    Log.e("openSU", "cannot set " + entry.substring(0, equals) + ": " + error.getMessage());
                }
            }
        }
        super.onCreate(savedInstanceState);
    }

    @Override
    protected String[] getLibraries() {
        return new String[] {"SDL3", "opensu-retro"};
    }

    @Override
    protected String[] getArguments() {
        String[] arguments = getIntent().getStringArrayExtra(EXTRA_ARGUMENTS);
        return arguments != null ? arguments : new String[0];
    }

    // The process is the game's: it ends with the runner, so the next launch starts clean.
    @Override
    protected void main() {
        super.main();
        android.os.Process.killProcess(android.os.Process.myPid());
    }
}
