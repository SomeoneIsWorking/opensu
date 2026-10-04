// Package config is the only place the environment is read. It turns the
// process environment once into a typed, immutable configuration that every
// other subsystem receives.
package config

import (
	"os"
	"path/filepath"
	"strconv"
	"strings"
)

// Config is the immutable configuration for one iideck run.
type Config struct {
	// SteamRoots are explicit Steam install roots. When empty each Steam
	// source discovers the standard locations itself.
	SteamRoots []string
	// ROMRoots are directories scanned for emulator ROMs.
	ROMRoots []string
	// EmulatorCommands maps an uppercase system name to the command that runs
	// it, as "program|arg|arg" entries so the whole mapping can be carried in
	// one environment variable.
	EmulatorCommands map[string][]string
	// EmulatorExtensions maps a file extension to a system name.
	EmulatorExtensions map[string]string

	Width      int
	Height     int
	Fullscreen bool
	// Chrome leaves the window decorated. The shell draws its own hints, so the
	// default is a frameless window.
	Chrome bool
	Debug  bool
}

// Default geometry is the Steam Deck's 1280x800 panel.
const (
	DefaultWidth  = 1280
	DefaultHeight = 800
)

// defaultExtensions maps ROM extensions to system names. This is deliberately a
// small built-in set: the ES-DE source replaces it wholesale when the user has
// a real systems configuration.
var defaultExtensions = map[string]string{
	".nes": "NES", ".sfc": "SNES", ".smc": "SNES", ".md": "Genesis",
	".gen": "Genesis", ".gba": "Game Boy Advance", ".gb": "Game Boy",
	".gbc": "Game Boy Color", ".ps1": "PlayStation", ".pbp": "PlayStation",
	".cue": "PlayStation", ".chd": "PlayStation", ".iso": "PlayStation",
	".bin": "PlayStation", ".z64": "Nintendo 64", ".n64": "Nintendo 64",
	".v64": "Nintendo 64", ".smd": "Master System", ".gg": "Game Gear",
	".g64": "Game Gear", ".gba.zip": "Game Boy Advance",
}

// Load reads the environment into the run configuration.
func Load() Config {
	cfg := Config{
		SteamRoots:         splitList(os.Getenv("IIDECK_STEAM_ROOTS")),
		ROMRoots:           splitList(os.Getenv("IIDECK_ROM_ROOTS")),
		EmulatorCommands:   parseCommands(os.Getenv("IIDECK_EMULATORS")),
		EmulatorExtensions: defaultExtensions,
		Width:              intEnv("IIDECK_WIDTH", DefaultWidth),
		Height:             intEnv("IIDECK_HEIGHT", DefaultHeight),
		Fullscreen:         boolEnv("IIDECK_FULLSCREEN", true),
		Chrome:             boolEnv("IIDECK_CHROME", false),
		Debug:              boolEnv("IIDECK_DEBUG", false),
	}
	if cfg.Width <= 0 {
		cfg.Width = DefaultWidth
	}
	if cfg.Height <= 0 {
		cfg.Height = DefaultHeight
	}
	return cfg
}

// splitList splits a colon separated path list, dropping empty entries.
func splitList(value string) []string {
	if value == "" {
		return nil
	}
	parts := strings.Split(value, string(os.PathListSeparator))
	out := make([]string, 0, len(parts))
	for _, p := range parts {
		if p = strings.TrimSpace(p); p != "" {
			out = append(out, p)
		}
	}
	return out
}

// parseCommands reads emulator mappings written as
// "SYSTEM=program|arg|arg;SYSTEM2=program". Splitting on '=' and '|' keeps a
// command's own arguments unambiguous.
func parseCommands(value string) map[string][]string {
	if value == "" {
		return map[string][]string{}
	}
	out := map[string][]string{}
	for _, entry := range strings.Split(value, ";") {
		entry = strings.TrimSpace(entry)
		if entry == "" {
			continue
		}
		name, command, found := strings.Cut(entry, "=")
		if !found {
			continue
		}
		fields := strings.Fields(command)
		if len(fields) == 0 {
			continue
		}
		out[strings.ToUpper(strings.TrimSpace(name))] = fields
	}
	return out
}

func intEnv(name string, fallback int) int {
	raw := os.Getenv(name)
	if raw == "" {
		return fallback
	}
	n, err := strconv.Atoi(raw)
	if err != nil {
		return fallback
	}
	return n
}

func boolEnv(name string, fallback bool) bool {
	raw := os.Getenv(name)
	if raw == "" {
		return fallback
	}
	b, err := strconv.ParseBool(raw)
	if err != nil {
		return fallback
	}
	return b
}

// ExpandPath resolves a configured root for display and for tests that run from
// a temporary directory.
func ExpandPath(path string) string {
	if strings.HasPrefix(path, "~/") {
		if home, err := os.UserHomeDir(); err == nil {
			return filepath.Join(home, path[2:])
		}
	}
	return path
}
