package io.github.someoneisworking.opensu;

import android.content.pm.ActivityInfo;

import io.github.someoneisworking.android.AndroidActivity;

/**
 * openSU is landscape, as iiSU is, until the player presses Rotate screen; SDL would otherwise pick
 * an orientation from the window size.
 */
public abstract class LandscapeActivity extends AndroidActivity {
    private int orientation = ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE;

    @Override
    public void setOrientationBis(int width, int height, boolean resizable, String hint) {
        setRequestedOrientation(orientation);
    }

    /** Rotate screen; fixed, so the sensor does not take it back. Any thread. */
    public void turnScreen(boolean portrait) {
        stand(portrait
                ? ActivityInfo.SCREEN_ORIENTATION_PORTRAIT
                : ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
    }

    /** The game that chose an orientation is over. Any thread. */
    void endTurn() {
        stand(ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE);
    }

    private void stand(int wanted) {
        runOnUiThread(() -> {
            orientation = wanted;
            setRequestedOrientation(wanted);
        });
    }
}
