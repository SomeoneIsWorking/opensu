// Frame generation and scaling through their one owner, against a recording output: each part
// follows its own game, writes only what changed, a value change reaches the game at once except a
// resolution, and with no game Gamescope's defaults come back.
#include "game_presentation.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "settings/framegen_settings.hpp"
#include "settings/game_settings_store.hpp"
#include "settings/scaling_settings.hpp"
#include "settings/setting_schema.hpp"
#include "support/scratch_dir.hpp"

namespace {

namespace fs = std::filesystem;
using namespace opensu;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

/// Every write, in order: `generate`, `stop`, `resize WxH`, `upscale <filter> <sharpness>`.
class RecordingOutput final : public app::PresentationOutput {
  public:
    explicit RecordingOutput(std::vector<std::string>& writes) : writes_{writes} {
    }

    void generate(const app::FrameGenRequest& request) override {
        writes_.push_back("generate " + request.dll.string());
    }
    void stopGenerating() override {
        writes_.emplace_back("stop");
    }
    void resize(settings::ScreenSize size) override {
        writes_.push_back("resize " + std::to_string(size.width) + "x" +
                          std::to_string(size.height));
    }
    void upscale(const app::Upscale& upscale) override {
        writes_.push_back(
            "upscale " +
            std::string{
                settings::scalingFilterChoices[static_cast<std::size_t>(upscale.filter)].key} +
            " " + std::to_string(upscale.sharpness));
    }

  private:
    std::vector<std::string>& writes_;
};

library::Game gameNamed(const std::string& id) {
    library::Game game;
    game.id = id;
    game.title = id;
    return game;
}

void followsAndWritesChangesOnly(const fs::path& root) {
    app::GameSettings gameSettings{settings::GameSettingsStore{root / "game-settings.json"},
                                   root / "settings.json", [](const std::string&) {
                                   }};
    settings::SettingLayers& layers = gameSettings.layers();
    const settings::Layer one{settings::Layer::Kind::Game, "one"};
    layers.set(one, "framegen.preset", "smooth-60");
    layers.set(one, settings::scalingResolutionKey, "50");
    layers.set(one, settings::scalingFilterKey.key, "fsr");

    std::vector<std::string> writes;
    std::optional<fs::path> dll{"/steam/Lossless.dll"};
    app::GamePresentation presentation{gameSettings, std::make_unique<RecordingOutput>(writes),
                                       [&dll] {
                                           return dll;
                                       }};
    expect(presentation.active(), "an output makes it active");

    presentation.follow(nullptr, nullptr);
    expect(writes == std::vector<std::string>{"stop"},
           "frame generation is switched off once; scaling waits for the screen");
    writes.clear();

    presentation.setScreen(settings::ScreenSize{1920, 1080});
    const library::Game game = gameNamed("one");
    presentation.follow(nullptr, &game);
    expect(writes == std::vector<std::string>{"resize 960x540", "upscale fsr 2"},
           "a launching game gets its size and filter before it is in front");
    writes.clear();

    presentation.follow(&game, &game);
    expect(writes == std::vector<std::string>{"generate /steam/Lossless.dll"},
           "frame generation starts when the game comes in front");
    writes.clear();

    presentation.follow(&game, &game);
    presentation.changed();
    expect(writes.empty(), "nothing is written again while nothing changed");

    layers.set(one, settings::scalingResolutionKey, "75");
    layers.set(one, settings::scalingFilterKey.key, "nis");
    layers.set(one, "framegen.preset", "off");
    presentation.changed();
    expect(writes == std::vector<std::string>{"stop", "upscale nis 2"},
           "frame generation and the filter change at once, the resolution at the next launch");
    writes.clear();

    presentation.follow(nullptr, nullptr);
    expect(writes == std::vector<std::string>{"resize 1920x1080", "upscale linear 2"},
           "the screen and Gamescope's defaults come back when the game is over");
}

void summariesNoteTheSession(const fs::path& root) {
    app::GameSettings gameSettings{settings::GameSettingsStore{root / "outside.json"},
                                   root / "settings.json", [](const std::string&) {
                                   }};
    const app::GamePresentation presentation{gameSettings, nullptr, [] {
                                                 return std::optional<fs::path>{};
                                             }};
    expect(!presentation.active(), "no output, not active");
    const settings::SettingLookup none = [](std::string_view) {
        return std::string{};
    };
    const app::PresentationSummary frameGen = presentation.frameGenSummary(none, "Global");
    expect(frameGen.id == "framegen.summary" &&
               frameGen.note == "Needs Lossless Scaling, installed through Steam · applies in "
                                "openSU's Gamescope session",
           "frame generation's note names what it needs and where it applies");
    const app::PresentationSummary scaling = presentation.scalingSummary(none, "Global");
    expect(scaling.id == "scaling.summary" &&
               scaling.note.ends_with(" · applies in openSU's Gamescope session"),
           "scaling's note says where it applies");
}

} // namespace

int main() {
    const test::ScratchDir scratch{OPENSU_TEST_SCRATCH, "game_presentation"};
    followsAndWritesChangesOnly(scratch.path());
    summariesNoteTheSession(scratch.path());
    std::puts("game_presentation: ok");
    return 0;
}
