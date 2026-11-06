package io.github.someoneisworking.opensu;

/** Where the runner's `:retro` process shows a game: its own task, or the shell's window. */
interface RunnerHost {
    /** Starts the runner; null once started, else why not. */
    String start(String[] arguments, String[] environment);

    /** Whether the host stands while the process forks; it goes when the process dies. */
    boolean standing();

    /** Brings the game back in front of the shell; false when there is none to bring. */
    boolean resume();

    /** Brings the shell back as the game ends behind it or in front. */
    void showShell();

    /** Takes down what the host shows, before the process is killed. */
    void stop();

    /** The process is gone. */
    void ended();
}
