// catalog_loader — reads every store's listing off the frame thread. Each provider lists on a
// thread of its own, so a slow store (Epic's `legendary list` takes seconds) delays neither the
// others nor the frame, and each listing reaches the shell as it arrives.
#pragma once

#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <thread>
#include <vector>

#include "game.hpp"

namespace opensu::library {

class CatalogLoader {
  public:
    explicit CatalogLoader(Catalog catalog);
    ~CatalogLoader();
    CatalogLoader(const CatalogLoader&) = delete;
    CatalogLoader& operator=(const CatalogLoader&) = delete;

    /// Asks every provider to list again, and returns at once. A provider still listing lists
    /// once more when it is done.
    void refresh();

    /// Reads `catalog`'s providers from now on and asks each to list, returning at once. A store
    /// the new catalog lacks drops out of the next snapshot with its games; a listing still under
    /// way from a replaced provider is thrown away when it ends.
    void replace(Catalog catalog);

    /// The catalog as it stands, if any listing arrived since the last call. A store whose first
    /// listing is not in is `Loading`; one that is listing again keeps its previous listing.
    [[nodiscard]] std::optional<CatalogSnapshot> take();

    /// Whether no provider is listing or waiting to.
    [[nodiscard]] bool idle() const;

    /// Blocks until `idle`; for a still render only.
    void waitIdle();

  private:
    struct Job {
        std::unique_ptr<Provider> provider;
        bool wanted{false};
        bool busy{false};
        /// Replaced: whatever it lists now is not shown.
        bool retired{false};
    };

    /// Takes over `catalog`'s providers and starts a thread for each.
    void adopt(Catalog catalog);
    void run(Job& job, const std::stop_token& stop);

    mutable std::mutex mutex_;
    std::condition_variable_any wake_;
    std::condition_variable_any settled_;
    std::vector<std::unique_ptr<Job>> jobs_;
    /// Replaced jobs, kept until their threads are joined.
    std::vector<std::unique_ptr<Job>> retired_;
    std::vector<Source> sources_;
    /// The stores changed since the last `take`.
    bool reshaped_{false};
    std::vector<SourceListing> arrived_;
    /// The latest listing of each store. Taken by the loop only.
    std::vector<SourceListing> known_;
    /// Last, so the threads stop before anything they read is destroyed.
    std::vector<std::jthread> threads_;
    std::vector<std::jthread> retiredThreads_;
};

} // namespace opensu::library
