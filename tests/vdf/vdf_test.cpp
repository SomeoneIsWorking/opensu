// Tests for the KeyValues parser, covering the shapes Steam actually writes:
// a real app manifest, both libraryfolders layouts, and a user config with the
// "UserLocalConfigStore" wrapper.
#include "node.hpp"

#include <cstdlib>
#include <string>

namespace {

void expect(bool condition, const char* what)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

void expectEqual(const std::string& got, const std::string& want, const char* what)
{
    if (got != want) {
        std::fprintf(stderr, "FAIL: %s = \"%s\", want \"%s\"\n", what, got.c_str(), want.c_str());
        std::exit(1);
    }
}

// A trimmed real manifest, with a comment and a numeric field.
constexpr std::string_view kManifest = R"("AppState"
{
	"appid"		"440"
	"name"		"Portal 2"
	"installdir"		"Portal 2"
	"StateFlags"		"4"
	// Steam writes this itself
	"LastUpdated"		"1736200000"
	"UserConfig"
	{
		"language"		"english"
	}
}
)";

void testManifest()
{
    iideck::vdf::ParseError error;
    const auto doc = iideck::vdf::parse(kManifest, error);
    expect(doc.has_value(), "manifest parses");

    expectEqual(doc->str({"AppState", "name"}).value_or(""), "Portal 2", "name");
    expect(doc->integer({"AppState", "StateFlags"}) == 4, "StateFlags is 4");
    expectEqual(doc->str({"AppState", "UserConfig", "language"}).value_or(""), "english", "nested language");
}

void testCaseInsensitiveLookup()
{
    iideck::vdf::ParseError error;
    const auto doc = iideck::vdf::parse(kManifest, error);
    expect(doc.has_value(), "manifest parses");
    expectEqual(doc->str({"appstate", "NAME"}).value_or(""), "Portal 2", "case-insensitive lookup");
}

void testQuotedEscapes()
{
    iideck::vdf::ParseError error;
    const auto doc = iideck::vdf::parse(R"("root" { "path" "D:\\Steam Library" })", error);
    expect(doc.has_value(), "escaped path parses");
    expectEqual(doc->str({"root", "path"}).value_or(""), "D:\\Steam Library", "escaped backslash");
}

void testBareTokens()
{
    iideck::vdf::ParseError error;
    const auto doc = iideck::vdf::parse(R"("root" { bare unquoted })", error);
    expect(doc.has_value(), "bare token parses");
    expectEqual(doc->str({"root", "bare"}).value_or(""), "unquoted", "bare value");
}

// The current layout: "1" { "path" "..." "contentid" "..." }.
constexpr std::string_view kLibraryFoldersCurrent = R"("libraryfolders"
{
	"0"
	{
		"path"		"/home/u/.local/share/Steam"
		"contentid"		"8019845413043883551"
	}
	"1"
	{
		"path"		"/mnt/games/SteamLibrary"
		"contentid"		"8019845413043883551"
	}
}
)";

// The layout Steam used before 2021: "1" "/mnt/games/SteamLibrary".
constexpr std::string_view kLibraryFoldersLegacy = R"("LibraryFolders"
{
	"TimeNextStatsReport"		"0"
	"1"		"/mnt/games/SteamLibrary"
}
)";

void testLibraryFoldersCurrent()
{
    iideck::vdf::ParseError error;
    const auto doc = iideck::vdf::parse(kLibraryFoldersCurrent, error);
    expect(doc.has_value(), "current libraryfolders parses");

    const auto folders = doc->block({"libraryfolders"});
    expect(folders.has_value(), "libraryfolders block exists");
    expect(folders->keys().size() == 2, "two folders");
    expectEqual(folders->str({"1", "path"}).value_or(""), "/mnt/games/SteamLibrary", "folder path");
    expectEqual(folders->str({"0", "contentid"}).value_or(""), "8019845413043883551", "content id");
}

void testLibraryFoldersLegacy()
{
    iideck::vdf::ParseError error;
    const auto doc = iideck::vdf::parse(kLibraryFoldersLegacy, error);
    expect(doc.has_value(), "legacy libraryfolders parses");

    const auto folders = doc->block({"LibraryFolders"});
    expect(folders.has_value(), "LibraryFolders block exists");
    // The legacy layout puts the path directly as the value.
    const auto value = folders->find("1");
    expect(value.has_value(), "folder 1 present");
    if (const auto* text = std::get_if<std::string>(&*value); text != nullptr) {
        expectEqual(*text, "/mnt/games/SteamLibrary", "legacy path is a string value");
    } else {
        expect(false, "legacy path should be a string value");
    }
}

void testUserConfigWrapper()
{
    constexpr std::string_view kUserConfig = R"("UserLocalConfigStore"
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
				}
				"Favorites"
				{
					"440"		"1"
				}
			}
		}
	}
}
)";
    iideck::vdf::ParseError error;
    const auto doc = iideck::vdf::parse(kUserConfig, error);
    expect(doc.has_value(), "user config parses");

    // The whole document is wrapped, so the paths start one level in.
    expect(doc->integer({"UserLocalConfigStore", "Software", "Valve", "Steam", "Apps", "440", "Playtime"}) == 145,
        "playtime is read through the wrapper");
    expect(doc->has({"UserLocalConfigStore", "Software", "Valve", "Steam", "Favorites", "440"}),
        "favourite is found");
}

void testMissingPathIsAbsent()
{
    iideck::vdf::ParseError error;
    const auto doc = iideck::vdf::parse(kManifest, error);
    expect(doc.has_value(), "manifest parses");
    expect(!doc->str({"AppState", "nosuchkey"}).has_value(), "missing key is absent");
    expect(!doc->block({"AppState", "name"}).has_value(), "a string leaf is not a block");
}

void testMalformedIsRejected()
{
    iideck::vdf::ParseError error;
    expect(!iideck::vdf::parse(R"("root" { "a" "b")", error).has_value(), "unterminated block is rejected");
    expect(!iideck::vdf::parse(std::string_view{R"("root" { "unterminated )"}, error).has_value(), "unterminated string is rejected");
    expect(!iideck::vdf::parse(R"("root" { } })", error).has_value(), "stray close brace is rejected");
    expect(error.offset > 0, "the error reports an offset");
}

} // namespace

int main()
{
    testManifest();
    testCaseInsensitiveLookup();
    testQuotedEscapes();
    testBareTokens();
    testLibraryFoldersCurrent();
    testLibraryFoldersLegacy();
    testUserConfigWrapper();
    testMissingPathIsAbsent();
    testMalformedIsRejected();
    std::printf("vdf: all checks passed\n");
    return 0;
}