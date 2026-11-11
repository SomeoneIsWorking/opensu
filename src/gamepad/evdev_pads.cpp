#include "evdev_pads.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <ranges>
#include <system_error>
#include <utility>

#include <poll.h>
#include <sys/eventfd.h>
#include <sys/inotify.h>
#include <unistd.h>

#include <linux/input-event-codes.h>

#include "lucent/log.h"

namespace opensu::gamepad {
namespace {

/// Owners of a kept pad nobody sits at: `kept_` and `seats_`.
constexpr long unseated = 2;
/// How often the bars' slots are asked whether a remote is there.
constexpr std::chrono::seconds askEvery{1};

std::system_error lastError(const char* what) {
    return std::system_error{errno, std::generic_category(), what};
}

} // namespace

EvdevPads::Pad::Pad(EvdevDevice opened)
    : device{std::move(opened)}, layout{layoutNow()}, translator{capabilities()} {
    // The translator starts from the pad's present state, so the first resume is right.
    restart({}, {});
}

EvdevPads::Pad::Pad(WiiSlot& remote)
    : slot{&remote}, layout{layoutNow()}, translator{capabilities()} {
    restart({}, {});
}

std::string EvdevPads::Pad::id() const {
    return (device ? device->node() : slot->node()).string();
}

const std::string& EvdevPads::Pad::name() const {
    return slot != nullptr ? slot->name() : device.value().name();
}

std::optional<WiiRemoteLayout> EvdevPads::Pad::layoutNow() const {
    if (slot != nullptr) {
        const std::optional<WiiRemotePart> plugged = slot->extension();
        return plugged ? WiiRemoteLayout{*plugged, WiiSlot::capabilitiesOf(*plugged)}
                       : WiiRemoteLayout{};
    }
    if (!device.value().wiiRemotePart()) {
        return std::nullopt;
    }
    if (extension) {
        return WiiRemoteLayout{extension->wiiRemotePart().value(), extension->capabilities()};
    }
    return WiiRemoteLayout{};
}

PadCapabilities EvdevPads::Pad::capabilities() const {
    return layout ? layout->capabilities() : device.value().capabilities();
}

std::vector<PadEvent> EvdevPads::Pad::translatable(const std::vector<PadEvent>& read,
                                                   WiiRemotePart part) const {
    if (!layout) {
        return read;
    }
    std::vector<PadEvent> mapped;
    for (const PadEvent& event : read) {
        if (const std::optional<PadEvent> to = layout->map(part, event)) {
            mapped.push_back(*to);
        }
    }
    return mapped;
}

std::vector<PadEvent> EvdevPads::Pad::translatable(const std::vector<WiiPartEvent>& read) const {
    std::vector<PadEvent> mapped;
    for (const WiiPartEvent& event : read) {
        if (const std::optional<PadEvent> to = layout.value().map(event.part, event.event)) {
            mapped.push_back(*to);
        }
    }
    return mapped;
}

void EvdevPads::Pad::restart(const ChordTriggers& chords, const ForwardMap& map) {
    translator = PadTranslator{capabilities()};
    translator.setChords(chords);
    translator.setForwardMap(map);
    std::vector<PadEvent> ignored;
    std::vector<Event> controls;
    const auto feed = [&](const std::vector<PadEvent>& events) {
        for (const PadEvent& event : events) {
            translator.translate(event, ignored, controls, ChordGate::Clock::now());
        }
    };
    if (slot != nullptr) {
        feed(translatable(slot->state()));
        return;
    }
    feed(translatable(device.value().state(), WiiRemotePart::Remote));
    if (extension) {
        feed(translatable(extension->state(), extension->wiiRemotePart().value()));
    }
}

EvdevPads::EvdevPads(std::filesystem::path inputDir, std::optional<device::HidPlaces> barSlots,
                     OwnVirtualPads own)
    : inputDir_{std::move(inputDir)}, barSlots_{std::move(barSlots)}, own_{own} {
    wake_ = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (wake_ < 0) {
        throw lastError("eventfd");
    }
    watch_ = inotify_init1(IN_CLOEXEC | IN_NONBLOCK);
    if (watch_ < 0) {
        ::close(wake_);
        throw lastError("inotify_init1");
    }
    // udev creates a node, then grants access to it: a pad becomes readable on the attribute
    // change, not the creation.
    inputWatch_ = inotify_add_watch(watch_, inputDir_.c_str(), IN_CREATE | IN_ATTRIB);
    if (inputWatch_ < 0) {
        const std::system_error failure = lastError("inotify_add_watch");
        ::close(watch_);
        ::close(wake_);
        throw failure;
    }
    if (barSlots_) {
        hidWatch_ = inotify_add_watch(watch_, barSlots_->dev.c_str(), IN_CREATE | IN_ATTRIB);
        if (hidWatch_ < 0) {
            lucent::info("gamepad", "DolphinBars plugged in later are not seen: {} is not watched",
                         barSlots_->dev.string());
        }
    }
    scan();
    openSlots();
    thread_ = std::jthread{[this](const std::stop_token& stop) {
        run(stop);
    }};
}

EvdevPads::~EvdevPads() {
    thread_.request_stop();
    wake();
    thread_.join();
    ::close(watch_);
    ::close(wake_);
}

std::vector<Event> EvdevPads::takeEvents() {
    const std::lock_guard lock{mutex_};
    return std::exchange(events_, {});
}

std::map<std::string, PadAxes> EvdevPads::axes() const {
    const std::lock_guard lock{mutex_};
    return axes_;
}

std::vector<PadInfo> EvdevPads::connected() const {
    const std::lock_guard lock{mutex_};
    return connected_;
}

std::optional<device::BatteryStatus> EvdevPads::battery(const PadInfo& pad) const {
    return batteries_.read(pad.uniq);
}

HeldPads EvdevPads::hold(ForwardMap map, WiiRemoteHold remotes) {
    std::unique_lock lock{mutex_};
    wanted_.map = std::move(map);
    wanted_.remotes = remotes;
    wanted_.blocked = false;
    request(lock, Claim::Game);
    if (appliedMissed_) {
        return {};
    }
    return {.environment = {"SDL_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT=0x045e/0x028e"},
            .padGuid = VirtualPad::sdlGuid()};
}

void EvdevPads::release(Claim after) {
    std::unique_lock lock{mutex_};
    wanted_.map = {};
    wanted_.remotes = WiiRemoteHold::AsPads;
    wanted_.blocked = false;
    request(lock, after);
}

void EvdevPads::claim(Claim claim) {
    std::unique_lock lock{mutex_};
    request(lock, claim);
}

void EvdevPads::request(std::unique_lock<std::mutex>& lock, Claim claim) {
    wanted_.claim = claim;
    awaitApplied(lock, ++wanted_.serial);
}

void EvdevPads::awaitApplied(std::unique_lock<std::mutex>& lock, std::uint64_t serial) {
    wake();
    appliedChanged_.wait(lock, [this, serial] {
        return appliedSerial_ >= serial;
    });
}

void EvdevPads::setBlocked(bool blocked) {
    const std::lock_guard lock{mutex_};
    wanted_.blocked = blocked;
    ++wanted_.serial;
    wake();
}

void EvdevPads::setChords(ChordTriggers chords) {
    std::unique_lock lock{mutex_};
    wanted_.chords = std::move(chords);
    awaitApplied(lock, ++wanted_.serial);
}

void EvdevPads::setWiiPointerRange(const WiiPointerRange& range) {
    const std::lock_guard lock{mutex_};
    wanted_.pointer = range;
    ++wanted_.serial;
    wake();
}

std::shared_ptr<VirtualPad> EvdevPads::lendPad() {
    std::unique_lock lock{mutex_};
    wanted_.lent = true;
    awaitApplied(lock, ++wanted_.serial);
    return lentPad_;
}

void EvdevPads::returnPad() {
    std::unique_lock lock{mutex_};
    wanted_.lent = false;
    awaitApplied(lock, ++wanted_.serial);
    lentPad_.reset();
}

std::vector<std::filesystem::path> EvdevPads::keepVirtualPads(std::size_t count) {
    std::unique_lock lock{mutex_};
    wanted_.keep = count;
    awaitApplied(lock, ++wanted_.serial);
    return keptNodes_;
}

std::shared_ptr<VirtualPad> EvdevPads::seatPad() {
    const std::lock_guard lock{mutex_};
    // From the last: controllers take the first.
    for (const auto& seat : std::views::reverse(seats_)) {
        if (seat.use_count() == unseated) {
            return seat;
        }
    }
    return nullptr;
}

void EvdevPads::keep(std::size_t count) {
    while (kept_.size() > count) {
        const VirtualPad* going = kept_.back().get();
        for (const auto& pad : pads_) {
            if (pad->output == going) {
                pad->output = nullptr;
                pad->live = false;
            }
        }
        kept_.pop_back();
    }
    while (kept_.size() < count) {
        try {
            kept_.push_back(std::make_unique<VirtualPad>());
        } catch (const std::system_error& failure) {
            lucent::error("gamepad", "cannot keep a virtual pad: {}", failure.what());
            break;
        }
    }
    std::vector<std::filesystem::path> nodes;
    std::string names;
    for (const auto& pad : kept_) {
        nodes.push_back(pad->node());
        names += (names.empty() ? "" : " ") + pad->node().string();
    }
    lucent::info("gamepad", "{} virtual pads kept: {}", kept_.size(), names);
    const std::lock_guard lock{mutex_};
    keptNodes_ = std::move(nodes);
    seats_ = kept_;
}

VirtualPad* EvdevPads::freeKept() const {
    for (const auto& kept : kept_) {
        const bool driven = std::ranges::any_of(pads_, [&kept](const auto& pad) {
            return pad->output == kept.get();
        });
        if (!driven && kept.use_count() == unseated) {
            return kept.get();
        }
    }
    return nullptr;
}

void EvdevPads::wake() const {
    const std::uint64_t one = 1;
    [[maybe_unused]] const auto written = ::write(wake_, &one, sizeof(one));
}

void EvdevPads::scan() {
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator{inputDir_, error}) {
        add(entry.path());
    }
}

