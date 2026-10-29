package io.github.someoneisworking.opensu;

import android.content.Intent;
import android.os.Bundle;
import android.system.ErrnoException;
import android.system.Os;
import android.util.Log;

import java.io.File;
import java.io.IOException;

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

    // Runs on SDL's main thread, so a large game is copied off the UI thread.
    @Override
    protected String[] getArguments() {
        String[] arguments = getIntent().getStringArrayExtra(EXTRA_ARGUMENTS);
        if (arguments == null) {
            return new String[0];
        }
        File staged = new File(getCacheDir(), "staged");
        File[] old = staged.listFiles();
        if (old != null) {
            for (File file : old) {
                file.delete();
            }
        }
        staged.mkdirs();
        DocumentTree documents = new DocumentTree(this);
        for (int i = 0; i < arguments.length; ++i) {
            if (!DocumentTree.isDocument(arguments[i])) {
                continue;
            }
            // Cores open games by path, so a game in a picked folder is copied to the cache first.
            try {
                arguments[i] = documents.stage(arguments[i], staged).getAbsolutePath();
            } catch (IOException | SecurityException error) {
                Log.e("openSU", "cannot copy " + arguments[i] + ": " + error.getMessage());
            }
        }
        return arguments;
    }

    /** Called from the runner's shell handoff: the shell comes to the front, where its Guide menu opens. */
    public void openShellMenu() {
        startActivity(new Intent(this, ShellActivity.class)
                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_REORDER_TO_FRONT));
    }

    // The process is the game's: it ends with the runner, so the next launch starts clean.
    @Override
    protected void main() {
        super.main();
        android.os.Process.killProcess(android.os.Process.myPid());
    }
}
