// Package steam reads a local Steam installation: which apps exist, whether
// they are installed, what artwork exists, and when they were last played. It
// never talks to the Steam client, and never modifies it.
package steam

import (
	"context"
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"time"

	"iideck/internal/library"
	"iideck/internal/vdf"
)

// Client reads one or more Steam install roots.
type Client struct {
	roots []string
}

// knownRoots are the standard install locations, in the order Steam itself
// prefers. A root qualifies when it holds both a steamapps and a config
// directory.
var knownRoots = []string{
	".local/share/Steam",
	".steam/steam",
	".steam/root",
	".steam/debian-installation",
	".var/app/com.valvesoftware.Steam/.local/share/Steam",
}

// artworkWideCandidates and artworkTallCandidates are tried in order. Steam
// stores portrait art under one name and, since 2023, landscape hero art under
// another, and older caches still hold only the legacy grid images.
var (
	artworkTallCandidates = []string{
		"appcache/librarycache/%s/library_600x900.jpg",
		"appcache/librarycache/%s/library_600x900.png",
		"appcache/librarycache/%s_600x900.jpg",
		"appcache/librarycache/%s/library_600x900.webp",
		"config/grid/%sp.jpg",
		"config/grid/%s.jpg",
		"config/grid/%sp.png",
	}
	artworkWideCandidates = []string{
		"appcache/librarycache/%s_library_hero.jpg",
		"appcache/librarycache/%s_library_hero.png",
		"appcache/librarycache/%s/library_hero.jpg",
		"appcache/librarycache/%s/library_hero.png",
		"appcache/librarycache/%s_header.jpg",
	}
)

// Discover finds every usable Steam root. When explicit roots are given they
// are authoritative — the user asked for exactly those — otherwise the standard
// locations are probed.
func Discover(explicit []string) *Client {
	candidates := explicit
	if len(candidates) == 0 {
		candidates = make([]string, 0, len(knownRoots))
		for _, rel := range knownRoots {
			candidates = append(candidates, filepath.Join(homeDir(), rel))
		}
	}

	seen := map[string]struct{}{}
	var roots []string
	for _, path := range candidates {
		qualified := qualify(path)
		if qualified == "" {
			continue
		}
		if _, ok := seen[qualified]; ok {
			continue
		}
		seen[qualified] = struct{}{}
		if isRoot(qualified) {
			roots = append(roots, qualified)
		}
	}
	return &Client{roots: roots}
}

// Name identifies this source to the catalog.
func (c *Client) Name() library.Source { return library.SourceSteam }

// Roots returns the install roots in use, for diagnostics.
func (c *Client) Roots() []string { return append([]string(nil), c.roots...) }

// Games lists every app the installation knows about, installed or not.
func (c *Client) Games(ctx context.Context) ([]*library.Game, error) {
	if len(c.roots) == 0 {
		return nil, errors.New("no Steam installation found")
	}

	var (
		games []*library.Game
		errs  []string
		seen  = map[string]struct{}{}
	)
	for _, root := range c.roots {
		play := loadUserData(root)
		for _, lib := range c.libraryFolders(ctx, root) {
			found, err := readLibrary(ctx, root, lib.path, play)
			if err != nil {
				errs = append(errs, fmt.Sprintf("%s: %v", lib.path, err))
				continue
			}
			for _, g := range found {
				// One app id is one tile even when Steam lists the same app in
				// two library folders.
				if _, dup := seen[g.ID]; dup {
					continue
				}
				seen[g.ID] = struct{}{}
				games = append(games, g)
			}
		}
	}
	if len(games) == 0 && len(errs) > 0 {
		return nil, errors.New(strings.Join(errs, "; "))
	}
	return games, nil
}

// libraryFolder is one place Steam keeps games.
type libraryFolder struct {
	path string
	// contentID identifies the physical library. Steam assigns the same value
	// to two mount points of one drive, which is how a machine with an internal
	// and an external mount of the same volume ends up duplicating every game.
	contentID string
}

// libraryFolders returns the install root plus every extra library Steam knows
// about, across both the current and the legacy libraryfolders layouts. The
// install root always comes first so its own manifests win, and two folders
// that name the same library are returned once.
func (c *Client) libraryFolders(ctx context.Context, root string) []libraryFolder {
	// Steam keeps entries for drives that are not currently mounted. A stale
	// entry has to be discarded before anything else, because two mount points
	// of one drive share a content id and a stale one must never win over the
	// mounted one that actually holds the games.
	candidates := []libraryFolder{{path: root}}
	for _, f := range c.readLibraryFolders(root) {
		if f.path != root {
			candidates = append(candidates, f)
		}
		if err := ctx.Err(); err != nil {
			return presentFolders(candidates)
		}
	}
	return presentFolders(candidates)
}

