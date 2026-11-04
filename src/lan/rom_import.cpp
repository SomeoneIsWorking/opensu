#include "rom_import.hpp"

#include <algorithm>
#include <fstream>
#include <system_error>
#include <utility>

#include "lucent/log.h"
#include "secrets/secret.hpp"

namespace opensu::lan {
namespace {

namespace fs = std::filesystem;
using namespace std::chrono_literals;

constexpr long ok = 200;
constexpr std::size_t largestFacts = 4096;
/// The longest wait before a host that did not answer is asked again.
constexpr std::chrono::milliseconds longestWait = 30s;
constexpr std::chrono::milliseconds firstWait = 1s;
/// How often a host still hashing a file is asked again.
constexpr std::chrono::milliseconds hashPoll = 500ms;
constexpr std::size_t readPiece = std::size_t{1} << 20;

/// A refusal's text without its line end, or the status when it has none.
std::string refusalOf(const net::WebClient::Answer& answer) {
    std::string text = answer.body;
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) {
        text.pop_back();
    }
    return text.empty() ? "HTTP " + std::to_string(answer.status) : text;
}

/// SHA-256 over the first `bytes` of `file`; false when it cannot be read.
bool hashPart(const fs::path& file, std::uint64_t bytes, crypto::Sha256& hash) {
    std::ifstream in{file, std::ios::binary};
    std::string piece(readPiece, '\0');
    std::uint64_t left = bytes;
    while (left > 0) {
        const auto want = static_cast<std::streamsize>(std::min<std::uint64_t>(left, piece.size()));
        in.read(piece.data(), want);
        if (in.gcount() != want) {
            return false;
        }
        hash.add(std::string_view{piece.data(), static_cast<std::size_t>(want)});
        left -= static_cast<std::uint64_t>(want);
    }
    return true;
}

} // namespace

std::string_view label(RomImport::State state) noexcept {
    switch (state) {
    case RomImport::State::Queued:
        return "queued";
    case RomImport::State::Fetching:
        return "fetching";
    case RomImport::State::Waiting:
        return "waiting";
    case RomImport::State::Checking:
        return "checking";
    case RomImport::State::Done:
        return "done";
    case RomImport::State::Failed:
        return "failed";
    }
    return "queued";
}

RomImport::RomImport(Setup setup, const net::WebClient& web)
    : setup_{std::move(setup)}, web_{web}, worker_{[this](const std::stop_token& stop) {
          run(stop);
      }} {
}

RomImport::~RomImport() {
    worker_.request_stop();
    wake_.notify_all();
}

void RomImport::add(std::vector<Item> items) {
    {
        const std::lock_guard lock{mutex_};
        for (Item& item : items) {
            std::uint64_t total = 0;
            for (const File& file : item.files) {
                total += file.bytes;
            }
            Progress progress{item.id, item.host, item.title,    item.system,
                              0,       total,     State::Queued, {}};
            const auto kept = std::ranges::find(entries_, item.id, [](const Entry& entry) {
                return entry.item.id;
            });
            if (kept == entries_.end()) {
                entries_.push_back(Entry{std::move(item), std::move(progress)});
            } else if (kept->progress.state == State::Done ||
                       kept->progress.state == State::Failed) {
                kept->item = std::move(item);
                kept->progress = std::move(progress);
            }
        }
    }
    wake_.notify_all();
    setup_.changed();
}

void RomImport::cancel(const std::string& host) {
    std::vector<Item> dropped;
    {
        const std::lock_guard lock{mutex_};
        for (const Entry& entry : entries_) {
            if (entry.item.host == host && entry.progress.state != State::Done) {
                dropped.push_back(entry.item);
            }
        }
        std::erase_if(entries_, [&](const Entry& entry) {
            return entry.item.host == host && entry.progress.state != State::Done;
        });
    }
    wake_.notify_all();
    for (const Item& item : dropped) {
        for (const File& file : item.files) {
            std::error_code ec;
            fs::remove(partOf(item.host, file), ec);
        }
    }
    setup_.changed();
}

