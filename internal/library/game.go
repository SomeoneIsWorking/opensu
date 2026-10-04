// Package library defines the launchable catalog: everything a controller can
// pick from, independent of which backend an entry came from.
package library

import "time"

// Source identifies the backend an entry came from.
type Source string

const (
	SourceSteam Source = "steam"
	SourceEpic  Source = "epic"
	SourceGOG   Source = "gog"
	SourceROM   Source = "rom"
)

// Label is the store name shown in the shell.
func (s Source) Label() string {
	switch s {
	case SourceSteam:
		return "Steam"
	case SourceEpic:
		return "Epic"
	case SourceGOG:
		return "GOG"
	case SourceROM:
		return "Emulator"
	default:
		return string(s)
	}
}

// TileSize is how much of the grid a tile occupies on the home screen.
type TileSize string

const (
	// TileSquare is a normal library entry.
	TileSquare TileSize = "square"
	// TileWide spans two columns and one row.
	TileWide TileSize = "wide"
	// TileHero spans two columns and two rows.
	TileHero TileSize = "hero"
)

// Span reports the tile's column and row span in the grid.
func (t TileSize) Span() (cols, rows int) {
	switch t {
	case TileWide:
		return 2, 1
	case TileHero:
		return 2, 2
	default:
		return 1, 1
	}
}

// LaunchSpec is the command that starts a game. Every source fills this in, so
// starting a game needs no knowledge of which store owns it.
type LaunchSpec struct {
	Program string   `json:"program"`
	Args    []string `json:"args"`
}

// Empty reports whether there is nothing to run.
func (l LaunchSpec) Empty() bool { return l.Program == "" }

// Game is one launchable entry in the catalog.
type Game struct {
	// ID is source qualified, so two stores holding the same title never
	// collide.
	ID       string `json:"id"`
	Source   Source `json:"source"`
	SourceID string `json:"sourceId"`
	Title    string `json:"title"`

	// Installed is whether the title can be launched right now.
	Installed bool `json:"installed"`

	// ArtworkFile is where the image bytes live on disk. It never reaches the
	// shell, which reads Artwork instead.
	ArtworkFile string `json:"-"`
	// ArtworkWideFile is the landscape image on disk, if the store has one.
	ArtworkWideFile string `json:"-"`
	// Artwork is the shell URL for the portrait tile image.
	Artwork string `json:"artwork"`
	// ArtworkWide is the shell URL for a landscape image, used by wide and
	// hero tiles.
	ArtworkWide string `json:"artworkWide"`

	PlaytimeMinutes int        `json:"playtimeMinutes"`
	LastPlayed      *time.Time `json:"lastPlayed"`
	Favourite       bool       `json:"favourite"`

	// ProcessHint is a substring of the launched process's command line, used
	// to notice that the game has exited.
	ProcessHint string `json:"-"`

	Launch LaunchSpec `json:"launch"`

	// Display fields, filled by Annotate.
	Size  TileSize `json:"size"`
	Hint  string   `json:"hint"`
	Badge string   `json:"badge"`
}