// readLibraryFolders reads the paths Steam records for its extra libraries,
// accepting both the current and the legacy layouts.
func (c *Client) readLibraryFolders(root string) []libraryFolder {
	data, err := os.ReadFile(filepath.Join(root, "steamapps", "libraryfolders.vdf"))
	if err != nil {
		return nil
	}
	doc, err := vdf.Parse(string(data))
	if err != nil {
		return nil
	}
	for _, key := range []string{"libraryfolders", "LibraryFolders"} {
		block, ok := doc.Node(key)
		if !ok {
			continue
		}
		var folders []libraryFolder
		for _, entry := range block.Keys() {
			var path, contentID string
			switch value := block[entry].(type) {
			case string:
				// Legacy layout: "1" "/mnt/games/SteamLibrary".
				path = value
			case vdf.Map:
				// Current layout: "1" { "path" "..." "contentid" "..." }.
				path, _ = value.Get("path")
				contentID, _ = value.Get("contentid")
			}
			if path == "" {
				continue
			}
			folders = append(folders, libraryFolder{path: path, contentID: contentID})
		}
		return folders
	}
	return nil
}

// presentFolders drops folders with no steamapps directory, then returns one
// entry per distinct path and per distinct content id.
func presentFolders(candidates []libraryFolder) []libraryFolder {
	var (
		out       []libraryFolder
		seenPath  = map[string]struct{}{}
		seenConte = map[string]struct{}{}
	)
	for _, f := range candidates {
		if _, dup := seenPath[f.path]; dup {
			continue
		}
		if f.path != "" && !hasSteamapps(f.path) {
			continue
		}
		seenPath[f.path] = struct{}{}
		if f.contentID != "" {
			if _, dup := seenConte[f.contentID]; dup {
				continue
			}
			seenConte[f.contentID] = struct{}{}
		}
		out = append(out, f)
	}
	return out
}

// hasSteamapps reports whether a library folder is currently readable.
func hasSteamapps(path string) bool {
	info, err := os.Stat(filepath.Join(path, "steamapps"))
	return err == nil && info.IsDir()
}

// userPlay is what a user's config says about one app.
type userPlay struct {
	lastPlayed  time.Time
	playtimeMin int
	favourite   bool
}

// loadUserData merges every user profile's app data. Later profiles overwrite
// earlier ones, which matches how Steam treats a shared machine.
func loadUserData(root string) map[string]userPlay {
	out := map[string]userPlay{}
	entries, err := os.ReadDir(filepath.Join(root, "userdata"))
	if err != nil {
		return out
	}
	for _, entry := range entries {
		if !entry.IsDir() || entry.Name() == "0" || entry.Name() == "ac" {
			continue
		}
		file := filepath.Join(root, "userdata", entry.Name(), "config", "localconfig.vdf")
		data, err := os.ReadFile(file)
		if err != nil {
			continue
		}
		doc, err := vdf.Parse(string(data))
		if err != nil {
			continue
		}
		user := userStore(doc)
		mergeApps(user, out)
		mergeFavourites(user, out)
	}
	return out
}

// userStore returns the node holding a user's Steam settings. Steam wraps that
// document in a "UserLocalConfigStore" key; other files it writes are not
// wrapped, so both shapes are accepted.
func userStore(doc vdf.Map) vdf.Map {
	if wrapped, ok := doc.Node("UserLocalConfigStore"); ok {
		return wrapped
	}
	return doc
}

func mergeApps(doc vdf.Map, out map[string]userPlay) {
	apps, ok := doc.Node("Software", "Valve", "Steam", "Apps")
	if !ok {
		return
	}
	for _, appid := range apps.Keys() {
		entry, ok := apps.Node(appid)
		if !ok {
			continue
		}
		play := out[appid]
		if raw, ok := entry.Get("LastPlayed"); ok {
			if secs, err := strconv.ParseInt(strings.TrimSpace(raw), 10, 64); err == nil && secs > 0 {
				play.lastPlayed = time.Unix(secs, 0).UTC()
			}
		}
		if minutes, ok := entry.GetInt("Playtime"); ok && minutes > 0 {
			play.playtimeMin = int(minutes)
		}
		if categories, ok := entry.Node("Categories"); ok && categories.Has("Favorites") {
			play.favourite = true
		}
		out[appid] = play
	}
}

func mergeFavourites(doc vdf.Map, out map[string]userPlay) {
	favourites, ok := doc.Node("Software", "Valve", "Steam", "Favorites")
	if !ok {
		return
	}
	for _, appid := range favourites.Keys() {
		play := out[appid]
		play.favourite = true
		out[appid] = play
	}
}

