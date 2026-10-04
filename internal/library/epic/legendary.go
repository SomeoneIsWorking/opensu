// Package epic lists Epic Games Store titles from the local Legendary
// installation. Legendary owns authentication and downloads; this package only
// reads what it has already installed so the grid can show those titles and
// launch them the same way.
package epic

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"os/exec"
	"path/filepath"
	"strings"

	"iideck/internal/library"
)

// Client reads the local Legendary installation.
type Client struct {
	// binary is the legendary executable, resolved from PATH when empty.
	binary string
}

// Discover returns a client using the legendary on PATH.
func Discover() *Client { return &Client{} }

// Name identifies this source to the catalog.
func (c *Client) Name() library.Source { return library.SourceEpic }

// ErrNoClient reports that Legendary is not installed or not logged in, which is
// a normal state: the catalog shows no Epic titles until it is.
var ErrNoClient = errors.New("legendary is not ready")

// Games lists installed Epic titles.
func (c *Client) Games(ctx context.Context) ([]*library.Game, error) {
	program := c.program()
	if _, err := exec.LookPath(program); err != nil {
		return nil, ErrNoClient
	}
	// --output json makes Legendary print the install list as one document
	// rather than a table. Legendary logs to stderr, so only stdout is parsed.
	out, err := exec.CommandContext(ctx, program, "list", "--output", "json").Output()
	if err != nil {
		// Legendary exits non-zero when it has no saved credentials, which means
		// there is nothing to list rather than a failure to report.
		var exitErr *exec.ExitError
		if errors.As(err, &exitErr) {
			return nil, ErrNoClient
		}
		return nil, fmt.Errorf("legendary list: %w", err)
	}
	return parse(out)
}

func (c *Client) program() string {
	if c.binary != "" {
		return c.binary
	}
	return "legendary"
}

// install is one entry of Legendary's install list.
type install struct {
	AppName     string `json:"app_name"`
	Title       string `json:"title"`
	Installed   bool   `json:"installed"`
	InstallPath string `json:"install_path"`
	IsDLC       bool   `json:"is_dlc"`
}

// parse reads Legendary's JSON install list.
func parse(data []byte) ([]*library.Game, error) {
	// Legendary has emitted both a bare array and an object keyed by app name,
	// depending on version, so both shapes are accepted.
	var entries []install
	if err := json.Unmarshal(data, &entries); err != nil {
		var asMap map[string]install
		if mapErr := json.Unmarshal(data, &asMap); mapErr != nil {
			return nil, fmt.Errorf("legendary list: %w", err)
		}
		for name, entry := range asMap {
			if entry.AppName == "" {
				entry.AppName = name
			}
			entries = append(entries, entry)
		}
	}

	var games []*library.Game
	for _, entry := range entries {
		// DLC is not a tile of its own; it belongs to its base game.
		if entry.IsDLC {
			continue
		}
		title := strings.TrimSpace(entry.Title)
		if entry.AppName == "" {
			continue
		}
		if title == "" {
			title = entry.AppName
		}
		game := &library.Game{
			ID:        string(library.SourceEpic) + ":" + entry.AppName,
			Source:    library.SourceEpic,
			SourceID:  entry.AppName,
			Title:     title,
			Installed: entry.Installed,
			Launch: library.LaunchSpec{
				Program: "legendary",
				Args:    []string{"launch", entry.AppName},
			},
		}
		if entry.InstallPath != "" {
			// The Wine prefix directory carries the app name, so a running game
			// is recognisable in /proc.
			game.ProcessHint = filepath.Base(entry.InstallPath)
		}
		games = append(games, game)
	}
	return games, nil
}
