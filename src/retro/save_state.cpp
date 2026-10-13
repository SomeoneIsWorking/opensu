#include "save_state.hpp"

#include <fstream>
#include <iterator>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "fileio/atomic_write.hpp"
#include "lucent/log.h"

namespace opensu::retro {

SaveState::SaveState(std::filesystem::path file) : file_{std::move(file)} {
    std::error_code error;
    exists_ = std::filesystem::is_regular_file(file_, error);
}

StateOutcome SaveState::save(Core& core) {
    const std::vector<std::byte> state = core.serialize();
    if (state.empty()) {
        return {StateResult::Refused, core.name() + " cannot save states"};
    }
    std::string error;
    const std::string_view bytes{reinterpret_cast<const char*>(state.data()), state.size()};
    if (!fileio::writeWhole(file_, bytes, error)) {
        lucent::error("retro", "state not written: {}", error);
        return {StateResult::FileFailed, "the state was not written: " + error};
    }
    exists_ = true;
    lucent::info("retro", "saved {} bytes of state to {}", state.size(), file_.string());
    return {};
}

StateOutcome SaveState::load(Core& core) {
    std::ifstream in{file_, std::ios::binary};
    if (!in) {
        std::error_code error;
        exists_ = std::filesystem::exists(file_, error);
        return exists_ ? StateOutcome{StateResult::FileFailed, "cannot read " + file_.string()}
                       : StateOutcome{StateResult::Missing, "no state is saved for this game"};
    }
    const std::vector<char> bytes{std::istreambuf_iterator<char>{in},
                                  std::istreambuf_iterator<char>{}};
    const std::span<const std::byte> state{reinterpret_cast<const std::byte*>(bytes.data()),
                                           bytes.size()};
    if (!core.unserialize(state)) {
        return {StateResult::Refused, core.name() + " refused the saved state"};
    }
    lucent::info("retro", "loaded state from {}", file_.string());
    return {};
}

} // namespace opensu::retro
