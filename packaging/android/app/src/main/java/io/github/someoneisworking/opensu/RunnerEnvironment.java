package io.github.someoneisworking.opensu;

import android.system.ErrnoException;
import android.system.Os;
import android.util.Log;

/** The launch's environment, set before the runner's config reads it. */
final class RunnerEnvironment {
    private RunnerEnvironment() {
    }

    static void apply(String[] environment) {
        if (environment == null) {
            return;
        }
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
}
