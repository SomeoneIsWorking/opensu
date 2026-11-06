package io.github.someoneisworking.opensu;

import android.app.ActivityManager;
import android.app.ApplicationExitInfo;
import android.content.ActivityNotFoundException;
import android.content.ClipData;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Process;
import android.system.OsConstants;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * The launches the shell starts: openSU's runner in the `:retro` process, or another app by an
 * explicit intent. A program ending in {@link #RUNNER_LIBRARY} is the runner; anything else is a
 * component, `package/class` or a bare package, with am-start style options for its intent.
 */
final class Launches {
    static final String RUNNER_LIBRARY = "/libopensu-retro.so";
    /** Runs the game in the shell's window; retro/embedded.hpp's embeddedArgument. */
    private static final String EMBEDDED = "--embedded";
    private static final long START_MILLIS = 10_000;
    // The system writes a process's exit record a moment after the process goes.
    private static final long EXIT_RECORD_MILLIS = 3_000;
    private static final int EXIT_RECORDS = 8;
    private static final int SIGNALLED = 128;

    private final ShellActivity shell;
    private final RunnerHost tasks;
    private final RunnerHost embedded;
    private RunnerHost host;
    private boolean runnerStarted;
    private boolean runnerSeen;
    private boolean foreign;
    private boolean foreignShown;
    private boolean foreignOver;
    private long startedAt;
    private long runnerGoneAt;
    private int runnerExit = -1;

    Launches(ShellActivity shell) {
        this.shell = shell;
        tasks = new TaskRunnerHost(shell);
        embedded = new EmbeddedRunner(shell);
        host = tasks;
    }

    synchronized String start(String program, String[] args, String[] environment) {
        runnerStarted = false;
        foreign = false;
        if (program.endsWith(RUNNER_LIBRARY)) {
            host = Arrays.asList(args).contains(EMBEDDED) ? embedded : tasks;
            String refused = host.start(args, environment);
            if (refused != null) {
                return program + " cannot be opened: " + refused;
            }
            startedAt = System.currentTimeMillis();
            runnerStarted = true;
            runnerSeen = false;
            runnerGoneAt = 0;
            runnerExit = -1;
            return null;
        }
        host = tasks;
        String failure = null;
        Intent intent = new Intent();
        try {
            applyOptions(intent, program, args);
        } catch (IllegalArgumentException bad) {
            failure = bad.getMessage();
        }
        if (failure != null) {
            return failure;
        }
        if (intent.getComponent() == null) {
            Intent launcher = shell.getPackageManager().getLaunchIntentForPackage(program);
            if (launcher == null) {
                return program + " is not installed";
            }
            launcher.setData(intent.getData());
            launcher.putExtras(intent);
            intent = launcher;
        }
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        try {
            shell.startActivity(intent);
        } catch (ActivityNotFoundException | SecurityException refused) {
            return program + " cannot be opened: " + refused.getMessage();
        }
        startedAt = System.currentTimeMillis();
        foreign = true;
        foreignShown = false;
        foreignOver = false;
        return null;
    }

    synchronized boolean running() {
        boolean starting = System.currentTimeMillis() - startedAt <= START_MILLIS;
        if (runnerStarted) {
            // The :retro process is forked after the host starts it.
            if (runnerPid() != 0) {
                runnerSeen = true;
                return true;
            }
            // The host stands while it forks and goes when it dies; the shell may not have looked.
            if (!runnerSeen && starting && host.standing()) {
                return true;
            }
            host.ended();
            return !exitSettled();
        }
        if (foreign && !foreignShown && !starting) {
            // The app never took the screen from the shell.
            foreignOver = true;
        }
        return foreign && !foreignOver;
    }

    /** How the last launch ended: 128 plus the signal, or 0 for a plain end or no record. */
    synchronized int exitStatus() {
        if (!runnerStarted) {
            return 0;
        }
        return runnerExit >= 0 ? runnerExit : Math.max(recordedExit(), 0);
    }

    private boolean exitSettled() {
        if (runnerExit >= 0) {
            return true;
        }
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            runnerExit = 0;
            return true;
        }
        int recorded = recordedExit();
        long now = System.currentTimeMillis();
        if (runnerGoneAt == 0) {
            runnerGoneAt = now;
        }
        if (recorded < 0 && now - runnerGoneAt < EXIT_RECORD_MILLIS) {
            return false;
        }
        runnerExit = Math.max(recorded, 0);
        return true;
    }

    /** The system's record of how the runner ended, or -1 with none yet. */
    private int recordedExit() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            return -1;
        }
        ActivityManager manager = (ActivityManager) shell.getSystemService(Context.ACTIVITY_SERVICE);
        String name = shell.getPackageName() + RetroActivity.PROCESS_SUFFIX;
        List<ApplicationExitInfo> records =
                manager.getHistoricalProcessExitReasons(shell.getPackageName(), 0, EXIT_RECORDS);
        for (ApplicationExitInfo record : records) {
            if (name.equals(record.getProcessName()) && record.getTimestamp() >= startedAt) {
                return statusOf(record);
            }
        }
        return -1;
    }

    private static int statusOf(ApplicationExitInfo record) {
        switch (record.getReason()) {
            case ApplicationExitInfo.REASON_CRASH:
            case ApplicationExitInfo.REASON_CRASH_NATIVE:
            case ApplicationExitInfo.REASON_ANR:
                return SIGNALLED + crashSignal(record.getStatus());
            case ApplicationExitInfo.REASON_SIGNALED:
                return SIGNALLED + record.getStatus();
            default:
                return 0;
        }
    }

    // The system kills a crashed process itself, and a Java crash has no signal of its own.
    private static int crashSignal(int status) {
        boolean ownSignal = status > 0 && status != OsConstants.SIGKILL
                && status != OsConstants.SIGTERM;
        return ownSignal ? status : OsConstants.SIGABRT;
    }

    /** Brings the runner back to the front, right over the shell; false when there is none. */
    synchronized boolean resume() {
        return runnerStarted && runnerPid() != 0 && host.resume();
    }

    /** Brings the shell back as a game ends behind it or in front. */
    synchronized void showShell() {
        host.showShell();
    }

    synchronized void stop() {
        if (runnerStarted) {
            host.stop();
        }
        int pid = runnerStarted ? runnerPid() : 0;
        if (pid != 0) {
            Process.killProcess(pid);
            while (runnerPid() != 0) {
                try {
                    wait(50);
                } catch (InterruptedException interrupted) {
                    Thread.currentThread().interrupt();
                    return;
                }
            }
        }
        if (runnerStarted) {
            host.ended();
        }
        foreignOver = true;
    }

    // Another app's game is over when the shell comes back to the front.
    synchronized void shellPaused() {
        if (foreign) {
            foreignShown = true;
        }
    }

    synchronized void shellResumed() {
        if (foreign && foreignShown) {
            foreignOver = true;
        }
    }

    private int runnerPid() {
        ActivityManager manager = (ActivityManager) shell.getSystemService(Context.ACTIVITY_SERVICE);
        List<ActivityManager.RunningAppProcessInfo> processes = manager.getRunningAppProcesses();
        String name = shell.getPackageName() + RetroActivity.PROCESS_SUFFIX;
        if (processes != null) {
            for (ActivityManager.RunningAppProcessInfo process : processes) {
                if (name.equals(process.processName)) {
                    return process.pid;
                }
            }
        }
        return 0;
    }

    /** -a action, -d data, -t type, -c category, --es/--ei/--ez key value. */
    private static void applyOptions(Intent intent, String program, String[] args) {
        int slash = program.indexOf('/');
        if (slash > 0) {
            String pkg = program.substring(0, slash);
            String cls = program.substring(slash + 1);
            intent.setComponent(new ComponentName(pkg, cls.startsWith(".") ? pkg + cls : cls));
        } else {
            intent.setPackage(program);
        }
        Uri data = null;
        String type = null;
        List<Uri> granted = new ArrayList<>();
        for (int i = 0; i < args.length; ++i) {
            String option = args[i];
            switch (option) {
                case "-a":
                    intent.setAction(value(args, ++i, option));
                    break;
                case "-d":
                    data = Uri.parse(value(args, ++i, option));
                    break;
                case "-t":
                    type = value(args, ++i, option);
                    break;
                case "-c":
                    intent.addCategory(value(args, ++i, option));
                    break;
                case "--es": {
                    String key = value(args, ++i, option);
                    String text = value(args, ++i, option);
                    intent.putExtra(key, text);
                    if (DocumentTree.isDocument(text)) {
                        granted.add(Uri.parse(text));
                    }
                    break;
                }
                case "--ei":
                    intent.putExtra(value(args, ++i, option), Integer.parseInt(value(args, ++i, option)));
                    break;
                case "--ez":
                    intent.putExtra(value(args, ++i, option), Boolean.parseBoolean(value(args, ++i, option)));
                    break;
                default:
                    throw new IllegalArgumentException("unknown intent option " + option);
            }
        }
        if (data != null || type != null) {
            intent.setDataAndType(data, type);
        }
        if (data != null && DocumentTree.isDocument(data.toString())) {
            granted.add(data);
        }
        // A game in a picked folder is lent to the app: data, or extras through the clip.
        if (!granted.isEmpty()) {
            ClipData clip = ClipData.newRawUri(null, granted.get(0));
            for (Uri uri : granted.subList(1, granted.size())) {
                clip.addItem(new ClipData.Item(uri));
            }
            intent.setClipData(clip);
            intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        }
    }

    private static String value(String[] args, int index, String option) {
        if (index >= args.length) {
            throw new IllegalArgumentException(option + " needs a value");
        }
        return args[index];
    }
}