void EvdevPads::add(const std::filesystem::path& node) {
    if (!node.filename().string().starts_with("event")) {
        return;
    }
    for (const auto& pad : pads_) {
        if ((pad->device && pad->device->node() == node) ||
            (pad->extension && pad->extension->node() == node) ||
            (pad->camera && pad->camera->node() == node)) {
            return;
        }
    }
    if (std::ranges::any_of(loose_, [&node](const EvdevDevice& held) {
            return held.node() == node;
        })) {
        return;
    }
    if (applied_.remotes == WiiRemoteHold::ToGame && EvdevDevice::wiiRemotePartAt(node)) {
        return;
    }
    std::optional<EvdevDevice> device = EvdevDevice::openPad(node, own_);
    if (!device) {
        return;
    }
    if (const std::optional<WiiRemotePart> part = device->wiiRemotePart();
        part && *part != WiiRemotePart::Remote) {
        addPart(std::move(*device));
        return;
    }
    connect(std::make_unique<Pad>(std::move(*device)));
}

void EvdevPads::connect(std::unique_ptr<Pad> pad) {
    pad->translator.setChords(applied_.chords);
    pad->translator.setForwardMap(applied_.map);
    pad->pointer.setRange(applied_.pointer);
    lucent::info("gamepad", "controller connected: {} ({})", pad->name(), pad->id());
    if (!apply(*pad, applied_)) {
        missed_ = true;
    }
    std::erase_if(loose_, [this, &pad](EvdevDevice& part) {
        return attach(*pad, part);
    });
    const PadInfo info{pad->id(), pad->name(), pad->device ? pad->device->uniq() : ""};
    std::vector<Event> events{
        Event{.kind = Event::Kind::Connected, .device = info.name, .source = info.id}};
    {
        const std::lock_guard lock{mutex_};
        connected_.push_back(info);
        axes_[info.id] = pad->translator.axes();
    }
    publish(events);
    pads_.push_back(std::move(pad));
}

