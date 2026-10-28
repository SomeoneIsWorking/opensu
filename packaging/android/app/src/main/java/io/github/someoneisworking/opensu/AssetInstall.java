package io.github.someoneisworking.opensu;

import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.res.AssetManager;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;

/** Copies the APK's assets/opensu to filesDir/share/opensu once per installed version. */
final class AssetInstall {
    private static final String SOURCE = "opensu";

    private AssetInstall() {
    }

    static void ensure(Context context) throws IOException {
        File root = new File(context.getFilesDir(), "share/opensu");
        File stamp = new File(context.getFilesDir(), "share/opensu.version");
        String version = installedVersion(context);
        if (stamp.isFile() && root.isDirectory()
                && version.equals(new String(Files.readAllBytes(stamp.toPath()), StandardCharsets.UTF_8))) {
            return;
        }
        deleteTree(root);
        copyTree(context.getAssets(), SOURCE, root);
        try (OutputStream out = new FileOutputStream(stamp)) {
            out.write(version.getBytes(StandardCharsets.UTF_8));
        }
    }

    // A reinstall of the same versionCode still changes lastUpdateTime.
    private static String installedVersion(Context context) throws IOException {
        try {
            PackageInfo info = context.getPackageManager().getPackageInfo(context.getPackageName(), 0);
            return info.getLongVersionCode() + ":" + info.lastUpdateTime;
        } catch (PackageManager.NameNotFoundException missing) {
            throw new IOException("the installed package cannot be read", missing);
        }
    }

    private static void copyTree(AssetManager assets, String path, File destination) throws IOException {
        String[] children = assets.list(path);
        if (children != null && children.length > 0) {
            if (!destination.isDirectory() && !destination.mkdirs()) {
                throw new IOException("cannot create " + destination);
            }
            for (String child : children) {
                copyTree(assets, path + "/" + child, new File(destination, child));
            }
            return;
        }
        try (InputStream in = assets.open(path); OutputStream out = new FileOutputStream(destination)) {
            byte[] buffer = new byte[64 * 1024];
            int read;
            while ((read = in.read(buffer)) > 0) {
                out.write(buffer, 0, read);
            }
        }
    }

    private static void deleteTree(File file) throws IOException {
        File[] children = file.listFiles();
        if (children != null) {
            for (File child : children) {
                deleteTree(child);
            }
        }
        if (file.exists() && !file.delete()) {
            throw new IOException("cannot remove " + file);
        }
    }
}
