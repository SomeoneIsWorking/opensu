#include "image_decoder.hpp"

#include <algorithm>
#include <iterator>
#include <utility>

namespace opensu::ui {
namespace {

Pixels decode(const std::filesystem::path& file) {
    if (file.empty() || !std::filesystem::is_regular_file(file)) {
        return {};
    }
    Pixels pixels{LoadImage(file.string().c_str())};
    if (pixels.empty()) {
        return pixels;
    }
    Image image = pixels.release();
    const int longest = std::max(image.width, image.height);
    if (longest > maxCoverSide) {
        const float scale = static_cast<float>(maxCoverSide) / static_cast<float>(longest);
        ImageResize(&image, std::max(1, static_cast<int>(static_cast<float>(image.width) * scale)),
                    std::max(1, static_cast<int>(static_cast<float>(image.height) * scale)));
    }
    return Pixels{image};
}

} // namespace

Pixels::~Pixels() {
    if (!empty()) {
        UnloadImage(image_);
    }
}

Pixels::Pixels(Pixels&& other) noexcept : image_{std::exchange(other.image_, Image{})} {
}

Pixels& Pixels::operator=(Pixels&& other) noexcept {
    if (this != &other) {
        if (!empty()) {
            UnloadImage(image_);
        }
        image_ = std::exchange(other.image_, Image{});
    }
    return *this;
}

ImageDecoder::ImageDecoder()
    : worker_{[this](const std::stop_token& stop) {
          run(stop);
      }} {
}

ImageDecoder::~ImageDecoder() {
    worker_.request_stop();
    wake_.notify_all();
}

void ImageDecoder::request(DecodeJob job) {
    {
        const std::lock_guard lock{mutex_};
        queue_.push_back(std::move(job));
    }
    wake_.notify_all();
}

std::vector<DecodeJob> ImageDecoder::prune(const std::function<bool(const DecodeJob&)>& keep) {
    const std::lock_guard lock{mutex_};
    const auto dropped = std::ranges::stable_partition(queue_, keep);
    std::vector<DecodeJob> out{std::make_move_iterator(dropped.begin()),
                               std::make_move_iterator(dropped.end())};
    queue_.erase(dropped.begin(), dropped.end());
    return out;
}

std::vector<Decoded> ImageDecoder::take(std::size_t limit) {
    const std::lock_guard lock{mutex_};
    const std::size_t count = std::min(limit, done_.size());
    std::vector<Decoded> out;
    out.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        out.push_back(std::move(done_[i]));
    }
    done_.erase(done_.begin(), done_.begin() + static_cast<std::ptrdiff_t>(count));
    return out;
}

bool ImageDecoder::idle() const {
    const std::lock_guard lock{mutex_};
    return queue_.empty() && !busy_ && done_.empty();
}

void ImageDecoder::waitDecoded() {
    std::unique_lock lock{mutex_};
    settled_.wait(lock, [this] {
        return queue_.empty() && !busy_;
    });
}

void ImageDecoder::run(const std::stop_token& stop) {
    while (!stop.stop_requested()) {
        DecodeJob job;
        {
            std::unique_lock lock{mutex_};
            busy_ = false;
            settled_.notify_all();
            if (!wake_.wait(lock, stop, [this] {
                    return !queue_.empty();
                })) {
                return;
            }
            job = std::move(queue_.front());
            queue_.erase(queue_.begin());
            busy_ = true;
        }
        Decoded done{std::move(job), {}, {}};
        done.portrait = decode(done.job.portrait);
        done.wide = decode(done.job.wide);
        const std::lock_guard lock{mutex_};
        done_.push_back(std::move(done));
    }
}

} // namespace opensu::ui