void EvdevPads::addPart(EvdevDevice part) {
    for (const auto& pad : pads_) {
        if (attach(*pad, part)) {
            return;
        }
    }
    lucent::info("gamepad", "{} ({}) waits for its Wii Remote", part.name(), part.node().string());
    loose_.push_back(std::move(part));
}

bool EvdevPads::attach(Pad& pad, EvdevDevice& part) {
    if (!pad.layout || !pad.device || part.parent().empty() ||
        pad.device->parent() != part.parent()) {
        return false;
    }
    const std::optional<WiiRemotePart> kind = part.wiiRemotePart();
    if ((kind == WiiRemotePart::Nunchuk || kind == WiiRemotePart::Classic) && !pad.extension) {
        setExtension(pad, std::move(part));
        return true;
    }
    if (kind == WiiRemotePart::Camera && !pad.camera) {
        lucent::info("gamepad", "{} ({}) points with its camera", pad.name(), pad.id());
        pad.camera = std::move(part);
        // Dots that hold still after the open are never reported again; the camera's next report
        // says where it points.
        for (const PadEvent& event : pad.camera->state()) {
            if (event.type == EV_ABS) {
                static_cast<void>(pad.pointer.feed(event));
            }
        }
        if (pad.grabbed) {
            static_cast<void>(pad.camera->grab());
        }
        return true;
    }
    return false;
}

