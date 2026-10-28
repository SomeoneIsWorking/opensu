// layered_settings — what the layered editor edits, for whichever scope it was opened on: a game
// (its Global, console, core and own scopes), Global, a console or a built-in core. Builds the
// schema that applies there (the emulator pick, video on a built-in core, frame generation and
// resolution and upscaling where the platform has them, the core's options from the running core or
// the last run), reads each scope's rows from it, and keeps every change: set at a scope, removed
// (remembered for this run so it can be restored), or a scope cleared. Each change is saved and
// reaches the running game at once: video and core options through the runner, frame generation and
// upscaling through `GamePresentation`, an emulator pick and a resolution at the next launch.
#pragma once

#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core_option_store.hpp"
#include "game_presentation.hpp"
#include "game_settings.hpp"
#include "library/emulators.hpp"
#include "library/game.hpp"
#include "library/shader_pack.hpp"
#include "retro_session.hpp"
#include "settings/setting_schema.hpp"
#include "settings/setting_scope.hpp"

namespace opensu::app {

/// Where the editor was opened: the layers its rows resolve through and the scopes it shows.
struct EditorContext {
    settings::LayerKeys keys;
    /// The scopes in the order the tabs show them; the last is the one opened on.
    std::vector<settings::Layer> scopes;
    /// The game it was opened for; empty for Global, a console or a core.
    std::string gameId;
};

class LayeredSettings {
  public:
    struct Hooks {
        /// The game `id` as the library has it now, or nothing when it is gone.
        std::function<std::optional<library::Game>(const std::string& id)> game;
        /// The game running now, or null.
        std::function<const library::Game*()> running;
        /// An emulator pick changed: the library applies it.
        std::function<void()> picksChanged;
        /// A value changed and was saved: the pages showing settings show it again.
        std::function<void()> changed;
        /// Tells the player something; `error` marks a refusal.
        std::function<void(const std::string& text, bool error)> say;
    };

    struct Sources {
        GameSettings& gameSettings;
        std::function<const library::roms::Emulators&()> found;
        const library::roms::ShaderPack& shaders;
        GamePresentation& presentation;
        RetroSession& retro;
        CoreOptionStore coreOptions;
    };

    LayeredSettings(Sources sources, Hooks hooks);

    [[nodiscard]] EditorContext forGame(const library::Game& game) const;
    [[nodiscard]] EditorContext forGlobal() const;
    [[nodiscard]] EditorContext forSystem(const std::string& system) const;
    [[nodiscard]] EditorContext forCore(const std::string& core) const;

    /// Whether `game` has anything to set.
    [[nodiscard]] bool offers(const library::Game& game) const;
    /// The headings `game`'s own rows stand under, joined: "Video, Frame generation, Core
    /// options"; empty when it has nothing to set.
    [[nodiscard]] std::string sectionsOf(const library::Game& game) const;
    /// How many settings `game` keeps of its own, as its scope's rows show them.
    [[nodiscard]] std::size_t kept(const library::Game& game) const;
    /// How many settings the context's own scope keeps, as its rows show them.
    [[nodiscard]] std::size_t kept(const EditorContext& context) const;

    /// What may be set in `context`.
    [[nodiscard]] settings::Schema schemaOf(const EditorContext& context) const;
    /// The rows of `scope` in `context`.
    [[nodiscard]] std::vector<settings::ScopedValue> rows(const settings::Schema& schema,
                                                          const EditorContext& context,
                                                          const settings::Layer& scope) const;
    /// A scope's name: "Global", "Game Boy Advance", "mGBA", "This game".
    [[nodiscard]] std::string scopeName(const settings::Layer& scope) const;
    /// The game the context was opened for, as the library has it now.
    [[nodiscard]] std::optional<library::Game> gameOf(const EditorContext& context) const;

    /// Keeps `value` for `key` at `scope`.
    void set(const EditorContext& context, const settings::Layer& scope, const std::string& key,
             const std::string& value);
    /// Removes `scope`'s own value for `key`, remembering it for `restore`.
    void remove(const settings::Layer& scope, const std::string& key);
    /// The value `remove` took from `scope` for `key` this run, or null.
    [[nodiscard]] const std::string* removed(const settings::Layer& scope,
                                             const std::string& key) const;
    /// Puts back what `remove` took.
    void restore(const settings::Layer& scope, const std::string& key);
    /// Removes everything `scope` keeps; how many went.
    std::size_t clear(const settings::Layer& scope);

    [[nodiscard]] const settings::SettingLayers& layersOf() const noexcept {
        return gameSettings_.layers();
    }
    /// The shader presets the picker lists.
    [[nodiscard]] const library::roms::ShaderPack& shaders() const noexcept {
        return shaders_;
    }
    /// The summary over `section`'s rows of what the values `applies` reads come to, with its
    /// note; nothing for a section without one. `where` names them in a report.
    [[nodiscard]] std::optional<PresentationSummary>
    summaryOf(std::string_view section, const settings::SettingLookup& applies,
              const std::string& where) const;

  private:
    /// The running core's options when `core` runs now, else those it defined when last run.
    [[nodiscard]] std::vector<settings::CoreOptionDefinition>
    coreOptionsOf(const std::string& core) const;
    /// The pick's default for `system`: "Automatic: mGBA", what the search found.
    [[nodiscard]] std::string automaticLabel(const std::string& system) const;
    /// The emulator pick for `context`, or nothing where there is none to make.
    [[nodiscard]] std::optional<settings::Setting>
    emulatorPickOf(const EditorContext& context) const;
    /// Saves, tells the pages, and applies the layers to the running game.
    void changed(const std::string& key);
    /// Puts the layers' video and core options into the running game.
    void applyToRunning();

    GameSettings& gameSettings_;
    std::function<const library::roms::Emulators&()> found_;
    const library::roms::ShaderPack& shaders_;
    GamePresentation& presentation_;
    RetroSession& retro_;
    CoreOptionStore coreOptions_;
    Hooks hooks_;
    /// Each core's options as last read, from the running core or the store.
    mutable std::map<std::string, std::vector<settings::CoreOptionDefinition>, std::less<>>
        coreOptionsKnown_;
    /// What `remove` took this run, by scope and key.
    std::map<std::pair<settings::Layer, std::string>, std::string> removed_;
};

} // namespace opensu::app
