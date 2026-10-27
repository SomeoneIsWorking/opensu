// setting_schema — every layered setting declared once: its key, label, help line, type, choices,
// default, the layers it may be kept at, when it shows (a condition on the values that apply) and
// when a change takes effect. A key family (a core's options, read from the core) is declared by
// its prefix. Clearing a layer, validating a loaded file, the layered editor's rows and the count
// of what a scope keeps (`setting_scope`) are all read from the declarations.
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

#include "setting_choices.hpp"
#include "setting_layers.hpp"

namespace opensu::settings {

enum class SettingType : std::uint8_t {
    /// One of `choices`.
    Choice,
    /// "off" or "on".
    Toggle,
    /// A whole number in `range`.
    Slider,
    /// A path chosen on its own picker (the shader preset).
    Path,
    /// One of `choices`, each standing for several other settings' values.
    Preset,
};

/// When a change reaches a running game.
enum class Takes : std::uint8_t { Live, NextLaunch };

/// The layer kinds a setting may be kept at.
class LayerKinds {
  public:
    constexpr LayerKinds(std::initializer_list<Layer::Kind> kinds) noexcept {
        for (const Layer::Kind kind : kinds) {
            bits_ |= bit(kind);
        }
    }
    [[nodiscard]] static constexpr LayerKinds all() noexcept {
        return {Layer::Kind::Global, Layer::Kind::System, Layer::Kind::Emulator, Layer::Kind::Game};
    }
    [[nodiscard]] constexpr bool has(Layer::Kind kind) const noexcept {
        return (bits_ & bit(kind)) != 0;
    }

  private:
    [[nodiscard]] static constexpr std::uint8_t bit(Layer::Kind kind) noexcept {
        return static_cast<std::uint8_t>(1U << static_cast<unsigned>(kind));
    }
    std::uint8_t bits_{0};
};

/// A choice as it is kept and as the player reads it.
struct SettingChoice {
    std::string key;
    std::string label;

    bool operator==(const SettingChoice&) const = default;
};

/// What a condition reads: a setting's value as it applies where the editor looks.
using SettingLookup = std::function<std::string(std::string_view key)>;

struct Setting {
    struct Range {
        int low{};
        int high{};
        int step{1};
    };

    std::string key;
    std::string label;
    /// One line saying what it does.
    std::string help;
    /// The heading it stands under.
    std::string section;
    SettingType type{SettingType::Choice};
    /// A choice's, a preset's or a toggle's values in order.
    std::vector<SettingChoice> choices;
    /// A slider's.
    Range range;
    /// What follows a slider's number ("%").
    std::string unit;
    /// The built-in default as kept.
    std::string fallback;
    /// How the default reads when it is none of `choices` (the emulator pick's "Automatic").
    std::string fallbackLabel;
    LayerKinds layers{LayerKinds::all()};
    /// Whether it shows, read on the values that apply; empty for always.
    std::function<bool(const SettingLookup&)> shownWhen;
    Takes takes{Takes::Live};

    /// `value` as the player reads it: its choice's label, a slider's number and unit, else itself.
    [[nodiscard]] std::string labelOf(std::string_view value) const;
    /// Whether `value` may be kept: one of its choices, a number in its range, any path.
    [[nodiscard]] bool accepts(std::string_view value) const;
};

/// The setting `choices` keeps: its key, label, values and default from the table.
[[nodiscard]] Setting choiceSetting(const ChoiceKey& choices, std::string help, std::string section,
                                    SettingType type = SettingType::Choice);

class Schema {
  public:
    Schema& add(Setting setting);
    /// Keys starting with `prefix` belong to the schema with any value: settings read from
    /// elsewhere at run time (a core's options), declared one by one with `add` where known.
    Schema& addFamily(std::string prefix);
    /// A key that belongs to the schema with any non-empty value: one whose choices depend on
    /// where it is set (the emulator pick), declared with `add` where they are known.
    Schema& addOpen(std::string key);

    [[nodiscard]] const Setting* find(std::string_view key) const;
    [[nodiscard]] const std::vector<Setting>& settings() const noexcept {
        return settings_;
    }
    /// Whether `key` is declared, alone or by its family.
    [[nodiscard]] bool owns(std::string_view key) const;
    /// Whether `value` may be kept under `key`: a declared setting's own rule; a family's key
    /// takes any non-empty value.
    [[nodiscard]] bool accepts(std::string_view key, std::string_view value) const;

    /// Deletes every key it owns from `layer`; how many went.
    std::size_t clear(SettingLayers& layers, const Layer& layer) const;
    /// Drops every kept value it does not own or would not accept, each reported naming `where`;
    /// how many went.
    std::size_t validate(SettingLayers& layers, std::string_view where) const;

  private:
    std::vector<Setting> settings_;
    std::vector<std::string> families_;
    std::vector<std::string> open_;
};

} // namespace opensu::settings
