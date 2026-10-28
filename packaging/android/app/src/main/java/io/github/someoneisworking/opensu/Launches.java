package io.github.someoneisworking.opensu;

import android.app.Activity;
import android.app.ActivityManager;
import android.content.ActivityNotFoundException;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Process;

import java.util.List;

/**
 * The launches the shell starts: openSU's runner in the `:retro` process, or another app by an
 * explicit intent. A program ending in {@link #RUNNER_LIBRARY} is the runner; anything else is a
 * component, `package/class` or a bare package, with am-start style options for its intent.
 */
final class Launches {
    static final String RUNNER_LIBRARY = "/libopensu-retro.so";
    private static final long FOREIGN_START_MILLIS = 10_000;

    private final Activity shell;
    private boolean runnerStarted;
    private boolean foreign;
    private boolean foreignShown;
    private boolean foreignOver;
    private long foreignStartedAt;

    Launches(Activity shell) {
        this.shell = shell;
    }

    synchronized String start(String program, String[] args, String[] environment) {
        runnerStarted = false;
        foreign = false;
        Intent intent;
        if (program.endsWith(RUNNER_LIBRARY)) {
            intent = new Intent(shell, RetroActivity.class)
                    .putExtra(RetroActivity.EXTRA_ARGUMENTS, args)
                    .putExtra(RetroActivity.EXTRA_ENVIRONMENT, environment);
        } else {
            String failure = null;
            intent = new Intent();
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
        }
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        try {
            shell.startActivity(intent);
        } catch (ActivityNotFoundException | SecurityException refused) {
            return program + " cannot be opened: " + refused.getMessage();
        }
        if (program.endsWith(RUNNER_LIBRARY)) {
            runnerStarted = true;
        } else {
            foreign = true;
            foreignShown = false;
            foreignOver = false;
            foreignStartedAt = System.currentTimeMillis();
        }
        return null;
    }

    synchronized boolean running() {
        if (runnerStarted) {
            return runnerPid() != 0;
        }
        if (foreign && !foreignShown
                && System.currentTimeMillis() - foreignStartedAt > FOREIGN_START_MILLIS) {
            // The app never took the screen from the shell.
            foreignOver = true;
        }
        return foreign && !foreignOver;
    }

    synchronized void stop() {
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
                case "--es":
                    intent.putExtra(value(args, ++i, option), value(args, ++i, option));
                    break;
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
        if (data != null && "content".equals(data.getScheme())) {
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
