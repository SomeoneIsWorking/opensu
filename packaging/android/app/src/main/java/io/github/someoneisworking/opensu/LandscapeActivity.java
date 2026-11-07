package io.github.someoneisworking.opensu;

import android.content.pm.ActivityInfo;

import io.github.someoneisworking.android.AndroidActivity;

/**
 * openSU stands where the player chose, landscape as iiSU does unless they chose portrait, and a
 * game's Rotate screen turns it from there; SDL would otherwise pick an orientation from the
 * window size.
 */
public abstract class LandscapeActivity extends AndroidActivity {
    private int rest = ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE;
    private int orientation = rest;

    @Override
    public void setOrientationBis(int width, int height, boolean resizable, String hint) {
        setRequestedOrientation(orientation);
    }

    /** Where the shell rests; a turned game keeps its own until it ends. Any thread. */
    public void standShell(boolean portrait) {
        runOnUiThread(() -> {
            boolean resting = orientation == rest;
            rest = portrait
                    ? ActivityInfo.SCREEN_ORIENTATION_SENSOR_PORTRAIT
                    : ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE;
            if (resting) {
                orientation = rest;
                setRequestedOrientation(rest);
            }
        });
    }

    /** Rotate screen; fixed, so the sensor does not take it back. Any thread. */
    public void turnScreen(boolean portrait) {
        stand(portrait
                ? ActivityInfo.SCREEN_ORIENTATION_PORTRAIT
                : ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
    }

    /** The game that chose an orientation is over. Any thread. */
    void endTurn() {
        runOnUiThread(() -> stand(rest));
    }

    private void stand(int wanted) {
        runOnUiThread(() -> {
            orientation = wanted;
            setRequestedOrientation(wanted);
        });
    }
}