void EvdevPads::setExtension(Pad& pad, std::optional<EvdevDevice> extension) {
    pad.extension = std::move(extension);
    if (pad.extension && pad.grabbed) {
        static_cast<void>(pad.extension->grab());
    }
    relayout(pad);
}

void EvdevPads::relayout(Pad& pad) {
    if (pad.live) {
        write(pad, pad.translator.rest());
    }
    pad.layout = pad.layoutNow();
    pad.restart(applied_.chords, applied_.map);
    if (pad.live) {
        write(pad, pad.translator.resume());
    }
    const std::optional<WiiRemotePart> extension = pad.layout.value().extension();
    lucent::info("gamepad", "{} ({}) {}", pad.name(), pad.id(),
                 extension == WiiRemotePart::Classic   ? "read as its Classic Controller"
                 : extension == WiiRemotePart::Nunchuk ? "held upright with its Nunchuk"
                                                       : "held sideways");
    const std::lock_guard lock{mutex_};
    axes_[pad.id()] = pad.translator.axes();
}

void EvdevPads::openSlots() {
    if (!barSlots_ || applied_.remotes == WiiRemoteHold::ToGame) {
        return;
    }
    for (const device::BarSlot& found : device::findWiiRemotes(*barSlots_).barSlots) {
        if (std::ranges::any_of(slots_, [&found](const auto& open) {
                return open->node() == found.node;
            })) {
            continue;
        }
        try {
            slots_.push_back(std::make_unique<WiiSlot>(found.node, found.index));
        } catch (const std::system_error& failure) {
            if (refusedSlots_.insert(found.node).second) {
                lucent::info("gamepad", "DolphinBar slot {} is not read: {}", found.index + 1,
                             failure.what());
            }
            continue;
        }
        refusedSlots_.erase(found.node);
        lucent::info("gamepad", "DolphinBar slot {} ({}) opened", found.index + 1,
                     found.node.string());
        static_cast<void>(slots_.back()->ask());
    }
}

void EvdevPads::askSlots(std::vector<Event>& events) {
    asked_ = std::chrono::steady_clock::now();
    for (const auto& slot : slots_) {
        const bool had = slot->remote();
        if (slot->ask() || !had) {
            continue;
        }
        const auto pad = std::ranges::find(pads_, slot.get(), &Pad::slot);
        if (pad != pads_.end()) {
            drop(static_cast<std::size_t>(pad - pads_.begin()), events);
        }
    }
}

