package io.github.someoneisworking.opensu;

import android.content.Context;
import android.net.wifi.WifiManager;
import android.os.Build;

/** Wi-Fi kept out of its power saving while a remote game streams to this tablet or phone. */
final class WifiHold {
    private final WifiManager.WifiLock lock;

    WifiHold(Context context) {
        WifiManager wifi = (WifiManager) context.getApplicationContext().getSystemService(Context.WIFI_SERVICE);
        int mode = Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q
                ? WifiManager.WIFI_MODE_FULL_LOW_LATENCY
                : WifiManager.WIFI_MODE_FULL_HIGH_PERF;
        lock = wifi != null ? wifi.createWifiLock(mode, "opensu-stream") : null;
        if (lock != null) {
            lock.setReferenceCounted(false);
        }
    }

    void set(boolean held) {
        if (lock == null) {
            return;
        }
        if (held) {
            lock.acquire();
        } else {
            lock.release();
        }
    }
}
