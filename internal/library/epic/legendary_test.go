package epic

import "testing"

func TestParseArrayShape(t *testing.T) {
	games, err := parse([]byte(`[
		{ "app_name": "Fortnite", "title": "Fortnite", "installed": true,
		  "install_path": "/home/u/Games/Heroic/Games/Fortnite" }
	]`))
	if err != nil {
		t.Fatalf("parse: %v", err)
	}
	if len(games) != 1 {
		t.Fatalf("got %d games, want 1", len(games))
	}
	g := games[0]
	if g.ID != "epic:Fortnite" {
		t.Errorf("id = %q", g.ID)
	}
	if !g.Installed {
		t.Error("should be installed")
	}
	if g.Launch.Program != "legendary" || g.Launch.Args[0] != "launch" || g.Launch.Args[1] != "Fortnite" {
		t.Errorf("launch = %+v", g.Launch)
	}
	if g.ProcessHint != "Fortnite" {
		t.Errorf("processHint = %q, want the install folder name", g.ProcessHint)
	}
}

func TestParseMapShape(t *testing.T) {
	// Older Legendary emits an object keyed by app name.
	games, err := parse([]byte(`{
		"Portal": { "title": "Portal", "installed": false }
	}`))
	if err != nil {
		t.Fatalf("parse: %v", err)
	}
	if len(games) != 1 {
		t.Fatalf("got %d games, want 1", len(games))
	}
	if games[0].SourceID != "Portal" {
		t.Errorf("sourceId = %q, want the map key", games[0].SourceID)
	}
	if games[0].Title != "Portal" {
		t.Errorf("title = %q", games[0].Title)
	}
}

func TestDLCCodesAreNotTiles(t *testing.T) {
	games, err := parse([]byte(`[
		{ "app_name": "Base", "title": "Base Game", "installed": true },
		{ "app_name": "BaseDLC", "title": "Some DLC", "is_dlc": true }
	]`))
	if err != nil {
		t.Fatalf("parse: %v", err)
	}
	if len(games) != 1 {
		t.Fatalf("got %d games, want 1; DLC should not be a tile", len(games))
	}
	if games[0].SourceID != "Base" {
		t.Errorf("surviving entry = %q, want Base", games[0].SourceID)
	}
}

func TestTitleFallsBackToAppName(t *testing.T) {
	games, err := parse([]byte(`[ { "app_name": "SomeSlug", "title": "" } ]`))
	if err != nil {
		t.Fatalf("parse: %v", err)
	}
	if games[0].Title != "SomeSlug" {
		t.Errorf("title = %q, want the app name as a fallback", games[0].Title)
	}
}

func TestEntryWithoutAppNameIsSkipped(t *testing.T) {
	games, err := parse([]byte(`[ { "title": "No App Name" } ]`))
	if err != nil {
		t.Fatalf("parse: %v", err)
	}
	if len(games) != 0 {
		t.Errorf("got %d games, want 0", len(games))
	}
}

func TestMalformedOutputIsReported(t *testing.T) {
	if _, err := parse([]byte(`not json`)); err == nil {
		t.Error("malformed output should be reported")
	}
}

func TestNotLoggedInIsNotAnError(t *testing.T) {
	// This machine has legendary installed but not authenticated, which is the
	// expected state for a fresh install and must not surface as a failure.
	if _, err := Discover().Games(t.Context()); err != nil && err != ErrNoClient {
		t.Errorf("Games() = %v, want nil or ErrNoClient", err)
	}
}
