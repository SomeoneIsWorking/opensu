package library

import (
	"context"
	"fmt"
	"sort"
	"strings"
	"time"
)

// Display thresholds, tuned against the reference layout: a game touched this
// week is something to resume, anything older is something you came back to.
const (
	ResumeWindow = 7 * 24 * time.Hour
	LingerWindow = 90 * 24 * time.Hour
)

// Provider is a backend that can list launchable games. Each store has its own
// package; the catalog only knows this aggregate shape.
type Provider interface {
	// Name is the store the provider represents.
	Name() Source
	// Games lists what the provider can launch.
	Games(ctx context.Context) ([]*Game, error)
}

// Catalog merges every configured source into the single grid the home screen
// shows.
type Catalog struct {
	sources []Provider
}

// NewCatalog builds a catalog over the given sources.
func NewCatalog(sources ...Provider) *Catalog {
	return &Catalog{sources: sources}
}

// Refresh reads every source and returns the merged, ordered catalog. A source
// that fails is reported in the returned error while the remaining sources
// still contribute, so one broken store cannot empty the grid.
func (c *Catalog) Refresh(ctx context.Context) ([]*Game, error) {
	var (
		all   []*Game
		fails []string
	)
	for _, src := range c.sources {
		games, err := src.Games(ctx)
		if err != nil {
			fails = append(fails, fmt.Sprintf("%s: %v", src.Name().Label(), err))
			continue
		}
		all = append(all, games...)
	}
	all = dedupe(all)
	Order(all)
	Annotate(all)
	PublishArtwork(all)
	if len(fails) > 0 {
		return all, fmt.Errorf("library: %s", strings.Join(fails, "; "))
	}
	return all, nil
}

// Order sorts the catalog the way the home screen reads: installed before
// uninstalled, most recently played first, then alphabetically so two runs
// produce the same grid.
func Order(games []*Game) {
	sort.SliceStable(games, func(i, j int) bool {
		a, b := games[i], games[j]
		if a.Installed != b.Installed {
			return a.Installed
		}
		at, bt := lastPlayed(a), lastPlayed(b)
		if !at.Equal(bt) {
			return at.After(bt)
		}
		return a.Title < b.Title
	})
}

// Annotate fills the display-only fields so the shell renders strings it was
// given instead of inventing them, and the home screen gets its mix of tile
// sizes from one place.
func Annotate(games []*Game) {
	resumed := 0
	lingering := 0
	now := time.Now()
	for _, g := range games {
		switch {
		case !g.Installed:
			g.Size = TileSquare
			g.Badge = "Not installed"
		case g.LastPlayed == nil:
			g.Size = TileSquare
			g.Hint = "Ready to play"
		case now.Sub(*g.LastPlayed) <= ResumeWindow:
			g.Hint = "Jump back in!"
			sizeFeature(&resumed, g)
		case now.Sub(*g.LastPlayed) <= LingerWindow:
			g.Hint = "Been a while…"
			sizeFeature(&lingering, g)
		default:
			g.Size = TileSquare
			g.Hint = "Been a while…"
		}
		if g.Source != SourceSteam {
			g.Badge = g.Source.Label()
		}
	}
}

// sizeFeature promotes the games worth featuring: the first one back gets the
// hero tile and the next few get wide tiles, then everything is square.
func sizeFeature(count *int, g *Game) {
	switch *count {
	case 0:
		g.Size = TileHero
	case 1, 2:
		g.Size = TileWide
	default:
		g.Size = TileSquare
	}
	*count++
}

// dedupe drops repeated ids, keeping the first occurrence so a source listed
// twice cannot produce two tiles for one game.
func dedupe(games []*Game) []*Game {
	seen := make(map[string]struct{}, len(games))
	out := games[:0]
	for _, g := range games {
		if g.ID == "" {
			continue
		}
		if _, ok := seen[g.ID]; ok {
			continue
		}
		seen[g.ID] = struct{}{}
		out = append(out, g)
	}
	return out
}

// PublishArtwork turns each entry's on-disk artwork path into the shell URL
// that serves it.
func PublishArtwork(games []*Game) {
	for _, g := range games {
		if g.ArtworkFile != "" {
			g.Artwork = "/artwork/" + g.ID
		}
		if g.ArtworkWideFile != "" {
			g.ArtworkWide = "/artwork-wide/" + g.ID
		}
	}
}

func lastPlayed(g *Game) time.Time {
	if g.LastPlayed == nil {
		return time.Time{}
	}
	return *g.LastPlayed
}