// readLibrary reads every app manifest in one library folder.
func readLibrary(ctx context.Context, root, libraryPath string, play map[string]userPlay) ([]*library.Game, error) {
	appsDir := filepath.Join(libraryPath, "steamapps")
	manifests, err := filepath.Glob(filepath.Join(appsDir, "appmanifest_*.acf"))
	if err != nil {
		return nil, err
	}
	if len(manifests) == 0 {
		return nil, nil
	}
	sortNumeric(manifests)

	var games []*library.Game
	for _, manifest := range manifests {
		if err := ctx.Err(); err != nil {
			return games, err
		}
		appid := strings.TrimSuffix(strings.TrimPrefix(filepath.Base(manifest), "appmanifest_"), ".acf")
		game, ok := readManifest(root, libraryPath, manifest, appid, play)
		if !ok {
			continue
		}
		games = append(games, game)
	}
	return games, nil
}

func readManifest(root, libraryPath, manifestPath, appid string, play map[string]userPlay) (*library.Game, bool) {
	data, err := os.ReadFile(manifestPath)
	if err != nil {
		return nil, false
	}
	doc, err := vdf.Parse(string(data))
	if err != nil {
		return nil, false
	}
	state, ok := doc.Node("AppState")
	if !ok {
		return nil, false
	}
	name, ok := state.Get("name")
	if !ok || strings.TrimSpace(name) == "" {
		return nil, false
	}
	if declared, ok := state.GetInt("appid"); ok && declared != 0 {
		appid = strconv.FormatInt(declared, 10)
	}
	installDir, _ := state.Get("installdir")

	installed := stateFlagsInstalled(state)
	if installDir != "" {
		if info, err := os.Stat(filepath.Join(libraryPath, "steamapps", "common", installDir)); err == nil && info.IsDir() {
			installed = true
		}
	}

	game := &library.Game{
		ID:        string(library.SourceSteam) + ":" + appid,
		Source:    library.SourceSteam,
		SourceID:  appid,
		Title:     strings.TrimSpace(name),
		Installed: installed,
		Launch: library.LaunchSpec{
			Program: "steam",
			Args:    []string{"steam://rungameid/" + appid},
		},
		// The Wine prefix path carries the app id, so a running game is
		// recognisable in /proc regardless of what the executable is called.
		ProcessHint: "compatdata/" + appid,
	}
	if p, ok := play[appid]; ok {
		game.PlaytimeMinutes = p.playtimeMin
		game.Favourite = p.favourite
		if !p.lastPlayed.IsZero() {
			played := p.lastPlayed
			game.LastPlayed = &played
		}
	}
	game.ArtworkFile = firstExisting(root, artworkTallCandidates, appid)
	game.ArtworkWideFile = firstExisting(root, artworkWideCandidates, appid)
	return game, true
}

// stateFlagsInstalled reads Steam's own install flag, which is authoritative
// even when the game directory has been moved or is a symlink.
func stateFlagsInstalled(state vdf.Map) bool {
	flags, ok := state.GetInt("StateFlags")
	return ok && flags&4 != 0
}

// firstExisting returns the first candidate path that is a readable file.
func firstExisting(root string, candidates []string, appid string) string {
	for _, pattern := range candidates {
		path := filepath.Join(root, filepath.FromSlash(fmt.Sprintf(pattern, appid)))
		if info, err := os.Stat(path); err == nil && !info.IsDir() {
			return path
		}
	}
	return ""
}

func isRoot(path string) bool {
	for _, dir := range []string{"steamapps", "config"} {
		if info, err := os.Stat(filepath.Join(path, dir)); err != nil || !info.IsDir() {
			return false
		}
	}
	return true
}

func qualify(path string) string {
	info, err := os.Stat(path)
	if err != nil {
		return ""
	}
	if !info.IsDir() {
		return ""
	}
	// Steam's own directories are symlinks to the real install; resolve so
	// artwork lookups do not depend on which path was discovered.
	if resolved, err := filepath.EvalSymlinks(path); err == nil {
		return resolved
	}
	return path
}

func homeDir() string {
	home, err := os.UserHomeDir()
	if err != nil {
		return ""
	}
	return home
}

// sortNumeric orders manifests by app id so the catalog is stable regardless of
// readdir order.
func sortNumeric(paths []string) {
	for i := 1; i < len(paths); i++ {
		for j := i; j > 0 && appIDOf(paths[j]) < appIDOf(paths[j-1]); j-- {
			paths[j], paths[j-1] = paths[j-1], paths[j]
		}
	}
}

func appIDOf(manifestPath string) string {
	name := filepath.Base(manifestPath)
	name = strings.TrimPrefix(name, "appmanifest_")
	name = strings.TrimSuffix(name, ".acf")
	n, err := strconv.Atoi(name)
	if err != nil {
		return name
	}
	return fmt.Sprintf("%020d", n)
}
