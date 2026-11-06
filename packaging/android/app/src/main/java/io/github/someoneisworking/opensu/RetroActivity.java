package io.github.someoneisworking.opensu;

import android.content.Intent;
import android.os.Bundle;
import android.os.Messenger;

/** openSU's libretro runner, one game per `:retro` process, as opensu-retro is on the desktop. */
public final class RetroActivity extends LandscapeActivity {
    static final String EXTRA_ARGUMENTS = "io.github.someoneisworking.opensu.ARGUMENTS";
    static final String EXTRA_ENVIRONMENT = "io.github.someoneisworking.opensu.ENVIRONMENT";
    static final String EXTRA_FROM_GAME = "io.github.someoneisworking.opensu.FROM_GAME";
    static final String EXTRA_SHELL = "io.github.someoneisworking.opensu.SHELL";
    static final String PROCESS_SUFFIX = ":retro";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        // The launch's environment, read by the runner's config before anything else.
        RunnerEnvironment.apply(getIntent().getStringArrayExtra(EXTRA_ENVIRONMENT));
        super.onCreate(savedInstanceState);
    }

    @Override
    protected String[] getLibraries() {
        return new String[] {"SDL3", "opensu-retro"};
    }

    // Runs on SDL's main thread, so a large game is copied off the UI thread.
    @Override
    protected String[] getArguments() {
        return new GameStaging(this).resolve(getIntent().getStringArrayExtra(EXTRA_ARGUMENTS));
    }

    /** Called from the runner's shell handoff; the shell turns too, to come over the game as it is. */
    @Override
    public void turnScreen(boolean portrait) {
        super.turnScreen(portrait);
        Messenger shell = getIntent().getParcelableExtra(EXTRA_SHELL);
        if (shell != null) {
            RetroService.tellTurn(shell, portrait);
        }
    }

    /** Called from the runner's shell handoff: the shell comes over the game with its Guide menu. */
    public void openShellMenu() {
        startActivity(new Intent(this, ShellActivity.class)
                .putExtra(EXTRA_FROM_GAME, true)
                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_REORDER_TO_FRONT
                        | Intent.FLAG_ACTIVITY_NO_ANIMATION));
    }

    // The process is the game's: it ends with the runner, so the next launch starts clean.
    @Override
    protected void main() {
        super.main();
        android.os.Process.killProcess(android.os.Process.myPid());
    }
}