std::vector<RomImport::Progress> RomImport::progress() const {
    const std::lock_guard lock{mutex_};
    std::vector<Progress> out;
    out.reserve(entries_.size());
    for (const Entry& entry : entries_) {
        out.push_back(entry.progress);
    }
    return out;
}

fs::path RomImport::partOf(const std::string& host, const File& file) const {
    const std::string folder =
        file.kind == wire::RomKind::Bios ? "bios" : "rom-" + std::to_string(file.root);
    return setup_.staging / host / folder / (file.path + ".part");
}

void RomImport::update(const std::string& id, State state, std::string note) {
    {
        const std::lock_guard lock{mutex_};
        const auto entry = std::ranges::find(entries_, id, [](const Entry& kept) {
            return kept.item.id;
        });
        if (entry == entries_.end()) {
            return;
        }
        entry->progress.state = state;
        entry->progress.note = std::move(note);
    }
    setup_.changed();
}

void RomImport::reached(const std::string& id, std::uint64_t done) {
    {
        const std::lock_guard lock{mutex_};
        const auto entry = std::ranges::find(entries_, id, [](const Entry& kept) {
            return kept.item.id;
        });
        if (entry == entries_.end()) {
            return;
        }
        entry->progress.done = done;
        entry->progress.total = std::max(entry->progress.total, done);
    }
    setup_.changed();
}

bool RomImport::wanted(const std::string& id) const {
    const std::lock_guard lock{mutex_};
    return std::ranges::any_of(entries_, [&](const Entry& entry) {
        return entry.item.id == id;
    });
}

void RomImport::rest(std::chrono::milliseconds wait, const std::string& id,
                     const std::stop_token& stop) {
    std::unique_lock lock{mutex_};
    wake_.wait_for(lock, stop, wait, [&] {
        return std::ranges::none_of(entries_, [&](const Entry& entry) {
            return entry.item.id == id;
        });
    });
}

void RomImport::run(const std::stop_token& stop) {
    while (!stop.stop_requested()) {
        std::optional<Item> next;
        {
            std::unique_lock lock{mutex_};
            wake_.wait(lock, stop, [this] {
                return std::ranges::any_of(entries_, [](const Entry& entry) {
                    return entry.progress.state == State::Queued;
                });
            });
            const auto queued = std::ranges::find(entries_, State::Queued, [](const Entry& entry) {
                return entry.progress.state;
            });
            if (stop.stop_requested() || queued == entries_.end()) {
                continue;
            }
            next = queued->item;
        }
        fetch(*next, stop);
    }
}

void RomImport::fetch(const Item& item, const std::stop_token& stop) {
    update(item.id, State::Fetching, {});
    std::uint64_t before = 0;
    for (const File& file : item.files) {
        std::chrono::milliseconds wait = firstWait;
        while (true) {
            if (stop.stop_requested() || !wanted(item.id)) {
                return;
            }
            std::string error;
            bool lasting = false;
            if (fetchFile(item, file, before, stop, error, lasting)) {
                break;
            }
            if (stop.stop_requested() || !wanted(item.id)) {
                return;
            }
            if (lasting) {
                lucent::warn("lan", "{} from {} cannot be imported: {}", item.title, item.hostName,
                             error);
                update(item.id, State::Failed, std::move(error));
                return;
            }
            lucent::info("lan", "importing {} from {} waits: {}", item.title, item.hostName, error);
            update(item.id, State::Waiting, std::move(error));
            rest(wait, item.id, stop);
            wait = std::min(wait * 2, longestWait);
            update(item.id, State::Fetching, {});
        }
        before += file.bytes;
        reached(item.id, before);
    }
    lucent::info("lan", "imported {} from {}", item.title, item.hostName);
    update(item.id, State::Done, {});
}

