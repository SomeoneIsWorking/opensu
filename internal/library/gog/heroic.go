// Package gog lists GOG titles from the local Heroic Games Launcher
// installation. Heroic owns authentication, downloads and cloud sync; this
// package only reads its saved library and hands launches back to it.
package gog

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"strings"

	"iideck/internal/library"
)

// Client reads the local Heroic installation.
type Client struct {
	// configDir is Heroic's configuration directory, resolved from HOME when
	// empty.
	configDir string
	// binary is the heroic executable, resolved from PATH when empty.
	binary string
}

// Discover returns a client using Heroic's default locations.
func Discover() *Client { return &Client{} }

// New returns a client for an explicit configuration directory, which the tests
// use and a relocated installation needs.
func New(configDir string) *Client { return &Client{configDir: configDir} }

// Name identifies this source to the catalog.
func (c *Client) Name() library.Source { return library.SourceGOG }

// ErrNoClient reports that Heroic is not installed, which is a normal state: the
// catalog shows no GOG titles until it is.
var ErrNoClient = errors.New("heroic is not installed")

// Games lists GOG titles from Heroic's saved library.
func (c *Client) Games(ctx context.Context) ([]*library.Game, error) {
	if err := ctx.Err(); err != nil {
		return nil, err
	}
	if !c.installed() {
		return nil, ErrNoClient
	}
	// Heroic caches its library under store_cache as a plain JSON document.
	// An empty object means no library saved yet, which is not a failure.
	path := filepath.Join(c.dir(), "store_cache", "gog_library.json")
	data, err := os.ReadFile(path)
	if err != nil {
		if errors.Is(err, os.ErrNotExist) {
			return nil, nil
		}
		return nil, fmt.Errorf("heroic library: %w", err)
	}
	games, err := c.parse(data)
	if err != nil {
		return nil, err
	}
	return games, nil
}

// installed reports whether Heroic's configuration directory exists.
func (c *Client) installed() bool {
	info, err := os.Stat(c.dir())
	return err == nil && info.IsDir()
}

func (c *Client) dir() string {
	if c.configDir != "" {
		return c.configDir
	}
	home, err := os.UserHomeDir()
	if err != nil {
		return ""
	}
	return filepath.Join(home, ".config", "heroic")
}

func (c *Client) program() string {
	if c.binary != "" {
		return c.binary
	}
	return "heroic"
}

// libraryEntry is one game in Heroic's saved GOG library. Heroic nests the
// display fields under "library" and keeps install state alongside.
type libraryEntry struct {
	AppName string `json:"app_name"`
	Title   string `json:"title"`
	Library struct {
		Title    string `json:"title"`
		AppName  string `json:"appName"`
		Folder   string `json:"folderName"`
		ImageURL string `json:"imageUrl"`
	} `json:"library"`
	Install *struct {
		Path      string `json:"path"`
		Installed bool   `json:"installed"`
		Version   string `json:"versionName"`
	} `json:"install"`
}

// parse reads Heroic's cached library.
func (c *Client) parse(data []byte) ([]*library.Game, error) {
	// Heroic writes "{}" for an empty library and "[]" once titles exist, so an
	// object is treated as no games rather than a malformed document.
	var entries []libraryEntry
	if err := json.Unmarshal(data, &entries); err != nil {
		var empty map[string]any
		if json.Unmarshal(data, &empty) == nil && len(empty) == 0 {
			return nil, nil
		}
		return nil, fmt.Errorf("heroic library: %w", err)
	}

	var games []*library.Game
	for _, entry := range entries {
		// Heroic repeats the title and app name inside "library"; the nested
		// values are the ones it displays, so they win when both are present.
		title := firstNonEmpty(entry.Library.Title, entry.Title)
		appName := firstNonEmpty(entry.Library.AppName, entry.AppName, title)
		if title == "" || appName == "" {
			continue
		}
		game := &library.Game{
			ID:     string(library.SourceGOG) + ":" + appName,
			Source: library.SourceGOG,
			// SourceID must match Steam's six-digit shape so the catalog can
			// recognise a GOG title that is also on Steam.
			SourceID:  appName,
			Title:     title,
			Installed: entry.Install != nil && entry.Install.Installed,
			Launch: library.LaunchSpec{
				Program: c.program(),
				Args:    []string{"util", "install", "--platform", "gog", appName},
			},
		}
		if entry.Install != nil && entry.Install.Path != "" {
			game.ProcessHint = filepath.Base(entry.Install.Path)
		}
		games = append(games, game)
	}
	return games, nil
}

func firstNonEmpty(values ...string) string {
	for _, v := range values {
		if v = strings.TrimSpace(v); v != "" {
			return v
		}
	}
	return ""
}
