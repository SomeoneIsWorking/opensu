#include "catalog_loader.hpp"

#include <algorithm>
#include <utility>

namespace opensu::library {

CatalogLoader::CatalogLoader(Catalog catalog) {
    adopt(std::move(catalog));
}

CatalogLoader::~CatalogLoader() {
    for (std::jthread& thread : threads_) {
        thread.request_stop();
    }
    wake_.notify_all();
}

void CatalogLoader::adopt(Catalog catalog) {
    std::vector<std::unique_ptr<Job>> jobs;
    std::vector<Source> sources;
    for (std::unique_ptr<Provider>& provider : std::move(catalog).release()) {
        sources.push_back(provider->source());
        jobs.push_back(std::make_unique<Job>(Job{std::move(provider)}));
    }
    {
        const std::lock_guard lock{mutex_};
        jobs_ = std::move(jobs);
        sources_ = std::move(sources);
    }
    for (const std::unique_ptr<Job>& job : jobs_) {
        threads_.emplace_back([this, &job = *job](const std::stop_token& stop) {
            run(job, stop);
        });
    }
}

void CatalogLoader::replace(Catalog catalog) {
    {
        const std::lock_guard lock{mutex_};
        for (std::unique_ptr<Job>& job : jobs_) {
            job->retired = true;
            retired_.push_back(std::move(job));
        }
        jobs_.clear();
        arrived_.clear();
        reshaped_ = true;
    }
    for (std::jthread& thread : threads_) {
        thread.request_stop();
        retiredThreads_.push_back(std::move(thread));
    }
    threads_.clear();
    wake_.notify_all();
    adopt(std::move(catalog));
    {
        const std::lock_guard lock{mutex_};
        std::erase_if(known_, [this](const SourceListing& listing) {
            return std::ranges::find(sources_, listing.status.source) == sources_.end();
        });
    }
    settled_.notify_all();
    refresh();
}

void CatalogLoader::refresh() {
    {
        const std::lock_guard lock{mutex_};
        for (const std::unique_ptr<Job>& job : jobs_) {
            job->wanted = true;
        }
    }
    wake_.notify_all();
}

std::optional<CatalogSnapshot> CatalogLoader::take() {
    {
        const std::lock_guard lock{mutex_};
        const bool reshaped = std::exchange(reshaped_, false);
        if (arrived_.empty() && !reshaped) {
            return std::nullopt;
        }
        for (SourceListing& listing : std::exchange(arrived_, {})) {
            const auto same =
                std::ranges::find(known_, listing.status.source, [](const SourceListing& known) {
                    return known.status.source;
                });
            if (same != known_.end()) {
                *same = std::move(listing);
            } else {
                known_.push_back(std::move(listing));
            }
        }
    }
    return assemble(sources_, known_);
}

bool CatalogLoader::idle() const {
    const std::lock_guard lock{mutex_};
    return std::ranges::none_of(jobs_, [](const std::unique_ptr<Job>& job) {
        return job->wanted || job->busy;
    });
}

void CatalogLoader::waitIdle() {
    std::unique_lock lock{mutex_};
    settled_.wait(lock, [this] {
        return std::ranges::none_of(jobs_, [](const std::unique_ptr<Job>& job) {
            return job->wanted || job->busy;
        });
    });
}

void CatalogLoader::run(Job& job, const std::stop_token& stop) {
    while (!stop.stop_requested()) {
        {
            std::unique_lock lock{mutex_};
            if (!wake_.wait(lock, stop, [&job] {
                    return job.wanted;
                })) {
                return;
            }
            job.wanted = false;
            job.busy = true;
        }
        SourceListing listing = readProvider(*job.provider);
        {
            const std::lock_guard lock{mutex_};
            if (!job.retired) {
                arrived_.push_back(std::move(listing));
            }
            job.busy = false;
        }
        settled_.notify_all();
    }
}

} // namespace opensu::library
