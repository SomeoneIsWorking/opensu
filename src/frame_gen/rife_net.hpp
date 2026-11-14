// rife_net — RIFE's graph (`ncnn_model`) run on a `GeneratorDevice`: every blob laid out in one
// device buffer for an input size, the weights in another, and each layer recorded as one compute
// dispatch or buffer copy (`shaders/rife_*.comp`), in the graph's order with a barrier between.
#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#include "frame_gen/compute_pass.hpp"
#include "frame_gen/generator_gpu.hpp"
#include "frame_gen/ncnn_model.hpp"

namespace opensu::frame_gen {

class RifeNet {
  public:
    /// Where a blob lies in the blobs' buffer, in floats, and its sides.
    struct Shape {
        std::int32_t offset{0};
        std::int32_t width{0};
        std::int32_t height{0};
        std::int32_t channels{0};
    };

    /// Throws when the device cannot make the passes or hold the weights.
    RifeNet(GeneratorDevice& gpu, ncnn::Model model);
    ~RifeNet();
    RifeNet(const RifeNet&) = delete;
    RifeNet& operator=(const RifeNet&) = delete;
    RifeNet(RifeNet&&) = delete;
    RifeNet& operator=(RifeNet&&) = delete;

    /// Lays the blobs out for frames of `width` by `height` (already padded to the model's
    /// multiple): `in0` and `in1` RGB, `in2` the time; throws on a shape a layer cannot take.
    void size(std::uint32_t width, std::uint32_t height);

    /// Records the graph from `in0`, `in1` and `in2` to `out0`, each layer type's work added to
    /// its entry in `work`.
    void record(VkCommandBuffer cmd, std::vector<NamedWork>& work) const;
    /// Records `value` into every float of `blob`.
    void fill(VkCommandBuffer cmd, int blob, float value) const;
    /// Records a copy of blob `from` into `to`, of the same shape.
    void copy(VkCommandBuffer cmd, int from, int to) const;

    [[nodiscard]] const ncnn::Model& model() const noexcept {
        return model_;
    }
    /// What `size`'s sides must be a multiple of: the graph's deepest downscale of its input.
    [[nodiscard]] std::uint32_t multiple() const noexcept {
        return multiple_;
    }
    [[nodiscard]] Shape shape(int blob) const;
    /// The blobs' set: binding 0 the blobs, 1 the weights.
    [[nodiscard]] VkDescriptorSetLayout setLayout() const noexcept {
        return setLayout_.get();
    }
    [[nodiscard]] VkDescriptorSet set() const noexcept {
        return set_;
    }

    /// `values` written into `blob`, waiting for the device (for tests).
    void write(int blob, std::span<const float> values);
    /// `blob`'s values read back, waiting for the device (for tests).
    [[nodiscard]] std::vector<float> read(int blob);

  private:
    struct Step;
    void upload(VkBuffer target, VkDeviceSize at, std::span<const float> values);

    GeneratorDevice& gpu_;
    VkDevice device_;
    ncnn::Model model_;
    /// Each layer's weights and bias offsets in the weights' buffer, -1 for none.
    std::vector<std::pair<std::int32_t, std::int32_t>> weightAt_;
    std::uint32_t multiple_{1};
    SetLayout setLayout_;
    Pass conv_;
    Pass deconv_;
    Pass interp_;
    Pass warp_;
    Pass binary_;
    Pass unary_;
    Pass shuffle_;
    Memory weightsMemory_;
    Buffer weights_;
    Memory blobsMemory_;
    Buffer blobs_;
    VkDeviceSize blobBytes_{0};
    Pool pool_;
    VkDescriptorSet set_{VK_NULL_HANDLE};
    std::vector<Shape> shapes_;
    std::vector<std::unique_ptr<Step>> steps_;
};

} // namespace opensu::frame_gen