EvdevPads::Pad* EvdevPads::padOf(const WiiSlot& slot) const {
    const auto pad = std::ranges::find(pads_, &slot, &Pad::slot);
    return pad != pads_.end() ? pad->get() : nullptr;
}

void EvdevPads::drop(std::size_t index, std::vector<Event>& events) {
    Pad& pad = *pads_[index];
    lucent::info("gamepad", "controller disconnected: {}", pad.name());
    if (pad.live) {
        write(pad, pad.translator.rest());
    }
    const std::string id = pad.id();
    if (pad.camera) {
        events.push_back(Event{
            .kind = Event::Kind::Pointer, .device = pad.name(), .source = id, .onScreen = false});
    }
    events.push_back(Event{.kind = Event::Kind::Disconnected, .device = pad.name(), .source = id});
    {
        const std::lock_guard lock{mutex_};
        std::erase_if(connected_, [&id](const PadInfo& info) {
            return info.id == id;
        });
        axes_.erase(id);
    }
    pads_.erase(pads_.begin() + static_cast<std::ptrdiff_t>(index));
}

void EvdevPads::letGoOfWiiRemotes() {
    std::vector<Event> events;
    for (std::size_t i = pads_.size(); i-- > 0;) {
        if (pads_[i]->layout) {
            lucent::info("gamepad", "{} ({}) is left to the game", pads_[i]->name(),
                         pads_[i]->id());
            drop(i, events);
        }
    }
    loose_.clear();
    if (!slots_.empty()) {
        lucent::info("gamepad", "{} DolphinBar slots are left to the game", slots_.size());
    }
    slots_.clear();
    publish(events);
}

bool EvdevPads::apply(Pad& pad, const Wanted& wanted) {
    const bool grab = wanted.claim == Claim::Shell || wanted.claim == Claim::Game;
    const bool virtualPad = wanted.claim == Claim::Game || wanted.claim == Claim::Away;
    bool held = true;
    if (grab && !pad.grabbed) {
        // Nothing grabs a hidraw node; the bar's slot is openSU's while it is open.
        pad.grabbed = !pad.device || pad.device->grab();
        held = pad.grabbed;
        for (std::optional<EvdevDevice>* part : {&pad.extension, &pad.camera}) {
            if (pad.grabbed && *part) {
                static_cast<void>((*part)->grab());
            }
        }
    } else if (!grab && pad.grabbed) {
        for (std::optional<EvdevDevice>* part : {&pad.device, &pad.extension, &pad.camera}) {
            if (*part) {
                (*part)->ungrab();
            }
        }
        pad.grabbed = false;
    }
    if (!virtualPad) {
        // A kept pad outlives this claim, so it must not keep the last press.
        if (pad.live) {
            write(pad, pad.translator.rest());
        }
        pad.output = nullptr;
        pad.made.reset();
        pad.live = false;
    } else if (pad.output == nullptr) {
        pad.output = freeKept();
        if (pad.output == nullptr) {
            try {
                pad.made = std::make_unique<VirtualPad>();
            } catch (const std::system_error& failure) {
                lucent::error("gamepad", "cannot give {} a virtual pad: {}", pad.name(),
                              failure.what());
                return false;
            }
            pad.output = pad.made.get();
        }
    }
    const bool lent = wanted.lent && !pads_.empty() && &pad == pads_.front().get();
    if (lent && pad.output != nullptr) {
        const auto kept = std::ranges::find(kept_, pad.output, &std::shared_ptr<VirtualPad>::get);
        const std::lock_guard lock{mutex_};
        lentPad_ = kept != kept_.end() ? *kept : pad.made;
    }
    const bool live = pad.output && wanted.claim == Claim::Game && !wanted.blocked && !lent;
    if (live != pad.live) {
        write(pad, live ? pad.translator.resume() : pad.translator.rest());
        pad.live = live;
    }
    lucent::debug("gamepad", "{}: grabbed {}, virtual pad {}", pad.name(), pad.grabbed,
                  pad.output != nullptr);
    return held;
}