bool RomImport::fetchFile(const Item& item, const File& file, std::uint64_t before,
                          const std::stop_token& stop, std::string& error, bool& lasting) {
    std::optional<Link> link = setup_.link(item.host, error);
    if (!link) {
        return false;
    }
    const PeerClient client{web_, link->endpoint};
    const PeerClient::Signer signer{setup_.selfId, link->secret};
    const auto wipe = [&] {
        secrets::wipeBytes(link->secret.data(), link->secret.size());
    };
    wire::RomAsk ask{file.kind, file.root, file.path, 0, 0};
    const auto askFacts = [&]() -> std::optional<wire::RomFacts> {
        const std::optional<net::WebClient::Answer> answer =
            client.call("POST", wire::romFileRoute, wire::write(ask), signer, largestFacts, error);
        if (!answer) {
            return std::nullopt;
        }
        if (answer->status != ok) {
            error = refusalOf(*answer);
            lasting = true;
            return std::nullopt;
        }
        std::optional<wire::RomFacts> facts = wire::readRomFacts(answer->body);
        if (!facts) {
            error = "its answer is not a file's size and hash";
        }
        return facts;
    };
    std::optional<wire::RomFacts> facts = askFacts();
    if (!facts) {
        wipe();
        return false;
    }
    const fs::path part = partOf(item.host, file);
    std::error_code ec;
    fs::create_directories(part.parent_path(), ec);
    std::uint64_t have = fs::exists(part, ec) ? fs::file_size(part, ec) : 0;
    crypto::Sha256 hash;
    if (ec || have > facts->bytes || !hashPart(part, have, hash)) {
        fs::remove(part, ec);
        have = 0;
        hash = crypto::Sha256{};
    }
    if (have > 0) {
        lucent::info("lan", "importing {} from {}: {} goes on from byte {}", item.title,
                     item.hostName, file.path, have);
    }
    reached(item.id, before + have);
    {
        std::ofstream out{part, std::ios::binary | std::ios::app};
        if (!out) {
            wipe();
            error = "cannot write " + part.string();
            lasting = true;
            return false;
        }
        while (have < facts->bytes) {
            if (stop.stop_requested() || !wanted(item.id)) {
                wipe();
                error = "stopped";
                return false;
            }
            ask.offset = have;
            ask.length = wire::romChunkBytes;
            const std::optional<net::WebClient::Answer> answer = client.call(
                "POST", wire::romChunkRoute, wire::write(ask), signer, wire::romChunkBytes, error);
            if (!answer) {
                wipe();
                return false;
            }
            if (answer->status != ok) {
                wipe();
                error = refusalOf(*answer);
                lasting = true;
                return false;
            }
            if (answer->body.empty() || answer->body.size() > facts->bytes - have) {
                wipe();
                error = "it sent a chunk of the wrong size";
                return false;
            }
            out.write(answer->body.data(), static_cast<std::streamsize>(answer->body.size()));
            out.flush();
            if (!out) {
                wipe();
                error = "cannot write " + part.string();
                lasting = true;
                return false;
            }
            hash.add(answer->body);
            have += answer->body.size();
            reached(item.id, before + have);
        }
    }
    update(item.id, State::Checking, {});
    while (facts->sha256.empty()) {
        rest(hashPoll, item.id, stop);
        if (stop.stop_requested() || !wanted(item.id)) {
            wipe();
            error = "stopped";
            return false;
        }
        facts = askFacts();
        if (!facts) {
            wipe();
            return false;
        }
    }
    wipe();
    const std::string arrived = secrets::hexText(hash.finish());
    if (arrived != facts->sha256 || have != facts->bytes) {
        fs::remove(part, ec);
        error = file.path + " arrived different from " + item.hostName + "'s copy";
        lasting = true;
        return false;
    }
    lucent::info("lan", "{} from {} arrived whole, SHA-256 {}", file.path, item.hostName, arrived);
    if (!setup_.place(item, file, part, error)) {
        fs::remove(part, ec);
        lasting = true;
        return false;
    }
    fs::remove(part, ec);
    update(item.id, State::Fetching, {});
    return true;
}

} // namespace opensu::lan
