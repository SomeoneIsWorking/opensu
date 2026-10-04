package launch

import (
	"errors"
	"os"
	"path/filepath"
	"testing"
	"time"

	"iideck/internal/library"
)

// fakeProc builds a directory shaped like /proc containing the given command
// lines, so process detection can be exercised without starting anything.
func fakeProc(t *testing.T, cmdlines ...string) string {
	t.Helper()
	dir := t.TempDir()
	for i, cmdline := range cmdlines {
		pid := filepath.Join(dir, string(rune('1'+i))+"234")
		if err := os.MkdirAll(pid, 0o755); err != nil {
			t.Fatal(err)
		}
		// The kernel separates argv entries with NUL bytes.
		if err := os.WriteFile(filepath.Join(pid, "cmdline"), []byte(cmdline), 0o644); err != nil {
			t.Fatal(err)
		}
	}
	// Non-numeric entries such as "self" and "sys" must be skipped.
	for _, name := range []string{"self", "sys", "net"} {
		if err := os.MkdirAll(filepath.Join(dir, name), 0o755); err != nil {
			t.Fatal(err)
		}
	}
	return dir
}

func withProc(t *testing.T, dir string) {
	t.Helper()
	original := procDir
	procDir = dir
	t.Cleanup(func() { procDir = original })
}

func TestProcessMatchesFindsHintInArgv(t *testing.T) {
	// The hint is matched against the command line, so the argv separator is a
	// NUL byte. Go source cannot hold one literally, so it is spelled as \x00.
	withProc(t, fakeProc(t, "steam\x00steam://rungameid/440\x00"))

	if !processMatches("steam://rungameid/440") {
		t.Error("a hint inside an argv entry should be detected")
	}
	// Steam's handoff leaves the app id in the Wine prefix path.
	withProc(t, fakeProc(t, "/home/u/steamapps/compatdata/367520/pf/x\x00wine64-preloader"))
	if !processMatches("compatdata/367520") {
		t.Error("a prefix path containing the hint should be detected")
	}
}

func TestProcessMatchesIgnoresOtherProcesses(t *testing.T) {
	withProc(t, fakeProc(t, "firefox\x00about:blank\x00", "kworker/0:1\x00"))

	if processMatches("compatdata/440") {
		t.Error("an unrelated process must not match")
	}
}

func TestProcessMatchesWithEmptyHint(t *testing.T) {
	withProc(t, fakeProc(t, "anything"))
	if processMatches("") {
		t.Error("an empty hint must never match, or every process would")
	}
}

func TestProcessMatchesWithUnreadableProc(t *testing.T) {
	withProc(t, filepath.Join(t.TempDir(), "missing"))

	if processMatches("anything") {
		t.Error("an unreadable process table must report no match")
	}
}

func TestProcessMatchesSkipsEntriesWithoutCmdline(t *testing.T) {
	// A pid directory with no readable cmdline is a process that just exited.
	withProc(t, fakeProc(t, "steam\x00-x\x00"))
	if err := os.Remove(filepath.Join(procDir, "1234", "cmdline")); err != nil {
		t.Fatal(err)
	}
	if processMatches("steam") {
		t.Error("a pid with no readable cmdline must be skipped, not matched")
	}
}

func TestStartRejectsEmptyLaunchSpec(t *testing.T) {
	l := New()
	err := l.Start(&library.Game{ID: "rom:/x.gb", Title: "No Emulator"}, nil, nil)
	if err == nil {
		t.Fatal("a game with no launch command should be refused")
	}
	var launchErr *Error
	if !errors.As(err, &launchErr) {
		t.Errorf("err = %T %v, want *Error", err, err)
	}
	if l.Running() {
		t.Error("a refused launch must not mark a game as running")
	}
}

func TestStartReportsMissingProgram(t *testing.T) {
	l := New()
	game := &library.Game{
		ID:     "steam:1",
		Title:  "Absent",
		Launch: library.LaunchSpec{Program: "definitely-not-a-real-program-xyz"},
	}
	if err := l.Start(game, nil, nil); err == nil {
		t.Error("launching a missing program should report an error")
	}
	if l.Running() {
		t.Error("a failed start must not leave the launcher marked running")
	}
}

func TestStartHidesAndShowsAroundAShortGame(t *testing.T) {
	withProc(t, t.TempDir())

	var hidden, shown int
	done := make(chan error, 1)
	go func() {
		done <- New().Start(&library.Game{
			ID:        "rom:/x.gb",
			Title:     "Short Game",
			Launch:    library.LaunchSpec{Program: "/bin/sleep", Args: []string{"0"}},
			Installed: true,
		}, func() { hidden++ }, func() { shown++ })
	}()

	select {
	case err := <-done:
		if err != nil {
			t.Errorf("Start = %v, want nil for a program that exits cleanly", err)
		}
	case <-time.After(10 * time.Second):
		t.Fatal("Start did not return for a short-lived program")
	}

	if hidden != 1 {
		t.Errorf("hide called %d times, want 1", hidden)
	}
	if shown != 1 {
		t.Errorf("show called %d times, want 1", shown)
	}
}

func TestRunningRejectsASecondGame(t *testing.T) {
	withProc(t, t.TempDir())
	l := New()

	first := make(chan error, 1)
	go func() {
		first <- l.Start(&library.Game{
			ID:     "rom:/a.gb",
			Title:  "First",
			Launch: library.LaunchSpec{Program: "/bin/sleep", Args: []string{"3"}},
		}, nil, nil)
	}()

	// Wait until the first game is marked running.
	deadline := time.Now().Add(5 * time.Second)
	for !l.Running() && time.Now().Before(deadline) {
		time.Sleep(10 * time.Millisecond)
	}
	if !l.Running() {
		t.Fatal("the first game should be marked running")
	}

	err := l.Start(&library.Game{
		ID:     "rom:/b.gb",
		Title:  "Second",
		Launch: library.LaunchSpec{Program: "/bin/sleep", Args: []string{"0"}},
	}, nil, nil)
	if err == nil {
		t.Error("a second launch should be refused while a game is running")
	}

	select {
	case <-first:
	case <-time.After(15 * time.Second):
		t.Fatal("the first game did not finish")
	}
}
