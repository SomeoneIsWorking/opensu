package io.github.someoneisworking.opensu;

import android.content.pm.ActivityInfo;

import io.github.someoneisworking.android.AndroidActivity;

/** openSU is landscape, as iiSU is; SDL would otherwise pick an orientation from the window size. */
public abstract class LandscapeActivity extends AndroidActivity {
    @Override
    public void setOrientationBis(int width, int height, boolean resizable, String hint) {
        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE);
    }
}
