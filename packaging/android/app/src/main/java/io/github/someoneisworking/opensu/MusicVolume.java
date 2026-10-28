package io.github.someoneisworking.opensu;

import android.content.Context;
import android.media.AudioManager;

/** The media stream's volume, which games and the device's volume keys share. */
final class MusicVolume {
    private final AudioManager audio;

    MusicVolume(Context context) {
        audio = context.getSystemService(AudioManager.class);
    }

    /** {percent, muted ? 1 : 0}. */
    int[] read() {
        int max = audio.getStreamMaxVolume(AudioManager.STREAM_MUSIC);
        int now = audio.getStreamVolume(AudioManager.STREAM_MUSIC);
        int percent = max > 0 ? Math.round(now * 100.0f / max) : 0;
        boolean muted = audio.isStreamMute(AudioManager.STREAM_MUSIC);
        return new int[] {percent, muted ? 1 : 0};
    }

    boolean setPercent(int percent) {
        int max = audio.getStreamMaxVolume(AudioManager.STREAM_MUSIC);
        int index = Math.round(Math.max(0, Math.min(100, percent)) * max / 100.0f);
        audio.setStreamVolume(AudioManager.STREAM_MUSIC, index, 0);
        return audio.getStreamVolume(AudioManager.STREAM_MUSIC) == index;
    }

    boolean setMuted(boolean muted) {
        audio.adjustStreamVolume(AudioManager.STREAM_MUSIC,
                muted ? AudioManager.ADJUST_MUTE : AudioManager.ADJUST_UNMUTE, 0);
        return audio.isStreamMute(AudioManager.STREAM_MUSIC) == muted;
    }
}
