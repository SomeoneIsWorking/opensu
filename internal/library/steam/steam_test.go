package steam

import (
	"context"
	"os"
	"path/filepath"
	"testing"
)

// fakeSteam builds a Steam install root with one installed game, one known but
// uninstalled game, artwork, a user profile and a second library folder using
// the current libraryfolders layout.
func fakeSteam(t *testing.T) (root, extraLibrary string) {
	t.Helper()
	base := t.TempDir()
	root = filepath.Join(base, "Steam")
	extraLibrary = filepath.Join(base, "Games")

	for _, dir := range []string{
		filepath.Join(root, "steamapps"),
		filepath.Join(root, "config", "grid"),
		filepath.Join(root, "appcache", "librarycache", "440"),
		filepath.Join(root, "steamapps", "common", "Portal 2"),
		filepath.Join(root, "userdata", "1234567", "config"),
		filepath.Join(extraLibrary, "steamapps", "common", "Deep Game"),
	} {
		if err := os.MkdirAll(dir, 0o755); err != nil {
			t.Fatalf("MkdirAll %s: %v", dir, err)
		}
	}

	write := func(path, content string) {
		t.Helper()
		if err := os.WriteFile(path, []byte(content), 0o644); err != nil {
			t.Fatalf("WriteFile %s: %v", path, err)
		}
	}

	write(filepath.Join(root, "steamapps", "libraryfolders.vdf"), `"libraryfolders"
{
	"0"
	{
		"path"		"`+root+`"
	}
	"1"
	{
		"path"		"`+extraLibrary+`"
		"apps"
		{
			"999"		"1"
		}
	}
}`)

	write(filepath.Join(root, "steamapps", "appmanifest_440.acf"), `"AppState"
{
	"appid"		"440"
	"name"		"Portal 2"
	"installdir"		"Portal 2"
	"StateFlags"		"4"
}`)
	write(filepath.Join(root, "steamapps", "appmanifest_620.acf"), `"AppState"
{
	"appid"		"620"
	"name"		"Portal 2"
	"installdir"		"Portal 2 (Missing)"
	"StateFlags"		"1026"
}`)

	write(filepath.Join(extraLibrary, "steamapps", "appmanifest_999.acf"), `"AppState"
{
	"appid"		"999"
	"name"		"Deep Game"
	"installdir"		"Deep Game"
	"StateFlags"		"4"
}`)

	// Artwork: 440 has both portrait and landscape, 620 has only the legacy grid
	// image, 999 has none.
	write(filepath.Join(root, "appcache", "librarycache", "440", "library_600x900.jpg"), "portrait")
	write(filepath.Join(root, "appcache", "librarycache", "440", "library_hero.jpg"), "landscape")
	write(filepath.Join(root, "config", "grid", "620p.jpg"), "legacy-grid")

	write(filepath.Join(root, "userdata", "1234567", "config", "localconfig.vdf"), `"UserLocalConfigStore"
{
	"Software"
	{
		"Valve"
		{
			"Steam"
			{
				"Apps"
				{
					"440"
					{
						"LastPlayed"		"1700000000"
						"Playtime"		"145"
					}
					"620"
					{
						"LastPlayed"		"1600000000"
					}
				}
				"Favorites"
				{
					"440"		"1"
				}
			}
		}
	}
}`)
	return root, extraLibrary
}

func TestGamesReadsEveryLibraryFolder(t *testing.T) {
	root, _ := fakeSteam(t)
	client := Discover([]string{root})

	games, err := client.Games(context.Background())
	if err != nil {
		t.Fatalf("Games: %v", err)
	}
	byID := map[string]string{}
	for _, g := range games {
		byID[g.ID] = g.Title
	}
	if len(byID) != 3 {
		t.Fatalf("got %d entries %v, want 3 (440, 620 and extra library 999)", len(byID), byID)
	}
	for id, title := range map[string]string{
		"steam:440": "Portal 2",
		"steam:620": "Portal 2",
		"steam:999": "Deep Game",
	} {
		if byID[id] != title {
			t.Errorf("%s = %q, want %q", id, byID[id], title)
		}
	}
}

func TestInstallStateFollowsFilesystemAndFlags(t *testing.T) {
	root, _ := fakeSteam(t)
	games, err := Discover([]string{root}).Games(context.Background())
	if err != nil {
		t.Fatalf("Games: %v", err)
	}
	for _, g := range games {
		want := g.SourceID == "440" || g.SourceID == "999"
		if g.Installed != want {
			t.Errorf("%s installed = %v, want %v (StateFlags and directory disagree on purpose)", g.ID, g.Installed, want)
		}
	}
}

