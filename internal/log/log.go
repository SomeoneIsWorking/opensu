// Package log installs the single process logger. Product code logs through
// slog and nothing writes to stdout or stderr directly.
package log

import (
	"log/slog"
	"os"
)

// Setup makes the process logger the default at the requested debug level.
func Setup(debug bool) {
	level := slog.LevelInfo
	if debug {
		level = slog.LevelDebug
	}
	slog.SetDefault(slog.New(slog.NewTextHandler(os.Stderr, &slog.HandlerOptions{Level: level})))
}
