package io.github.someoneisworking.opensu;

import android.content.Context;
import android.net.Uri;
import android.util.Log;

import java.io.File;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.List;

/**
 * Cores open games by path, so a game in a picked folder is copied to the cache first, and kept
 * while the document's size and date hold, so playing it again copies nothing.
 */
final class GameStaging {
    /** Names a document a core opens by path beside the game; romset_match.hpp's besideArgument. */
    private static final String BESIDE = "--beside";

    private final DocumentTree documents;
    private final File root;

    GameStaging(Context context) {
        documents = new DocumentTree(context);
        root = new File(context.getCacheDir(), "staged");
    }

    /** The runner's arguments: each document as a staged path, `--beside` ones staged only. */
    String[] resolve(String[] arguments) {
        if (arguments == null) {
            return new String[0];
        }
        File folder = null;
        List<String> runner = new ArrayList<>();
        for (int i = 0; i < arguments.length; ++i) {
            boolean beside = BESIDE.equals(arguments[i]) && i + 1 < arguments.length;
            String argument = beside ? arguments[++i] : arguments[i];
            if (DocumentTree.isDocument(argument)) {
                try {
                    // A launch's documents share a folder, so a core finds one beside the other.
                    folder = folder != null ? folder : folderOf(argument);
                    argument = stage(argument, folder).getAbsolutePath();
                } catch (IOException | SecurityException error) {
                    Log.e("openSU", "cannot copy " + argument + ": " + error.getMessage());
                }
            }
            if (!beside) {
                runner.add(argument);
            }
        }
        return runner.toArray(new String[0]);
    }

    private File stage(String location, File folder) throws IOException {
        Uri uri = Uri.parse(location);
        File staged = new File(folder, new File(documents.name(uri)).getName());
        File note = StagedFiles.noteOf(staged);
        String stamp = documents.stamp(uri);
        String source = location + "\n" + stamp;
        if (stamp != null && staged.isFile() && note.isFile()
                && source.equals(new String(Files.readAllBytes(note.toPath()),
                        StandardCharsets.UTF_8))) {
            staged.setLastModified(System.currentTimeMillis());
            return staged;
        }
        note.delete();
        staged.delete();
        StagedFiles.trim(root, Math.max(StagedFiles.BUDGET - documents.size(uri), 0), folder);
        File part = StagedFiles.partOf(staged);
        documents.copy(uri, part);
        if (!part.renameTo(staged)) {
            throw new IOException("cannot put " + staged.getName() + " in place");
        }
        // Last, so a copy cut short is never taken for a whole one.
        if (stamp != null) {
            Files.write(note.toPath(), source.getBytes(StandardCharsets.UTF_8));
        }
        StagedFiles.trim(root, StagedFiles.BUDGET, folder);
        return staged;
    }

    /** The staged folder of the document's own folder, its URI up to the last separator. */
    private File folderOf(String location) throws IOException {
        int end = Math.max(location.lastIndexOf("%2F"), location.lastIndexOf('/'));
        File folder = new File(root, digest(location.substring(0, Math.max(end, 0))));
        if (!folder.isDirectory() && !folder.mkdirs()) {
            throw new IOException("cannot make " + folder);
        }
        return folder;
    }

    private static String digest(String text) {
        try {
            byte[] hash = MessageDigest.getInstance("SHA-1").digest(
                    text.getBytes(StandardCharsets.UTF_8));
            StringBuilder hex = new StringBuilder();
            for (byte value : hash) {
                hex.append(String.format("%02x", value));
            }
            return hex.toString();
        } catch (NoSuchAlgorithmException missing) {
            throw new IllegalStateException(missing);
        }
    }
}
