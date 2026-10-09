// The catalog loader: a provider that blocks delays neither the caller nor the other stores, and
// each listing is handed over as it arrives.
#include "library/catalog_loader.hpp"

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

using opensu::library::Availability;
using opensu::library::CatalogLoader;
using opensu::library::Game;
using opensu::library::Source;

void expect(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

const opensu::library::CatalogSnapshot&
required(const std::optional<opensu::library::CatalogSnapshot>& snapshot, const char* what) {
    if (!snapshot) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
    return *snapshot;
}

/// Holds a provider's listing until the test lets it go.
class Gate {
  public:
    void open() {
        {
            const std::lock_guard lock{mutex_};
            open_ = true;
        }
        opened_.notify_all();
    }
    void wait() {
        std::unique_lock lock{mutex_};
        opened_.wait(lock, [this] {
            return open_;
        });
    }

  private:
    std::mutex mutex_;
    std::condition_variable opened_;
    bool open_{false};
};

class FakeProvider final : public opensu::library::Provider {
  public:
    FakeProvider(Source source, std::string id, Gate* gate)
        : source_{source}, id_{std::move(id)}, gate_{gate} {
    }
    [[nodiscard]] Source source() const override {
        return source_;
    }
    [[nodiscard]] std::vector<Game> list() override {
        if (gate_ != nullptr) {
            gate_->wait();
        }
        Game game;
        game.id = id_;
        game.title = id_;
        game.source = source_;
        return {game};
    }

  private:
    Source source_;
    std::string id_;
    Gate* gate_;
};

template <class Ready> bool eventually(const Ready& ready) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (std::chrono::steady_clock::now() < deadline) {
        if (ready()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return false;
}

void blockedStoreDoesNotBlockTheCaller() {
    Gate epicGate;
    opensu::library::Catalog catalog;
    catalog.add(std::make_unique<FakeProvider>(Source::Steam, "steam:1", nullptr));
    catalog.add(std::make_unique<FakeProvider>(Source::Epic, "epic:1", &epicGate));
    CatalogLoader loader{std::move(catalog)};

    const auto before = std::chrono::steady_clock::now();
    loader.refresh();
    expect(std::chrono::steady_clock::now() - before < std::chrono::milliseconds(500),
           "refresh returns while a provider is still blocked");

    std::optional<opensu::library::CatalogSnapshot> early;
    expect(eventually([&] {
               early = loader.take();
               return early.has_value();
           }),
           "the store that is not blocked arrives");
    const opensu::library::CatalogSnapshot& first = required(early, "a listing arrived");
    expect(first.games.size() == 1 && first.games[0].id == "steam:1",
           "the listing that arrived is in the catalog");
    expect(first.sources[0].availability == Availability::Ready, "Steam is ready");
    expect(first.sources[1].availability == Availability::Loading,
           "the blocked store is loading, not absent");
    expect(!loader.take(), "nothing new, nothing handed over");
    expect(!loader.idle(), "the loader is still waiting on Epic");

    epicGate.open();
    loader.waitIdle();
    const std::optional<opensu::library::CatalogSnapshot> late = loader.take();
    const opensu::library::CatalogSnapshot& full = required(late, "the late listing arrived");
    expect(full.games.size() == 2, "the late listing joins the catalog");
    expect(full.sources[1].availability == Availability::Ready, "and Epic is ready");
}

void refreshKeepsWhatIsKnown() {
    opensu::library::Catalog catalog;
    catalog.add(std::make_unique<FakeProvider>(Source::Steam, "steam:1", nullptr));
    CatalogLoader loader{std::move(catalog)};
    loader.refresh();
    loader.waitIdle();
    expect(loader.take().has_value(), "the first listing arrives");
    loader.refresh();
    loader.waitIdle();
    const auto again = loader.take();
    expect(again && again->games.size() == 1 &&
               again->sources[0].availability == Availability::Ready,
           "a second read replaces the listing without a Loading gap");
}

} // namespace

int main() {
    blockedStoreDoesNotBlockTheCaller();
    refreshKeepsWhatIsKnown();
    std::printf("catalog_loader: all checks passed\n");
    return 0;
}