func TestArtworkResolutionPrefersModernImages(t *testing.T) {
	root, _ := fakeSteam(t)
	games, err := Discover([]string{root}).Games(context.Background())
	if err != nil {
		t.Fatalf("Games: %v", err)
	}
	for _, g := range games {
		switch g.SourceID {
		case "440":
			if filepath.Base(g.ArtworkFile) != "library_600x900.jpg" {
				t.Errorf("440 portrait = %q", g.ArtworkFile)
			}
			if filepath.Base(g.ArtworkWideFile) != "library_hero.jpg" {
				t.Errorf("440 wide = %q", g.ArtworkWideFile)
			}
		case "620":
			if filepath.Base(g.ArtworkFile) != "620p.jpg" {
				t.Errorf("620 should fall back to the legacy grid image, got %q", g.ArtworkFile)
			}
		case "999":
			if g.ArtworkFile != "" || g.ArtworkWideFile != "" {
				t.Errorf("999 has no artwork, got %q / %q", g.ArtworkFile, g.ArtworkWideFile)
			}
		}
	}
}

func TestUserDataMergesPlaytimeAndFavourites(t *testing.T) {
	root, _ := fakeSteam(t)
	games, err := Discover([]string{root}).Games(context.Background())
	if err != nil {
		t.Fatalf("Games: %v", err)
	}
	for _, g := range games {
		switch g.SourceID {
		case "440":
			if g.PlaytimeMinutes != 145 {
				t.Errorf("440 playtime = %d, want 145", g.PlaytimeMinutes)
			}
			if !g.Favourite {
				t.Error("440 should be marked favourite")
			}
			if g.LastPlayed == nil {
				t.Error("440 should have a last played time")
			}
		case "620":
			if g.LastPlayed == nil {
				t.Error("620 should have a last played time")
			}
			if g.PlaytimeMinutes != 0 {
				t.Errorf("620 playtime = %d, want 0", g.PlaytimeMinutes)
			}
		case "999":
			if g.LastPlayed != nil {
				t.Error("999 was never played and must not report a time")
			}
		}
	}
}

func TestLaunchSpecTargetsSteamAppID(t *testing.T) {
	root, _ := fakeSteam(t)
	games, err := Discover([]string{root}).Games(context.Background())
	if err != nil {
		t.Fatalf("Games: %v", err)
	}
	for _, g := range games {
		if g.Launch.Program != "steam" {
			t.Errorf("%s program = %q", g.ID, g.Launch.Program)
		}
		want := "steam://rungameid/" + g.SourceID
		if len(g.Launch.Args) != 1 || g.Launch.Args[0] != want {
			t.Errorf("%s args = %v, want [%s]", g.ID, g.Launch.Args, want)
		}
		if g.ProcessHint != "compatdata/"+g.SourceID {
			t.Errorf("%s process hint = %q", g.ID, g.ProcessHint)
		}
	}
}

func TestLegacyLibraryFoldersLayout(t *testing.T) {
	base := t.TempDir()
	root := filepath.Join(base, "Steam")
	legacy := filepath.Join(base, "Old")
	for _, dir := range []string{
		filepath.Join(root, "steamapps"),
		filepath.Join(root, "config"),
		filepath.Join(legacy, "steamapps", "common", "Old Game"),
	} {
		if err := os.MkdirAll(dir, 0o755); err != nil {
			t.Fatal(err)
		}
	}
	os.WriteFile(filepath.Join(root, "steamapps", "libraryfolders.vdf"),
		[]byte(`"LibraryFolders"
{
	"1"		"`+legacy+`"
}`), 0o644)
	os.WriteFile(filepath.Join(legacy, "steamapps", "appmanifest_77.acf"),
		[]byte(`"AppState" { "appid" "77" "name" "Old Game" "installdir" "Old Game" "StateFlags" "4" }`), 0o644)

	games, err := Discover([]string{root}).Games(context.Background())
	if err != nil {
		t.Fatalf("Games: %v", err)
	}
	found := false
	for _, g := range games {
		if g.ID == "steam:77" {
			found = true
		}
	}
	if !found {
		t.Errorf("legacy library folder was not read, got %d entries", len(games))
	}
}

func TestNoInstallationIsAnError(t *testing.T) {
	client := Discover([]string{filepath.Join(t.TempDir(), "nope")})
	if len(client.Roots()) != 0 {
		t.Fatalf("expected no usable roots, got %v", client.Roots())
	}
	if _, err := client.Games(context.Background()); err == nil {
		t.Error("expected an error when no Steam installation exists")
	}
}
