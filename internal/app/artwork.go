package app

import (
	"net/http"
	"os"
	"path/filepath"
	"strings"

	"iideck/internal/library"
)

// Artwork routes. The frontend requests these instead of file paths so the
// shell keeps control of which files it will read.
const (
	routeArtwork     = "/artwork/"
	routeArtworkWide = "/artwork-wide/"
)

// ServeArtwork serves box art from disk for the asset server. Only paths the
// catalog already recorded are served, so the shell cannot be used to read
// arbitrary files.
func (a *App) ServeArtwork() http.Handler {
	return http.HandlerFunc(a.serveArtwork)
}

func (a *App) serveArtwork(w http.ResponseWriter, r *http.Request) {
	path := r.URL.Path
	var (
		game *library.Game
		wide bool
	)
	switch {
	case strings.HasPrefix(path, routeArtworkWide):
		game = a.game(strings.TrimPrefix(path, routeArtworkWide))
		wide = true
	case strings.HasPrefix(path, routeArtwork):
		game = a.game(strings.TrimPrefix(path, routeArtwork))
	default:
		http.NotFound(w, r)
		return
	}

	if game == nil {
		http.NotFound(w, r)
		return
	}
	file := game.ArtworkFile
	if wide {
		file = game.ArtworkWideFile
	}
	serveImage(w, r, file)
}

// serveImage writes an image file with a content type from its extension.
func serveImage(w http.ResponseWriter, r *http.Request, path string) {
	if path == "" {
		http.NotFound(w, r)
		return
	}
	info, err := os.Stat(path)
	if err != nil || info.IsDir() {
		http.NotFound(w, r)
		return
	}
	f, err := os.Open(path)
	if err != nil {
		http.NotFound(w, r)
		return
	}
	defer f.Close()

	w.Header().Set("Content-Type", contentType(path))
	// Artwork changes only when the store updates it, so it is safe to cache.
	w.Header().Set("Cache-Control", "public, max-age=86400")
	http.ServeContent(w, r, filepath.Base(path), info.ModTime(), f)
}

// contentType maps an image extension to its media type. Steam writes JPEG and
// occasionally WebP.
func contentType(path string) string {
	switch strings.ToLower(filepath.Ext(path)) {
	case ".jpg", ".jpeg":
		return "image/jpeg"
	case ".png":
		return "image/png"
	case ".webp":
		return "image/webp"
	default:
		return "application/octet-stream"
	}
}
