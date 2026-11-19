#include "guest_video.hpp"

#include <algorithm>
#include <chrono>
#include <format>
#include <stdexcept>
#include <utility>
#include <vector>

#include "clock/percentile.hpp"
#include "lucent/log.h"

namespace opensu::stream {
namespace {

/// The shown frames the percentiles are taken over: the last ten seconds at 60 a second.
constexpr std::size_t framesKept = 600;
/// Decoded frames waiting to be shown that are remembered; older ones were never shown.
constexpr std::size_t waitingKept = 8;
constexpr double microsPerMilli = 1000.0;
/// How often a decoder holding frames is asked for its pictures between packets.
constexpr auto collectEvery = std::chrono::milliseconds{2};
/// How often the decoding thread wakes without packets, to notice a stream with no pictures.
constexpr auto silenceCheckedEvery = std::chrono::seconds{1};

} // namespace

GuestVideo::GuestVideo(PictureMemory memory)
    : memory_{memory}, lastPictureAt_{std::chrono::steady_clock::now()},
      decoding_{[this](const std::stop_token& stop) {
          decodeLoop(stop);
      }} {
}

bool GuestVideo::take(const wire::VideoPacket& packet, std::uint64_t receivedUs,
                      std::int64_t hostAheadUs) {
    {
        const std::lock_guard lock{queueMutex_};
        queue_.push_back(
            Queued{.packet = packet, .receivedUs = receivedUs, .hostAheadUs = hostAheadUs});
    }
    queued_.notify_one();
    return keyWanted_.load();
}

void GuestVideo::decodeLoop(const std::stop_token& stop) {
    std::vector<Queued> batch;
    while (!stop.stop_requested()) {
        {
            std::unique_lock lock{queueMutex_};
            const auto packets = [this] {
                return !queue_.empty();
            };
            queued_.wait_for(lock, stop, behind() ? collectEvery : silenceCheckedEvery, packets);
            if (stop.stop_requested()) {
                return;
            }
            batch.swap(queue_);
        }
        packetsSincePicture_ += batch.size();
        noteSilence();
        // A PyroWave frame two newer ones have begun after would only be shown late; a frame
        // others build on is decoded anyway.
        std::uint32_t newest = 0;
        for (const Queued& queued : batch) {
            newest = std::max(newest, queued.packet.frame);
        }
        const std::uint32_t oldestKept = newest > 0 ? newest - 1 : 0;
        std::uint32_t skipped = 0;
        std::size_t lateFrames = 0;
        for (const Queued& queued : batch) {
            const bool intra =
                queued.packet.codec == static_cast<std::uint8_t>(VideoCodec::PyroWave);
            if (intra && queued.packet.frame < oldestKept) {
                if (queued.packet.frame != skipped) {
                    skipped = queued.packet.frame;
                    ++lateFrames;
                }
                continue;
            }
            decode(queued);
        }
        if (lateFrames > 0) {
            const std::lock_guard lock{mutex_};
            late_ += lateFrames;
        }
        batch.clear();
        collect();
        noteFit();
    }
}

void GuestVideo::noteFit() {
    GuestFit::Counts counts;
    bool pyroWave = false;
    {
        const std::lock_guard lock{mutex_};
        counts = {.decoded = decoded_,
                  .dropped = dropped_,
                  .late = late_,
                  .arrivalUs = playout_.delayUs()};
        pyroWave = codec_ == nameOf(VideoCodec::PyroWave);
    }
    if (!fit_.take(counts, pyroWave, wire::nowMicros())) {
        return;
    }
    if (fit_.pyroWaveGivenUp() && !pyroWaveGivenUp_) {
        pyroWaveGivenUp_ = true;
        lucent::warn("video",
                     "PyroWave lost or skipped {:.0f}% of its frames and decoded them {} ms after "
                     "their send: asking for a hardware codec",
                     fit_.missed() * 100, fit_.arrivalUs() / 1000);
        return;
    }
    framesPerSecondFit_ = fit_.framesPerSecond();
    lucent::info("video",
                 "{:.0f}% of frames were lost or late: asking for {} frames a second (0 is all)",
                 fit_.missed() * 100, fit_.framesPerSecond());
}

void GuestVideo::decode(const Queued& queued) {
    const wire::VideoPacket& packet = queued.packet;
    const std::int64_t hostAheadUs = queued.hostAheadUs;
    const auto local = [hostAheadUs](std::uint64_t hostUs) {
        return static_cast<double>(static_cast<std::int64_t>(hostUs) - hostAheadUs);
    };
    // A frame's parity can come after the frame was whole and handed on.
    if (packet.frame <= handedOn_) {
        return;
    }
    if (!building_ || building_->number != packet.frame) {
        if (building_ && packet.frame < building_->number) {
            return;
        }
        if (building_) {
            const std::lock_guard lock{mutex_};
            ++dropped_;
        }
        building_ = Times{.number = packet.frame,
                          .capturedUs = local(packet.capturedUs),
                          .encodedUs = local(packet.encodedUs),
                          .sentUs = local(packet.sentUs),
                          .receivedUs = 0,
                          .decodedUs = 0};
    }
    building_->receivedUs = static_cast<double>(queued.receivedUs);
    const auto codec = static_cast<VideoCodec>(packet.codec);
    if (holds(undecodable_, codec)) {
        building_.reset();
        return;
    }
    std::unique_ptr<VideoDecoder>& decoder = decoders_[codec];
    bool whole = false;
    try {
        if (!decoder) {
            decoder = openDecoder(codec);
            lucent::info("video", "decoding {} with {}", nameOf(codec), decoder->backend());
        }
        whole = decoder->push(packet);
    } catch (const std::runtime_error& failure) {
        lucent::warn("video", "cannot decode {}: {}", nameOf(codec), failure.what());
        undecodable_ = static_cast<VideoCodecs>(undecodable_ | codecBit(codec));
        decoders_.erase(codec);
        building_.reset();
        return;
    }
    // Asked again with each packet while the decoder waits, so a lost ask or key frame is mended.
    keyWanted_ = decoder->awaitingKey();
    if (!whole) {
        return;
    }
    inFlight_.push_back(*building_);
    if (inFlight_.size() > waitingKept) {
        inFlight_.pop_front();
    }
    handedOn_ = packet.frame;
    building_.reset();
    Picture picture;
    try {
        if (decoder->decode(picture, memory_.load())) {
            finish(codec, std::move(picture), packet.frame);
        }
    } catch (const std::runtime_error& failure) {
        lucent::warn("video", "frame {} did not decode: {}", packet.frame, failure.what());
        keyWanted_ = decoder->awaitingKey();
    }
}

void GuestVideo::noteSilence() {
    const auto quiet = std::chrono::steady_clock::now() - lastPictureAt_;
    if (silent_ || quiet < silenceWarned) {
        return;
    }
    silent_ = true;
    lucent::warn("video", "no picture for {} s: {} packets came in that time",
                 std::chrono::duration_cast<std::chrono::seconds>(quiet).count(),
                 packetsSincePicture_);
}

bool GuestVideo::behind() const {
    return std::ranges::any_of(decoders_, [](const auto& entry) {
        return entry.second->behind();
    });
}

void GuestVideo::collect() {
    for (const auto& [codec, decoder] : decoders_) {
        if (!decoder->behind()) {
            continue;
        }
        Picture picture;
        try {
            if (decoder->collect(picture)) {
                finish(codec, std::move(picture), 0);
            }
        } catch (const std::runtime_error& failure) {
            lucent::warn("video", "a frame did not decode: {}", failure.what());
            keyWanted_ = decoder->awaitingKey();
        }
    }
}

void GuestVideo::finish(VideoCodec codec, Picture picture, std::uint32_t number) {
    const std::uint32_t shows = picture.frame.value_or(number);
    const auto found = std::ranges::find(inFlight_, shows, &Times::number);
    if (found == inFlight_.end()) {
        return;
    }
    Times times = *found;
    inFlight_.erase(inFlight_.begin(), found + 1);
    times.decodedUs = static_cast<double>(wire::nowMicros());
    if (silent_) {
        lucent::info("video", "pictures again after {} packets without one", packetsSincePicture_);
    }
    silent_ = false;
    lastPictureAt_ = std::chrono::steady_clock::now();
    packetsSincePicture_ = 0;
    Frame frame{.number = shows,
                .picture = std::move(picture),
                .capturedUs = static_cast<std::uint64_t>(std::max(times.capturedUs, 0.0)),
                .sentUs = static_cast<std::uint64_t>(std::max(times.sentUs, 0.0)),
                .decodedUs = static_cast<std::uint64_t>(times.decodedUs)};
    {
        const std::lock_guard lock{mutex_};
        if (decoded_ == 0) {
            lucent::info("video", "first picture: {} {}x{}, frame {}", nameOf(codec),
                         frame.picture.width, frame.picture.height, shows);
        }
        ++decoded_;
        if (nameOf(codec) != codec_) {
            switches_ += codec_ != "none" ? 1 : 0;
            codec_ = nameOf(codec);
        }
        last_ = frame;
        playout_.decoded(frame.sentUs, frame.decodedUs);
        fresh_.push_back(std::move(frame));
        while (fresh_.size() > framesHeldMost) {
            letGo(times.decodedUs);
        }
        waiting_.push_back(times);
        if (waiting_.size() > waitingKept) {
            waiting_.pop_front();
        }
    }
    decodedOne_.notify_all();
}

std::optional<GuestVideo::Frame> GuestVideo::takeFrame(std::uint64_t nowUs) {
    const std::lock_guard lock{mutex_};
    if (fresh_.empty()) {
        repeats_ += decoded_ > 0 ? 1 : 0;
        return std::nullopt;
    }
    const std::uint64_t delayUs = playout_.refresh(nowUs);
    const auto due = std::ranges::find_if(fresh_, [nowUs, delayUs](const Frame& frame) {
        return frame.sentUs + delayUs > nowUs;
    });
    if (due == fresh_.begin()) {
        ++repeats_;
        return std::nullopt;
    }
    return handOver(static_cast<std::size_t>(due - fresh_.begin()) - 1, static_cast<double>(nowUs));
}

std::optional<GuestVideo::Frame> GuestVideo::handOver(std::size_t index, double nowUs) {
    for (std::size_t older = 0; older < index; ++older) {
        letGo(nowUs);
    }
    Frame frame = std::move(fresh_.front());
    fresh_.pop_front();
    const auto found = std::ranges::find(waiting_, frame.number, &Times::number);
    if (found != waiting_.end()) {
        found->takenUs = nowUs;
    }
    return frame;
}

void GuestVideo::letGo(double atUs) {
    const Frame& frame = fresh_.front();
    lucent::debug("videotrace", "frame {} decoded {} let go untaken at {:.0f}", frame.number,
                  frame.decodedUs, atUs);
    fresh_.pop_front();
}

void GuestVideo::passed(std::uint64_t beganUs, std::uint64_t acquiredUs,
                        std::uint64_t submittedUs) {
    const std::lock_guard lock{mutex_};
    ++passes_;
    lucent::debug("videotrace", "pass began {} acquired {} submitted {}", beganUs, acquiredUs,
                  submittedUs);
}

std::optional<GuestVideo::Frame> GuestVideo::waitFrame(const std::stop_token& stop) {
    std::unique_lock lock{mutex_};
    if (!decodedOne_.wait(lock, stop, [this] {
            return !fresh_.empty();
        })) {
        return std::nullopt;
    }
    return handOver(fresh_.size() - 1, static_cast<double>(wire::nowMicros()));
}

void GuestVideo::readBack() {
    memory_ = PictureMemory::Cpu;
    lucent::warn("video", "pictures are read back from the decoder from now on");
}

std::optional<GuestVideo::Frame> GuestVideo::lastDecoded() const {
    const std::lock_guard lock{mutex_};
    return last_;
}

void GuestVideo::presented(std::uint32_t number, std::uint64_t submittedUs, bool drawn) {
    const std::lock_guard lock{mutex_};
    const auto found = std::ranges::find(waiting_, number, &Times::number);
    if (found == waiting_.end()) {
        return;
    }
    const Times times = *found;
    waiting_.erase(waiting_.begin(), found + 1);
    lucent::debug("videotrace",
                  "frame {} sent {:.0f} received {:.0f} decoded {:.0f} taken {:.0f} submitted {}",
                  number, times.sentUs, times.receivedUs, times.decodedUs, times.takenUs,
                  submittedUs);
    const auto shown = static_cast<double>(submittedUs);
    shown_.push_back(Stages{.encode = (times.encodedUs - times.capturedUs) / microsPerMilli,
                            .send = (times.sentUs - times.encodedUs) / microsPerMilli,
                            .network = (times.receivedUs - times.sentUs) / microsPerMilli,
                            .decode = (times.decodedUs - times.receivedUs) / microsPerMilli,
                            .submit = (shown - times.decodedUs) / microsPerMilli,
                            .wait = (times.takenUs - times.decodedUs) / microsPerMilli,
                            .show = (shown - times.takenUs) / microsPerMilli,
                            .total = (shown - times.capturedUs) / microsPerMilli});
    if (shown_.size() > framesKept) {
        shown_.pop_front();
    }
    ++presented_;
    if (drawn) {
        ++drawn_;
    }
}

std::string GuestVideo::summary() const {
    const std::lock_guard lock{mutex_};
    std::string out = std::format(
        "codec={} switches={} decoded={} dropped={} late={} submitted={} drawn={} "
        "presents={} repeats={}",
        codec_, switches_, decoded_, dropped_, late_, presented_, drawn_, passes_, repeats_);
    const auto stage = [this, &out](const char* name, double Stages::* field) {
        std::vector<double> values;
        values.reserve(shown_.size());
        for (const Stages& stages : shown_) {
            values.push_back(stages.*field);
        }
        std::ranges::sort(values);
        out += std::format(" {}={:.2f}/{:.2f}", name, clock::percentile(values, 0.5),
                           clock::percentile(values, 0.95));
    };
    if (!shown_.empty()) {
        stage("encode", &Stages::encode);
        stage("send", &Stages::send);
        stage("network", &Stages::network);
        stage("decode", &Stages::decode);
        stage("submit", &Stages::submit);
        stage("wait", &Stages::wait);
        stage("show", &Stages::show);
        stage("total", &Stages::total);
    }
    return out;
}

} // namespace opensu::stream
