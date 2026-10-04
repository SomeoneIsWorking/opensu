package vdf

import (
	"errors"
	"testing"
)

// A trimmed real manifest: nested block, quoted name, numeric fields, a
// Windows-style escaped path and a comment.
const manifest = `
"AppState"
{
	"appid"		"1070560"
	"name"		"Vampire Survivors"
	"installdir"		"Vampire Survivors"
	"StateFlags"		"4"
	// a comment Steam writes itself
	"LastUpdated"		"1736200000"
	"UserConfig"
	{
		"language"		"english"
	}
}
`

func TestParseNestedDocument(t *testing.T) {
	doc, err := Parse(manifest)
	if err != nil {
		t.Fatalf("ParseBytes: %v", err)
	}
	if got, _ := doc.Get("AppState", "name"); got != "Vampire Survivors" {
		t.Errorf("name = %q, want %q", got, "Vampire Survivors")
	}
	if got, ok := doc.GetInt("AppState", "StateFlags"); !ok || got != 4 {
		t.Errorf("StateFlags = %d (ok=%v), want 4", got, ok)
	}
	if got, ok := doc.Get("AppState", "UserConfig", "language"); !ok || got != "english" {
		t.Errorf("language = %q (ok=%v), want english", got, ok)
	}
}

func TestLookupIsCaseInsensitive(t *testing.T) {
	doc, err := Parse(manifest)
	if err != nil {
		t.Fatalf("ParseBytes: %v", err)
	}
	if got, ok := doc.Get("appstate", "NAME"); !ok || got != "Vampire Survivors" {
		t.Errorf("case-insensitive lookup = %q (ok=%v)", got, ok)
	}
}

func TestParseQuotedEscapes(t *testing.T) {
	doc, err := Parse(`"root" { "path" "D:\\Steam Library" }`)
	if err != nil {
		t.Fatalf("ParseBytes: %v", err)
	}
	if got, _ := doc.Get("root", "path"); got != `D:\Steam Library` {
		t.Errorf("path = %q, want %q", got, `D:\Steam Library`)
	}
}

func TestParseBareTokens(t *testing.T) {
	doc, err := Parse(`"root" { bare unquoted }`)
	if err != nil {
		t.Fatalf("ParseBytes: %v", err)
	}
	if got, _ := doc.Get("root", "bare"); got != "unquoted" {
		t.Errorf("bare = %q, want %q", got, "unquoted")
	}
}

func TestParseLibraryFoldersShape(t *testing.T) {
	const doc = `"libraryfolders"
{
	"0"
	{
		"path"		"/home/u/.local/share/Steam"
		"label"		""
		"contentid"		"123"
		"totalsize"		"0"
	}
	"1"
	{
		"path"		"/mnt/games/SteamLibrary"
		"apps"
		{
			"1070560"		"158658686"
		}
	}
}`
	parsed, err := Parse(doc)
	if err != nil {
		t.Fatalf("ParseBytes: %v", err)
	}
	folders, ok := parsed.Node("libraryfolders")
	if !ok {
		t.Fatal("libraryfolders block missing")
	}
	if got := len(folders.Keys()); got != 2 {
		t.Errorf("library count = %d, want 2", got)
	}
	if got, _ := folders.Get("1", "path"); got != "/mnt/games/SteamLibrary" {
		t.Errorf("path = %q", got)
	}
	if got, ok := folders.GetInt("1", "apps", "1070560"); !ok || got != 158658686 {
		t.Errorf("apps size = %d (ok=%v)", got, ok)
	}
}

func TestParseRejectsUnterminatedBlock(t *testing.T) {
	_, err := Parse(`"root" { "a" "b"`)
	if !errors.Is(err, ErrSyntax) {
		t.Errorf("err = %v, want ErrSyntax", err)
	}
}

func TestMissingPathIsAbsent(t *testing.T) {
	doc, err := Parse(manifest)
	if err != nil {
		t.Fatalf("ParseBytes: %v", err)
	}
	if _, ok := doc.Get("AppState", "nosuchkey"); ok {
		t.Error("missing key reported as present")
	}
	if _, ok := doc.Node("AppState", "name"); ok {
		t.Error("string leaf reported as node")
	}
}
