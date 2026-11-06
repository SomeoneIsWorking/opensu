package io.github.someoneisworking.opensu;

import java.io.File;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;

/**
 * The staged folder on disk: one folder per game folder, each game beside a hidden note of the
 * document it came from. A game's modified time is its last use.
 */
final class StagedFiles {
    /** How much staged games may take; the least recently played go first. */
    static final long BUDGET = 4L << 30;
    private static final String PART = ".part";

    private StagedFiles() {
    }

    static File noteOf(File staged) {
        return new File(staged.getParentFile(), "." + staged.getName() + ".source");
    }

    static File partOf(File staged) {
        return new File(staged.getParentFile(), staged.getName() + PART);
    }

    /** Brings `root` under `budget` bytes without touching `keep`, a folder in use. */
    static void trim(File root, long budget, File keep) {
        File[] entries = root.listFiles();
        if (entries == null) {
            return;
        }
        List<File> games = new ArrayList<>();
        long total = 0;
        for (File entry : entries) {
            if (!entry.isDirectory()) {
                entry.delete();
                continue;
            }
            File[] files = entry.listFiles();
            if (files == null) {
                continue;
            }
            for (File file : files) {
                if (file.getName().endsWith(PART)) {
                    file.delete();
                } else if (!file.getName().startsWith(".")) {
                    total += file.length();
                    if (!entry.equals(keep)) {
                        games.add(file);
                    }
                }
            }
        }
        games.sort(Comparator.comparingLong(File::lastModified));
        for (File game : games) {
            if (total <= budget) {
                break;
            }
            total -= game.length();
            noteOf(game).delete();
            game.delete();
        }
        for (File entry : entries) {
            String[] left = entry.list();
            if (left != null && left.length == 0 && !entry.equals(keep)) {
                entry.delete();
            }
        }
    }
}
