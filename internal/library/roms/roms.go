// Package roms scans directories of emulator ROMs and resolves a per-system
// emulator command for each one.
package roms

import (
	"context"
	"errors"
	"fmt"
	"io/fs"
	"os"
	"path/filepath"
	"sort"
	"strings"

	"iideck/internal/config"
	"iideck/internal/library"
)

// Source scans configured ROM directories.
type Source struct {
	roots      []string
	extensions map[string]string
	emulators  map[string][]string
}

// Discover builds a source from the run configuration. With no ROM roots
// configured there is nothing to scan, and Name still reports Emulator so the
// catalog can include it.
func Discover(cfg config.Config) *Source {
	roots := make([]string, 0, len(cfg.ROMRoots))
	for _, root := range cfg.ROMRoots {
		roots = append(roots, config.ExpandPath(root))
	}
	return &Source{
		roots:      roots,
		extensions: cfg.EmulatorExtensions,
		emulators:  cfg.EmulatorCommands,
	}
}

// Name identifies this source to the catalog.
func (s *Source) Name() library.Source { return library.SourceROM }

// Games lists every ROM found under the configured roots.
func (s *Source) Games(ctx context.Context) ([]*library.Game, error) {
	if len(s.roots) == 0 {
		return nil, nil
	}
	var (
		games []*library.Game
		errs  []string
	)
	for _, root := range s.roots {
		if err := ctx.Err(); err != nil {
			return games, err
		}
		found, err := s.scanRoot(ctx, root)
		if err != nil {
			errs = append(errs, fmt.Sprintf("%s: %v", root, err))
			continue
		}
		games = append(games, found...)
	}
	if len(games) == 0 && len(errs) > 0 {
		return nil, errors.New(strings.Join(errs, "; "))
	}
	return games, nil
}

// scanRoot walks one ROM directory. Subdirectories are not descended into: an
// emulator library is conventionally flat, and a ROM collection with thousands of
// nested directories is usually a mistake rather than a layout.
func (s *Source) scanRoot(ctx context.Context, root string) ([]*library.Game, error) {
	entries, err := os.ReadDir(root)
	if err != nil {
		return nil, err
	}
	var games []*library.Game
	for _, entry := range entries {
		if err := ctx.Err(); err != nil {
			return games, err
		}
		if entry.IsDir() || !s.isROM(entry.Name()) {
			continue
		}
		path := filepath.Join(root, entry.Name())
		info, err := entry.Info()
		if err != nil {
			continue
		}
		games = append(games, s.game(path, entry.Name(), info))
	}
	return games, nil
}

// isROM reports whether a filename has an extension the configuration maps to a
// system. Comparison is case-insensitive because collections mix cases freely.
func (s *Source) isROM(name string) bool {
	ext := strings.ToLower(filepath.Ext(name))
	if ext == "" {
		return false
	}
	_, ok := s.extensions[ext]
	return ok
}

// game builds one catalog entry. The title is the filename without its extension,
// which is the only name a loose ROM file carries.
func (s *Source) game(path, name string, info fs.FileInfo) *library.Game {
	system := s.system(name)
	title := strings.TrimSuffix(name, filepath.Ext(name))
	game := &library.Game{
		ID:        string(library.SourceROM) + ":" + path,
		Source:    library.SourceROM,
		SourceID:  system,
		Title:     title,
		Installed: true,
		// The file itself is what makes the entry launchable.
		ProcessHint: name,
		Launch:      s.launch(system, path),
	}
	if !info.ModTime().IsZero() {
		modified := info.ModTime().UTC()
		game.LastPlayed = &modified
	}
	return game
}

// system is the uppercase system name for a ROM's extension.
func (s *Source) system(name string) string {
	ext := strings.ToLower(filepath.Ext(name))
	if system, ok := s.extensions[ext]; ok {
		return system
	}
	return "Unknown"
}

// launch resolves the command for a system, appending the ROM path. A system
// with no configured command yields an empty spec, which the shell reports rather
// than launching nothing silently.
func (s *Source) launch(system, romPath string) library.LaunchSpec {
	fields, ok := s.emulators[strings.ToUpper(system)]
	if !ok || len(fields) == 0 {
		return library.LaunchSpec{}
	}
	args := make([]string, 0, len(fields))
	args = append(args, fields[1:]...)
	args = append(args, romPath)
	return library.LaunchSpec{Program: fields[0], Args: args}
}

// Roots reports the configured ROM directories.
func (s *Source) Roots() []string { return append([]string(nil), s.roots...) }

// Systems reports the systems the source can launch, sorted, for diagnostics and
// for telling the user which emulators are still unconfigured.
func (s *Source) Systems() []string {
	seen := map[string]struct{}{}
	var out []string
	for _, system := range s.extensions {
		if _, ok := seen[system]; ok {
			continue
		}
		seen[system] = struct{}{}
		out = append(out, system)
	}
	sort.Strings(out)
	return out
}
