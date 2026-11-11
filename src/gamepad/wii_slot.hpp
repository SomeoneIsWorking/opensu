// wii_slot — one slot of a DolphinBar in its Wii Remote mode (a hidraw node of 057e:0306), read as
// the remote synced to it, in the remote's own reports (Wiibrew's protocol). A status request asks
// whether a remote is there: the bar refuses the write (EPIPE) for an empty slot, and a remote
// answers with its status, which says whether an extension is plugged in. An extension is set up
// unencrypted (0x55 to 0xa400f0, 0x00 to 0xa400fb) and named by the id at 0xa400fa; the buttons
// then come in report 0x30, with a Nunchuk's or Classic Controller's six bytes in 0x32. What it
// reads comes out as the events hid-wiimote makes for a Bluetooth remote's parts, so
// `WiiRemoteLayout` reads both alike. The slot's player light is lit when its remote answers. The
// camera, the accelerometer and Motion Plus are not turned on, and nothing else of the remote's
// is written.
#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "pad_translator.hpp"
#include "wii_remote_layout.hpp"

namespace opensu::gamepad {

/// One of hid-wiimote's events and the part's device it would come from.
struct WiiPartEvent {
    WiiRemotePart part{WiiRemotePart::Remote};
    PadEvent event;

    bool operator==(const WiiPartEvent&) const = default;
};

class WiiSlot {
  public:
    /// Opens `node`, slot `index` (0 to 3) of its bar, for reading and writing, without blocking.
    /// Throws std::system_error when it cannot, or when it is not a character device.
    WiiSlot(const std::filesystem::path& node, int index);
    /// Speaks over `fd`, which it then owns, as the bar's node `node`.
    WiiSlot(int fd, std::filesystem::path node, int index);
    WiiSlot(WiiSlot&& other) noexcept;
    WiiSlot& operator=(WiiSlot&& other) noexcept;
    WiiSlot(const WiiSlot&) = delete;
    WiiSlot& operator=(const WiiSlot&) = delete;
    ~WiiSlot();

    [[nodiscard]] int fd() const noexcept {
        return fd_;
    }
    [[nodiscard]] const std::filesystem::path& node() const noexcept {
        return node_;
    }
    /// The remote's name, as hid-wiimote names a Bluetooth one.
    [[nodiscard]] const std::string& name() const noexcept {
        return name_;
    }

    /// Asks whether a remote is there; its status report answers. False when the slot refuses
    /// the request: no remote, or the bar is gone. A remote that was there has then left.
    bool ask();

    /// Reads every report waiting, appending what changed as hid-wiimote's events. False once
    /// the node is gone.
    bool read(std::vector<WiiPartEvent>& out);

    /// Whether a remote has answered and not left since.
    [[nodiscard]] bool remote() const noexcept {
        return remote_;
    }
    /// The extension being read: a Nunchuk or a Classic Controller, once named; nothing for
    /// none, or one of another kind.
    [[nodiscard]] std::optional<WiiRemotePart> extension() const noexcept {
        return extension_;
    }
    /// What hid-wiimote's device for `extension` reports: its buttons and its axes' ranges.
    [[nodiscard]] static PadCapabilities capabilitiesOf(WiiRemotePart extension);
    /// The keys held and the axes now, as events.
    [[nodiscard]] std::vector<WiiPartEvent> state() const;

  private:
    /// Where an extension's setup is: each step waits for the remote's answer to the last.
    enum class Setup : std::uint8_t {
        None,
        Unlocking,
        Unencrypting,
        Naming,
    };

    void takeReport(std::span<const std::uint8_t> report, std::vector<WiiPartEvent>& out);
    void takeStatus(std::span<const std::uint8_t> report);
    void takeAck(std::span<const std::uint8_t> report);
    void takeName(std::span<const std::uint8_t> report);
    void takeButtons(std::uint8_t first, std::uint8_t second, std::vector<WiiPartEvent>& out);
    void takeExtension(WiiRemotePart part, std::span<const std::uint8_t> bytes,
                       std::vector<WiiPartEvent>& out);
    /// Sets one of the extension's registers (report 0x16).
    void writeRegister(std::uint32_t address, std::uint8_t value);
    /// The report mode for what is plugged in: buttons, and the extension's bytes with one.
    void requestReports();
    bool send(std::span<const std::uint8_t> report);
    void key(WiiRemotePart part, std::uint16_t code, bool down, std::vector<WiiPartEvent>& out);
    void axis(WiiRemotePart part, std::uint16_t code, std::int32_t value,
              std::vector<WiiPartEvent>& out);
    /// Emits `event` from `part` when its code's value changed.
    void report(WiiRemotePart part, const PadEvent& event, std::vector<WiiPartEvent>& out);

    int fd_{-1};
    std::filesystem::path node_;
    std::string name_{"Nintendo Wii Remote"};
    int index_{0};
    bool remote_{false};
    /// The status says an extension is plugged in.
    bool plugged_{false};
    Setup setup_{Setup::None};
    std::optional<WiiRemotePart> extension_;
    /// The value of every key and axis reported, by part and code.
    std::vector<WiiPartEvent> now_;
};

} // namespace opensu::gamepad
