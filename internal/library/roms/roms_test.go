package roms

import (
	"os"
	"path/filepath"
	"testing"

	"iideck/internal/config"
)

func testConfig(romRoot string) config.Config {
	return config.Config{
		ROMRoots: []string{romRoot},
		EmulatorCommands: map[string][]string{
			"SNES": {"snes9x-gtk", "-fullscreen"},
		},
		EmulatorExtensions: map[string]string{
			".sfc": "SNES",
			".nes": "NES",
		},
	}
}

func writeROM(t *testing.T, dir, name string) {
	t.Helper()
	if err := os.WriteFile(filepath.Join(dir, name), []byte("rom"), 0o644); err != nil {
		t.Fatal(err)
	}
}

func TestScansConfiguredRootAndResolvesEmulator(t *testing.T) {
	dir := t.TempDir()
	writeROM(t, dir, "Super Metroid.sfc")
	writeROM(t, dir, "notes.txt")
	writeROM(t, dir, "no-extension")

	games, err := Discover(testConfig(dir)).Games(t.Context())
	if err != nil {
		t.Fatalf("Games: %v", err)
	}
	if len(games) != 1 {
		t.Fatalf("got %d entries, want 1 (only the .sfc is a ROM)", len(games))
	}
	g := games[0]
	if g.Title != "Super Metroid" {
		t.Errorf("title = %q, want %q", g.Title, "Super Metroid")
	}
	if g.SourceID != "SNES" {
		t.Errorf("system = %q, want SNES", g.SourceID)
	}
	if !g.Installed {
		t.Error("a ROM file on disk should be installed")
	}
	// The emulator's own arguments come before the ROM path.
	want := []string{"-fullscreen", filepath.Join(dir, "Super Metroid.sfc")}
	if g.Launch.Program != "snes9x-gtk" {
		t.Errorf("program = %q", g.Launch.Program)
	}
	if len(g.Launch.Args) != 2 || g.Launch.Args[0] != want[0] || g.Launch.Args[1] != want[1] {
		t.Errorf("args = %v, want %v", g.Launch.Args, want)
	}
}

func TestUnconfiguredSystemHasNoLaunchCommand(t *testing.T) {
	dir := t.TempDir()
	writeROM(t, dir, "Zelda.nes")

	games, err := Discover(testConfig(dir)).Games(t.Context())
	if err != nil {
		t.Fatalf("Games: %v", err)
	}
	if len(games) != 1 {
		t.Fatalf("got %d entries, want 1", len(games))
	}
	// NES has no configured emulator, so the entry is listed but not launchable.
	// The shell reports that rather than pretending to start something.
	if !games[0].Launch.Empty() {
		t.Errorf("launch = %+v, want empty for an unconfigured system", games[0].Launch)
	}
}

func TestExtensionMatchIsCaseInsensitive(t *testing.T) {
	dir := t.TempDir()
	writeROM(t, dir, "UPPER.SFC")
	writeROM(t, dir, "mixed.Smc")

	// The configuration only knows lowercase .sfc, but collections mix cases.
	cfg := testConfig(dir)
	cfg.EmulatorExtensions = map[string]string{".sfc": "SNES", ".smc": "SNES"}

	games, err := Discover(cfg).Games(t.Context())
	if err != nil {
		t.Fatalf("Games: %v", err)
	}
	if len(games) != 2 {
		t.Fatalf("got %d entries, want 2", len(games))
	}
	for _, g := range games {
		if g.SourceID != "SNES" {
			t.Errorf("%s mapped to %q, want SNES", g.Title, g.SourceID)
		}
	}
}

func TestIdsAreUniquePerFile(t *testing.T) {
	dir := t.TempDir()
	writeROM(t, dir, "A.sfc")
	writeROM(t, dir, "B.sfc")

	games, err := Discover(testConfig(dir)).Games(t.Context())
	if err != nil {
		t.Fatalf("Games: %v", err)
	}
	if games[0].ID == games[1].ID {
		t.Errorf("both entries share id %q", games[0].ID)
	}
}

func TestNoRootsIsEmptyNotAnError(t *testing.T) {
	s := Discover(config.Config{})
	games, err := s.Games(t.Context())
	if err != nil {
		t.Fatalf("Games: %v", err)
	}
	if len(games) != 0 {
		t.Errorf("got %d entries with no ROM roots configured", len(games))
	}
	if s.Name() != "rom" {
		t.Errorf("Name = %q, want rom", s.Name())
	}
}

func TestMissingRootIsReportedNotIgnored(t *testing.T) {
	missing := filepath.Join(t.TempDir(), "not-here")
	_, err := Discover(testConfig(missing)).Games(t.Context())
	if err == nil {
		t.Error("a configured ROM root that does not exist should be reported")
	}
}

func TestSubdirectoriesAreNotDescended(t *testing.T) {
	dir := t.TempDir()
	nested := filepath.Join(dir, "sub")
	if err := os.MkdirAll(nested, 0o755); err != nil {
		t.Fatal(err)
	}
	writeROM(t, nested, "nested.sfc")
	writeROM(t, dir, "top.sfc")

	games, err := Discover(testConfig(dir)).Games(t.Context())
	if err != nil {
		t.Fatalf("Games: %v", err)
	}
	if len(games) != 1 || games[0].Title != "top" {
		t.Errorf("got %d entries %+v, want only the top-level ROM", len(games), games)
	}
}

func TestSystemsAreDeduplicatedAndSorted(t *testing.T) {
	cfg := config.Config{
		EmulatorExtensions: map[string]string{
			".sfc": "SNES", ".smc": "SNES", ".nes": "NES", ".z64": "Nintendo 64",
		},
	}
	got := Discover(cfg).Systems()
	want := []string{"NES", "Nintendo 64", "SNES"}
	if len(got) != len(want) {
		t.Fatalf("Systems = %v, want %v", got, want)
	}
	for i := range want {
		if got[i] != want[i] {
			t.Errorf("Systems = %v, want %v", got, want)
			break
		}
	}
}
