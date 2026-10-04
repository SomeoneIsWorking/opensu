// Package app wires the library, the gamepad reader and the window together. It
// is the only place the frontend talks to, and the only place that knows about
// Wails.
package app

import (
	"context"
	"log/slog"
	"sync"

	"github.com/wailsapp/wails/v2/pkg/runtime"

	"iideck/internal/config"
	"iideck/internal/gamepad"
	"iideck/internal/launch"
	"iideck/internal/library"
	"iideck/internal/library/epic"
	"iideck/internal/library/gog"
	"iideck/internal/library/roms"
	"iideck/internal/library/steam"
)

// eventGamepad is the frontend event name for controller state changes.
const eventGamepad = "gamepad"

// eventLibrary is the frontend event name for a catalog refresh completing.
const eventLibrary = "library"

// App is the frontend's view of the program. Wails binds its exported methods.
type App struct {
	ctx     context.Context
	cfg     config.Config
	catalog *library.Catalog
	pad     *gamepad.Reader
	launch  *launch.Launcher

	mu    sync.RWMutex
	games []*library.Game
}

// New builds the application from the run configuration.
func New(cfg config.Config) *App {
	romSource := roms.Discover(cfg)
	catalog := library.NewCatalog(
		steam.Discover(cfg.SteamRoots),
		epic.Discover(),
		gog.Discover(),
		romSource,
	)
	slog.Info("rom sources configured",
		"roots", len(romSource.Roots()),
		"systems", len(romSource.Systems()),
	)
	return &App{
		cfg:     cfg,
		catalog: catalog,
		pad:     gamepad.NewReader(),
		launch:  launch.New(),
	}
}

// Startup begins reading controllers and loads the library in the background so
// the window appears immediately.
func (a *App) Startup(ctx context.Context) {
	a.ctx = ctx

	go a.pad.Watch()
	go a.forwardInput()

	go func() {
		if _, err := a.Refresh(); err != nil {
			// A partially readable library is still worth showing; the error
			// describes which source failed.
			slog.Warn("library refresh reported problems", "error", err)
		}
	}()
}

// Shutdown stops the reader so the process can exit.
func (a *App) Shutdown(context.Context) {
	if err := a.pad.Close(); err != nil {
		slog.Warn("closing gamepad reader", "error", err)
	}
}

// forwardInput relays controller events to the frontend. The reader's channel is
// drained continuously so input is never dropped for want of a reader.
func (a *App) forwardInput() {
	for ev := range a.pad.Events() {
		runtime.EventsEmit(a.ctx, eventGamepad, ev)
	}
}

// Games returns the current catalog.
func (a *App) Games() []*library.Game {
	a.mu.RLock()
	defer a.mu.RUnlock()
	return a.games
}

// Refresh rereads every source and returns the merged catalog.
func (a *App) Refresh() ([]*library.Game, error) {
	games, err := a.catalog.Refresh(context.Background())

	a.mu.Lock()
	a.games = games
	a.mu.Unlock()

	slog.Info("catalog refreshed", "games", len(games))
	runtime.EventsEmit(a.ctx, eventLibrary, len(games))
	return games, err
}

// Launch starts a game and hides the window while it runs. It returns as soon as
// the game is on its way; the shell returns when the game exits.
func (a *App) Launch(id string) error {
	game := a.game(id)
	if game == nil {
		return &launch.Error{ID: id, Reason: "not in the catalog"}
	}
	slog.Info("launching", "id", id, "title", game.Title, "program", game.Launch.Program)

	// The window callbacks take the Wails context, which the launcher does not
	// need to know about.
	hide := func() { runtime.WindowHide(a.ctx) }
	show := func() { runtime.WindowShow(a.ctx) }
	go func() {
		if err := a.launch.Start(game, hide, show); err != nil {
			slog.Error("game ended with an error", "id", id, "error", err)
		}
	}()
	return nil
}

// ShowWindow brings the shell back into view. It is bound so the frontend can
// restore the window without a restart.
func (a *App) ShowWindow() {
	runtime.WindowShow(a.ctx)
}

// Rumble plays a force-feedback effect on the most recently connected pad.
func (a *App) Rumble(strong, weak, durationMs int) error {
	device := a.pad.Primary()
	if device == "" {
		return gamepad.ErrNoRumble
	}
	return a.pad.Rumble(device, uint16(strong), uint16(weak), uint16(durationMs))
}

// game finds one catalog entry by id.
func (a *App) game(id string) *library.Game {
	a.mu.RLock()
	defer a.mu.RUnlock()
	for _, g := range a.games {
		if g.ID == id {
			return g
		}
	}
	return nil
}
