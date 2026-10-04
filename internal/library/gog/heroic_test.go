package gog

import (
	"os"
	"path/filepath"
	"testing"
)

func writeLibrary(t *testing.T, dir, body string) {
	t.Helper()
	cache := filepath.Join(dir, "store_cache")
	if err := os.MkdirAll(cache, 0o755); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(cache, "gog_library.json"), []byte(body), 0o644); err != nil {
		t.Fatal(err)
	}
}

func TestMissingHeroicIsNotAnError(t *testing.T) {
	c := New(filepath.Join(t.TempDir(), "absent"))
	if _, err := c.Games(t.Context()); err == nil {
		t.Error("a missing Heroic configuration should report ErrNoClient")
	}
}

func TestEmptyLibraryYieldsNoGames(t *testing.T) {
	dir := t.TempDir()
	writeLibrary(t, dir, `{}`)

	games, err := New(dir).Games(t.Context())
	if err != nil {
		t.Fatalf("Games: %v", err)
	}
	if len(games) != 0 {
		t.Errorf("got %d games from an empty library, want 0", len(games))
	}
}

func TestNoSavedLibraryIsNotAnError(t *testing.T) {
	dir := t.TempDir()
	if err := os.MkdirAll(filepath.Join(dir, "store_cache"), 0o755); err != nil {
		t.Fatal(err)
	}
	games, err := New(dir).Games(t.Context())
	if err != nil {
		t.Fatalf("a Heroic with no saved library should not error, got %v", err)
	}
	if len(games) != 0 {
		t.Errorf("got %d games, want 0", len(games))
	}
}

func TestNestedLibraryFieldsWin(t *testing.T) {
	dir := t.TempDir()
	writeLibrary(t, dir, `[
		{
			"app_name": "outer_name",
			"title": "Outer Title",
			"library": { "appName": "inner_name", "title": "Inner Title" }
		}
	]`)
	games, err := New(dir).Games(t.Context())
	if err != nil {
		t.Fatalf("Games: %v", err)
	}
	if len(games) != 1 {
		t.Fatalf("got %d games, want 1", len(games))
	}
	// Heroic displays the nested values, so those are what the tile shows.
	if games[0].Title != "Inner Title" {
		t.Errorf("title = %q, want Inner Title", games[0].Title)
	}
	if games[0].SourceID != "inner_name" {
		t.Errorf("sourceId = %q, want inner_name", games[0].SourceID)
	}
}

func TestOuterFieldsAreUsedWhenLibraryIsEmpty(t *testing.T) {
	dir := t.TempDir()
	writeLibrary(t, dir, `[
		{ "app_name": "outer_name", "title": "Outer Title", "library": {} }
	]`)
	games, err := New(dir).Games(t.Context())
	if err != nil {
		t.Fatalf("Games: %v", err)
	}
	if games[0].Title != "Outer Title" || games[0].SourceID != "outer_name" {
		t.Errorf("got %+v, want the outer title and app name", games[0])
	}
}

func TestInstallStateAndProcessHint(t *testing.T) {
	dir := t.TempDir()
	writeLibrary(t, dir, `[
		{
			"library": { "appName": "installed_game", "title": "Installed" },
			"install": { "path": "/games/Heroic/Games/Installed", "installed": true }
		},
		{ "library": { "appName": "not_downloaded", "title": "Not Downloaded" } }
	]`)
	games, err := New(dir).Games(t.Context())
	if err != nil {
		t.Fatalf("Games: %v", err)
	}
	byID := map[string]string{}
	for _, g := range games {
		byID[g.SourceID] = g.ID
	}
	if len(games) != 2 {
		t.Fatalf("got %d games, want 2", len(games))
	}
	// A title with an install record is launchable and detectable in /proc.
	for _, g := range games {
		if g.SourceID == "installed_game" {
			if !g.Installed {
				t.Error("installed_game should be marked installed")
			}
			if g.ProcessHint != "Installed" {
				t.Errorf("processHint = %q, want the install folder name", g.ProcessHint)
			}
		}
		if g.SourceID == "not_downloaded" && g.Installed {
			t.Error("a title with no install record must not be marked installed")
		}
	}
}

func TestEntriesWithoutIdentityAreSkipped(t *testing.T) {
	dir := t.TempDir()
	writeLibrary(t, dir, `[ { "library": { "title": "" } }, { "library": {} } ]`)
	games, err := New(dir).Games(t.Context())
	if err != nil {
		t.Fatalf("Games: %v", err)
	}
	if len(games) != 0 {
		t.Errorf("got %d games, want 0 for entries with no identity", len(games))
	}
}

func TestMalformedLibraryIsReported(t *testing.T) {
	dir := t.TempDir()
	writeLibrary(t, dir, `not json`)
	if _, err := New(dir).Games(t.Context()); err == nil {
		t.Error("a malformed library should report an error rather than an empty grid")
	}
}
