// Package launch starts a game and hides the shell until it exits.
package launch

import (
	"errors"
	"fmt"
	"log/slog"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"sync"
	"syscall"
	"time"

	"iideck/internal/library"
)

// Timeout bounds how long the shell waits before showing itself again, so a game
// whose process cannot be recognised cannot leave the shell hidden forever.
const Timeout = 12 * time.Hour

// pollInterval is how often /proc is consulted while a game runs.
const pollInterval = 2 * time.Second

// procDir is the process table, overridable so the detection can be exercised
// without starting a game.
var procDir = "/proc"

// Error describes a launch that cannot be attempted.
type Error struct {
	// ID is the catalog entry that was requested.
	ID string
	// Reason is why it could not start.
	Reason string
}

func (e *Error) Error() string { return fmt.Sprintf("launch %s: %s", e.ID, e.Reason) }

// Launcher starts games. It is safe for concurrent use.
type Launcher struct {
	mu      sync.Mutex
	running bool
}

// New returns a launcher.
func New() *Launcher { return &Launcher{} }

// Running reports whether a game is currently running.
func (l *Launcher) Running() bool {
	l.mu.Lock()
	defer l.mu.Unlock()
	return l.running
}

// Start runs a game, hides the window, and shows it again when the game exits.
// It blocks until then, so callers run it in their own goroutine.
func (l *Launcher) Start(game *library.Game, hide, show func()) error {
	if game.Launch.Empty() {
		return &Error{ID: game.ID, Reason: "no launch command for " + game.Title}
	}

	l.mu.Lock()
	if l.running {
		l.mu.Unlock()
		return &Error{ID: game.ID, Reason: "another game is already running"}
	}
	l.running = true
	l.mu.Unlock()

	defer func() {
		l.mu.Lock()
		l.running = false
		l.mu.Unlock()
	}()

	cmd := exec.Command(game.Launch.Program, game.Launch.Args...)
	// A game must outlive the shell: it gets its own session so a shell exit or
	// a controlling-terminal hangup cannot reach it.
	cmd.SysProcAttr = &syscall.SysProcAttr{Setsid: true}

	if err := cmd.Start(); err != nil {
		return fmt.Errorf("launch %s: %w", game.Title, err)
	}
	slog.Info("game started", "title", game.Title, "pid", cmd.Process.Pid)

	if hide != nil {
		hide()
	}
	childDone := make(chan error, 1)
	go func() { childDone <- cmd.Wait() }()

	gone := l.watchUntilGone(game, childDone)

	if show != nil {
		show()
	}
	return <-gone
}

// watchUntilGone returns a channel carrying the outcome once the game has
// finished. A game that hands off to a different process, as Steam and Legendary
// do, is followed through the process table rather than the child alone.
func (l *Launcher) watchUntilGone(game *library.Game, childDone <-chan error) <-chan error {
	done := make(chan error, 1)
	go func() {
		defer close(done)

		var childErr error
		childFinished := false
		deadline := time.After(Timeout)
		seen := false

		for {
			select {
			case childErr = <-childDone:
				childFinished = true
			case <-deadline:
				slog.Warn("timed out waiting for the game to exit", "title", game.Title)
				done <- nil
				return
			case <-time.After(pollInterval):
			}

			if childFinished {
				// A handed-off process may outlive the child, so the game counts
				// as gone only once nothing matches in /proc. One grace poll
				// catches a process that is still shutting down.
				if processMatches(game.ProcessHint) {
					seen = true
					continue
				}
				if seen {
					break
				}
				// The child exited and nothing matched, so the game is over
				// whether or not it was ever seen.
				break
			}

			if processMatches(game.ProcessHint) {
				seen = true
			} else if seen {
				break
			}
		}
		done <- childErr
	}()
	return done
}

// processMatches reports whether any running process's command line contains the
// hint, which for every source is a substring unique to that game's Wine prefix
// or install folder.
func processMatches(hint string) bool {
	if hint == "" {
		return false
	}
	entries, err := os.ReadDir(procDir)
	if err != nil {
		return false
	}
	for _, entry := range entries {
		if !entry.IsDir() {
			continue
		}
		name := entry.Name()
		if name[0] < '0' || name[0] > '9' {
			continue
		}
		cmdline, err := os.ReadFile(filepath.Join(procDir, name, "cmdline"))
		if err != nil {
			// The process exited between the readdir and the read.
			continue
		}
		if strings.Contains(strings.ReplaceAll(string(cmdline), "\x00", " "), hint) {
			return true
		}
	}
	return false
}

// ErrNotRunning reports that no game is running.
var ErrNotRunning = errors.New("no game is running")