void EvdevPads::write(Pad& pad, const std::vector<PadEvent>& events) {
    if (!pad.output) {
        return;
    }
    // A virtual pad that cannot be written is a uinput failure the game cannot recover from
    // either; the pad stays held and the game sees it stop.
    try {
        pad.output->write(events);
    } catch (const std::system_error& failure) {
        lucent::error("gamepad", "{}: {}", pad.name(), failure.what());
    }
}

void EvdevPads::publish(std::vector<Event>& events) {
    if (events.empty()) {
        return;
    }
    const std::lock_guard lock{mutex_};
    events_.insert(events_.end(), std::make_move_iterator(events.begin()),
                   std::make_move_iterator(events.end()));
}

void EvdevPads::run(const std::stop_token& stop) {
    while (!stop.stop_requested()) {
        std::vector<pollfd> fds;
        fds.push_back(pollfd{wake_, POLLIN, 0});
        fds.push_back(pollfd{watch_, POLLIN, 0});
        // What each polled descriptor after the first two is: a pad's device, extension or
        // camera, a part waiting for its remote, or a bar's slot.
        struct Polled {
            std::size_t index;
            enum class Kind : std::uint8_t { Device, Extension, Camera, Loose, Slot } kind;
        };
        std::vector<Polled> polled;
        for (std::size_t i = 0; i < pads_.size(); ++i) {
            if (pads_[i]->device) {
                fds.push_back(pollfd{pads_[i]->device->fd(), POLLIN, 0});
                polled.push_back({i, Polled::Kind::Device});
            }
            if (pads_[i]->extension) {
                fds.push_back(pollfd{pads_[i]->extension->fd(), POLLIN, 0});
                polled.push_back({i, Polled::Kind::Extension});
            }
            if (pads_[i]->camera) {
                fds.push_back(pollfd{pads_[i]->camera->fd(), POLLIN, 0});
                polled.push_back({i, Polled::Kind::Camera});
            }
        }
        for (std::size_t i = 0; i < loose_.size(); ++i) {
            fds.push_back(pollfd{loose_[i].fd(), POLLIN, 0});
            polled.push_back({i, Polled::Kind::Loose});
        }
        for (std::size_t i = 0; i < slots_.size(); ++i) {
            fds.push_back(pollfd{slots_[i]->fd(), POLLIN, 0});
            polled.push_back({i, Polled::Kind::Slot});
        }
        int timeout = -1;
        if (!slots_.empty()) {
            const auto due = asked_ + askEvery - std::chrono::steady_clock::now();
            timeout = static_cast<int>(std::max<std::chrono::milliseconds::rep>(
                0, std::chrono::duration_cast<std::chrono::milliseconds>(due).count()));
        }
        if (::poll(fds.data(), fds.size(), timeout) < 0 && errno != EINTR) {
            lucent::error("gamepad", "poll: {}", std::generic_category().message(errno));
            // Nothing is read from here on; a waiting hold() is answered with nothing held.
            const std::lock_guard lock{mutex_};
            appliedSerial_ = UINT64_MAX;
            appliedMissed_ = true;
            appliedChanged_.notify_all();
            return;
        }
        std::uint64_t count = 0;
        [[maybe_unused]] const auto drained = ::read(wake_, &count, sizeof(count));

        Wanted wanted;
        {
            const std::lock_guard lock{mutex_};
            wanted = wanted_;
        }
        const bool remotesChanged = wanted.remotes != applied_.remotes;
        if (remotesChanged && wanted.remotes == WiiRemoteHold::ToGame) {
            // Before the claim, so no remote is grabbed or given a virtual pad for the game.
            letGoOfWiiRemotes();
            polled.clear();
        }
        if (wanted.chords != applied_.chords) {
            for (const auto& pad : pads_) {
                pad->translator.setChords(wanted.chords);
            }
        }
        if (wanted.map != applied_.map) {
            // Before a hold, so a new virtual pad resumes from the mapped state.
            for (const auto& pad : pads_) {
                pad->translator.setForwardMap(wanted.map);
            }
        }
        if (wanted.pointer != applied_.pointer) {
            for (const auto& pad : pads_) {
                pad->pointer.setRange(wanted.pointer);
            }
        }
        const bool keepChanged = wanted.keep != applied_.keep;
        if (keepChanged) {
            keep(wanted.keep);
        }
        if (keepChanged || wanted.claim != applied_.claim || wanted.blocked != applied_.blocked ||
            wanted.lent != applied_.lent) {
            missed_ = false;
            for (const auto& pad : pads_) {
                if (!apply(*pad, wanted)) {
                    missed_ = true;
                }
            }
        }
        applied_ = wanted;
        if (remotesChanged && wanted.remotes == WiiRemoteHold::AsPads) {
            // The game is over: the remotes are pads again, under the claim now in force.
            scan();
            openSlots();
            polled.clear();
        }
        {
            const std::lock_guard lock{mutex_};
            appliedSerial_ = wanted.serial;
            appliedMissed_ = missed_;
        }
        appliedChanged_.notify_all();

        std::vector<Event> events;
        const ChordGate::Clock::time_point now = ChordGate::Clock::now();
        // Pads first: `polled` indexes pads_, loose_ and slots_ as they were polled, before any
        // is removed; pads made meanwhile go at the end.
        std::vector<std::size_t> gone;
        std::vector<std::size_t> extensionGone;
        std::vector<std::size_t> cameraGone;
        std::vector<std::size_t> looseGone;
        std::vector<std::size_t> slotsGone;
        for (std::size_t at = 0; at < polled.size(); ++at) {
            const pollfd& fd = fds[at + 2];
            if (fd.revents == 0) {
                continue;
            }
            const bool hungUp = (fd.revents & (POLLHUP | POLLERR)) != 0;
            const Polled& source = polled[at];
            std::vector<PadEvent> read;
            if (source.kind == Polled::Kind::Loose) {
                // Read only to notice it leave.
                if (!loose_[source.index].read(read) || hungUp) {
                    looseGone.push_back(source.index);
                }
                continue;
            }
            if (source.kind == Polled::Kind::Slot) {
                WiiSlot& slot = *slots_[source.index];
                const std::optional<WiiRemotePart> plugged = slot.extension();
                std::vector<WiiPartEvent> reports;
                if (!slot.read(reports) || hungUp) {
                    slotsGone.push_back(source.index);
                    continue;
                }
                Pad* pad = padOf(slot);
                if (pad == nullptr) {
                    if (slot.remote()) {
                        // Made from the slot's state, which these reports already are.
                        connect(std::make_unique<Pad>(slot));
                    }
                    continue;
                }
                if (slot.extension() != plugged) {
                    relayout(*pad);
                    continue;
                }
                std::vector<PadEvent> forward;
                const std::size_t before = events.size();
                for (const PadEvent& event : pad->translatable(reports)) {
                    pad->translator.translate(event, forward, events, now);
                }
                for (std::size_t i = before; i < events.size(); ++i) {
                    events[i].source = pad->id();
                }
                {
                    const std::lock_guard lock{mutex_};
                    axes_[pad->id()] = pad->translator.axes();
                }
                if (pad->live) {
                    write(*pad, forward);
                }
                continue;
            }
            Pad& pad = *pads_[source.index];
            if (source.kind == Polled::Kind::Camera) {
                if (!pad.camera->read(read) || hungUp) {
                    cameraGone.push_back(source.index);
                    continue;
                }
                for (const PadEvent& event : read) {
                    if (const std::optional<WiiAim> aim = pad.pointer.feed(event)) {
                        events.push_back(Event{.kind = Event::Kind::Pointer,
                                               .device = pad.name(),
                                               .source = pad.id(),
                                               .onScreen = aim->onScreen,
                                               .x = aim->x,
                                               .y = aim->y});
                    }
                }
                continue;
            }
            const bool fromExtension = source.kind == Polled::Kind::Extension;
            EvdevDevice& device = fromExtension ? *pad.extension : *pad.device;
            if (!device.read(read) || hungUp) {
                (fromExtension ? extensionGone : gone).push_back(source.index);
                continue;
            }
            if (!fromExtension && pad.camera) {
                // A game reads the remote's A as its own, wherever the remote points.
                const bool shellHolds =
                    applied_.claim == Claim::Free || applied_.claim == Claim::Shell;
                std::erase_if(read, [&](const PadEvent& event) {
                    if (event.type != EV_KEY || event.code != BTN_A) {
                        return false;
                    }
                    const bool pointer = event.value == 1
                                             ? shellHolds && pad.pointer.press(true)
                                             : event.value == 0 && pad.pointer.press(false);
                    if (pointer) {
                        events.push_back(Event{.kind = Event::Kind::PointerButton,
                                               .pressed = event.value == 1,
                                               .device = pad.name(),
                                               .source = pad.id()});
                    }
                    return pointer;
                });
            }
            std::vector<PadEvent> forward;
            const std::size_t before = events.size();
            const WiiRemotePart part =
                fromExtension ? *pad.extension->wiiRemotePart() : WiiRemotePart::Remote;
            for (const PadEvent& event : pad.translatable(read, part)) {
                pad.translator.translate(event, forward, events, now);
            }
            for (std::size_t i = before; i < events.size(); ++i) {
                events[i].source = pad.id();
            }
            {
                const std::lock_guard lock{mutex_};
                axes_[pad.id()] = pad.translator.axes();
            }
            if (pad.live) {
                write(pad, forward);
            }
        }
        for (const std::size_t i : extensionGone) {
            if (std::ranges::find(gone, i) == gone.end()) {
                lucent::info("gamepad", "extension unplugged from {}", pads_[i]->name());
                setExtension(*pads_[i], std::nullopt);
            }
        }
        for (const std::size_t i : cameraGone) {
            Pad& pad = *pads_[i];
            pad.camera.reset();
            pad.pointer = WiiPointer{};
            pad.pointer.setRange(applied_.pointer);
            events.push_back(Event{.kind = Event::Kind::Pointer,
                                   .device = pad.name(),
                                   .source = pad.id(),
                                   .onScreen = false});
        }
        for (const std::size_t i : std::views::reverse(looseGone)) {
            loose_.erase(loose_.begin() + static_cast<std::ptrdiff_t>(i));
        }
        for (const std::size_t i : std::views::reverse(gone)) {
            drop(i, events);
        }
        for (const std::size_t i : std::views::reverse(slotsGone)) {
            const WiiSlot* slot = slots_[i].get();
            const auto pad = std::ranges::find(pads_, slot, &Pad::slot);
            if (pad != pads_.end()) {
                drop(static_cast<std::size_t>(pad - pads_.begin()), events);
            }
            lucent::info("gamepad", "DolphinBar slot {} is gone", slot->node().string());
            slots_.erase(slots_.begin() + static_cast<std::ptrdiff_t>(i));
        }
        if (!slots_.empty() && std::chrono::steady_clock::now() >= asked_ + askEvery) {
            askSlots(events);
        }
        // A batch is forwarded before its controls are reported.
        publish(events);

        if ((fds[1].revents & POLLIN) != 0) {
            std::array<char, 4096> buffer{};
            const ssize_t got = ::read(watch_, buffer.data(), buffer.size());
            bool hidrawChanged = false;
            for (ssize_t at = 0; at < got;) {
                const auto* change = reinterpret_cast<const inotify_event*>(buffer.data() + at);
                if (change->len > 0 && change->wd == hidWatch_) {
                    hidrawChanged =
                        hidrawChanged || std::string_view{change->name}.starts_with("hidraw");
                } else if (change->len > 0) {
                    add(inputDir_ / change->name);
                }
                at += static_cast<ssize_t>(sizeof(inotify_event) + change->len);
            }
            if (hidrawChanged) {
                openSlots();
            }
        }
    }
}

} // namespace opensu::gamepad
