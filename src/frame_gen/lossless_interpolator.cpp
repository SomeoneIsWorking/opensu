#include <algorithm>
#include <utility>

#include "frame_gen/interpolator.hpp"

namespace opensu::frame_gen {
namespace {

class LosslessInterpolator final : public Interpolator {
  public:
    LosslessInterpolator(std::shared_ptr<framegen::CDevice> device,
                         std::unique_ptr<framegen::CLosslessGenerator> generator)
        : device_{std::move(device)}, generator_{std::move(generator)} {
    }

    bool begin(std::uint32_t width, std::uint32_t height, SourcePixel /*pixel*/) override {
        return generator_->BeginSession(width, height);
    }
    std::optional<framegen::Submission_t> add(const framegen::GeneratorImage_t& frame) override {
        return generator_->AddRealFrame(frame);
    }
    std::optional<framegen::Submission_t> generate(float t,
                                                   const framegen::GeneratorImage_t& output,
                                                   const framegen::OutputHooks_t& hooks) override {
        return generator_->Generate(t, output, hooks);
    }

  private:
    std::shared_ptr<framegen::CDevice> device_;
    std::unique_ptr<framegen::CLosslessGenerator> generator_;
};

} // namespace

std::unique_ptr<Interpolator> lossless(GeneratorDevice& gpu,
                                       const settings::FrameGenSettings& settings,
                                       LosslessSource source, std::string& why) {
    std::shared_ptr<framegen::CDevice> device = framegen::CDevice::Create(gpu.desc(), why);
    if (!device) {
        return nullptr;
    }
    constexpr float percent = 100.0F;
    std::unique_ptr<framegen::CLosslessGenerator> generator = framegen::CLosslessGenerator::Create(
        device,
        framegen::LosslessSettings_t{
            .sDllPath = source.dll.string(),
            .pModules = std::move(source.modules),
            .eSizeMode = settings.performance ? framegen::LosslessSizeMode::Performance
                                              : framegen::LosslessSizeMode::Balanced,
            .flFlowScale = std::clamp(static_cast<float>(settings.flowScalePercent) / percent,
                                      framegen::k_flLosslessMinFlowScale, 1.0F),
            .bUiDetection = true},
        why);
    if (!generator) {
        return nullptr;
    }
    return std::make_unique<LosslessInterpolator>(std::move(device), std::move(generator));
}

} // namespace opensu::frame_gen
