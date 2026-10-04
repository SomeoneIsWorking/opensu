// Command iideck is a gamepad-first shell for a game library. It shows Steam,
// Epic, GOG and emulator ROMs in one grid and launches each title into the
// runtime that already owns it.
package main

import (
	"embed"
	"log/slog"

	"github.com/wailsapp/wails/v2"
	"github.com/wailsapp/wails/v2/pkg/options"
	"github.com/wailsapp/wails/v2/pkg/options/assetserver"

	"iideck/internal/app"
	"iideck/internal/config"
	iilog "iideck/internal/log"
)

// assets holds the shell's frontend. The frontend is plain HTML, CSS and
// JavaScript with no build step, so it is embedded from source and the program
// is a single file with no install tree.
//
//go:embed all:frontend
var assets embed.FS

func main() {
	cfg := config.Load()
	iilog.Setup(cfg.Debug)

	shell := app.New(cfg)
	slog.Info("starting iideck",
		"width", cfg.Width, "height", cfg.Height,
		"fullscreen", cfg.Fullscreen, "chrome", cfg.Chrome,
	)

	err := wails.Run(&options.App{
		Title:      "iideck",
		Width:      cfg.Width,
		Height:     cfg.Height,
		Fullscreen: cfg.Fullscreen,
		Frameless:  !cfg.Chrome,
		// The shell draws its own focus ring and hints, so the window needs no
		// native decoration.
		StartHidden: false,
		AssetServer: &assetserver.Options{
			Assets: assets,
			// Artwork lives on disk outside the embedded tree, so the shell serves
			// it through the asset server rather than embedding it.
			Handler: shell.ServeArtwork(),
		},
		BackgroundColour: &options.RGBA{R: 244, G: 243, B: 247, A: 1},
		OnStartup:        shell.Startup,
		OnShutdown:       shell.Shutdown,
		Bind: []interface{}{
			shell,
		},
	})
	if err != nil {
		slog.Error("iideck stopped", "error", err)
	}
}
