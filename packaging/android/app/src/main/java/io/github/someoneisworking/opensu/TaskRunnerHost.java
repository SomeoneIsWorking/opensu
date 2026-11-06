package io.github.someoneisworking.opensu;

import android.app.ActivityManager;
import android.content.ActivityNotFoundException;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;

/** The runner in {@link RetroActivity}'s own task, showing through the translucent shell. */
final class TaskRunnerHost implements RunnerHost {
    private final ShellActivity shell;

    TaskRunnerHost(ShellActivity shell) {
        this.shell = shell;
    }

    @Override
    public String start(String[] arguments, String[] environment) {
        shell.seeThrough(true);
        try {
            shell.startActivity(new Intent(shell, RetroActivity.class)
                    .putExtra(RetroActivity.EXTRA_ARGUMENTS, arguments)
                    .putExtra(RetroActivity.EXTRA_ENVIRONMENT, environment)
                    .putExtra(RetroActivity.EXTRA_SHELL, shell.runnerInbox())
                    .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK));
        } catch (ActivityNotFoundException | SecurityException refused) {
            shell.seeThrough(false);
            return refused.getMessage();
        }
        return null;
    }

    @Override
    public boolean standing() {
        return runnerTask() != null;
    }

    @Override
    public boolean resume() {
        if (runnerTask() == null) {
            return false;
        }
        // AppTask.moveToFront would put the home screen under it, where a dying game leaves it.
        shell.startActivity(new Intent(shell, RetroActivity.class)
                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_REORDER_TO_FRONT
                        | Intent.FLAG_ACTIVITY_NO_ANIMATION));
        return true;
    }

    @Override
    public void showShell() {
        shell.seeThrough(false);
        ActivityManager.AppTask task = taskOf(ShellActivity.class.getName());
        if (task != null) {
            task.moveToFront();
        }
    }

    @Override
    public void stop() {
        ActivityManager.AppTask task = runnerTask();
        if (task != null) {
            // A killed activity in the background would otherwise be restored from Recents.
            task.finishAndRemoveTask();
        }
    }

    @Override
    public void ended() {
        shell.endTurn();
    }

    private ActivityManager.AppTask runnerTask() {
        return taskOf(RetroActivity.class.getName());
    }

    private ActivityManager.AppTask taskOf(String activity) {
        ActivityManager manager = (ActivityManager) shell.getSystemService(Context.ACTIVITY_SERVICE);
        for (ActivityManager.AppTask task : manager.getAppTasks()) {
            ComponentName base = task.getTaskInfo().baseActivity;
            if (base != null && activity.equals(base.getClassName())) {
                return task;
            }
        }
        return null;
    }
}
