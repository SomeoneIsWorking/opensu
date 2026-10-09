// image_decoder — decodes cover art files into pixels on a worker thread, so the frame thread
// only uploads them. Decoding needs no GL context; the texture upload that follows does.
#pragma once

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <mutex>
#include <stop_token>
#include <thread>
#include <utility>
#include <vector>

#include "raylib.h"

namespace opensu::ui {

/// The longest side a cover texture keeps; covers arrive up to 1200 x 1600 and more, and a tile
/// is never drawn that large, so larger ones are scaled down while decoding.
inline constexpr int maxCoverSide = 768;

/// Decoded pixels, released with the owner.
class Pixels {
  public:
    Pixels() = default;
    explicit Pixels(Image image) noexcept : image_{image} {
    }
    ~Pixels();
    Pixels(Pixels&& other) noexcept;
    Pixels& operator=(Pixels&& other) noexcept;
    Pixels(const Pixels&) = delete;
    Pixels& operator=(const Pixels&) = delete;

    [[nodiscard]] bool empty() const noexcept {
        return image_.data == nullptr;
    }
    [[nodiscard]] const Image& image() const noexcept {
        return image_;
    }

    /// Gives the pixels up to the caller, who unloads them.
    [[nodiscard]] Image release() noexcept {
        return std::exchange(image_, Image{});
    }

  private:
    Image image_{};
};

/// One tile's art files to decode, and the tile they are for.
struct DecodeJob {
    /// The shelf the tile was in, so an answer for a shelf since replaced is dropped.
    std::uint64_t shelf{};
    /// The tile's place in that shelf.
    std::size_t tile{};
    /// Changes whenever the tile's files do.
    std::uint64_t ticket{};
    std::filesystem::path portrait;
    std::filesystem::path wide;
};

/// A job done; an unreadable or missing file gives empty pixels.
struct Decoded {
    DecodeJob job;
    Pixels portrait;
    Pixels wide;
};

class ImageDecoder {
  public:
    ImageDecoder();
    ~ImageDecoder();
    ImageDecoder(const ImageDecoder&) = delete;
    ImageDecoder& operator=(const ImageDecoder&) = delete;

    /// Queues a job; the worker takes the oldest first.
    void request(DecodeJob job);

    /// Drops the queued jobs `keep` refuses and returns them. A job already decoding is kept.
    std::vector<DecodeJob> prune(const std::function<bool(const DecodeJob&)>& keep);

    /// Up to `limit` finished jobs, oldest first.
    [[nodiscard]] std::vector<Decoded> take(std::size_t limit);

    /// Whether nothing is queued, decoding or waiting to be taken.
    [[nodiscard]] bool idle() const;

    /// Blocks until `idle` or nothing but finished jobs is left; for a still render only.
    void waitDecoded();

  private:
    void run(const std::stop_token& stop);

    mutable std::mutex mutex_;
    std::condition_variable_any wake_;
    std::condition_variable settled_;
    std::vector<DecodeJob> queue_;
    std::vector<Decoded> done_;
    bool busy_{false};
    /// Last, so the worker stops before anything it reads is destroyed.
    std::jthread worker_;
};

} // namespace opensu::ui
