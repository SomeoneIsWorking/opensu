#include "setting_row.hpp"

#include <utility>

namespace opensu::ui {

SettingsRow makeRow(std::string id, RowKind kind, std::string label, std::string note,
                    std::string value) {
    SettingsRow row;
    row.id = std::move(id);
    row.kind = kind;
    row.label = std::move(label);
    row.note = std::move(note);
    row.value = std::move(value);
    return row;
}

SettingsRow makeToggle(std::string id, std::string label, std::string note, bool on) {
    SettingsRow row =
        makeRow(std::move(id), RowKind::Toggle, std::move(label), std::move(note), {});
    row.on = on;
    return row;
}

SettingsRow makeSlider(std::string id, std::string label, std::string note, SliderRange range) {
    SettingsRow row =
        makeRow(std::move(id), RowKind::Slider, std::move(label), std::move(note), {});
    row.level = range.level;
    row.low = range.low;
    row.high = range.high;
    row.step = range.step;
    return row;
}

SettingsRow makeChoice(std::string id, std::string label, std::string note, Options options) {
    std::string value =
        options.chosen < options.labels.size() ? options.labels[options.chosen] : std::string{};
    SettingsRow row = makeRow(std::move(id), RowKind::Choice, std::move(label), std::move(note),
                              std::move(value));
    row.choices = std::move(options.labels);
    row.chosen = options.chosen;
    return row;
}

SettingsRow makeHeading(std::string label) {
    return makeRow({}, RowKind::Heading, std::move(label), {}, {});
}

SettingsRow makePage(std::string id, std::string label, std::string note, std::string value) {
    return makeRow(std::move(id), RowKind::Page, std::move(label), std::move(note),
                   std::move(value));
}

} // namespace opensu::ui
