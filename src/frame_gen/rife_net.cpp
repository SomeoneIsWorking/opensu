#include "frame_gen/rife_net.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <climits>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>

namespace opensu::frame_gen {
namespace {

constexpr std::uint32_t convCode[] = {
#include "rife_conv.comp.inc"
};
constexpr std::uint32_t deconvCode[] = {
#include "rife_deconv.comp.inc"
};
constexpr std::uint32_t interpCode[] = {
#include "rife_interp.comp.inc"
};
constexpr std::uint32_t warpCode[] = {
#include "rife_warp.comp.inc"
};
constexpr std::uint32_t binaryCode[] = {
#include "rife_binary.comp.inc"
};
constexpr std::uint32_t unaryCode[] = {
#include "rife_unary.comp.inc"
};
constexpr std::uint32_t shuffleCode[] = {
#include "rife_shuffle.comp.inc"
};

using Ivec4 = std::array<std::int32_t, 4>;

// Push constants of `shaders/rife_*.comp`, laid out as std430 lays them.
struct ConvPush {
    Ivec4 src;
    Ivec4 dst;
    std::int32_t weight;
    std::int32_t bias;
    std::int32_t kernel;
    std::int32_t stride;
    std::int32_t pad;
    float slope;
};
struct DeconvPush {
    Ivec4 src;
    Ivec4 dst;
    std::int32_t weight;
    std::int32_t bias;
    std::int32_t kernel;
    std::int32_t stride;
    std::int32_t pad;
};
struct InterpPush {
    Ivec4 src;
    Ivec4 dst;
    std::array<float, 2> scale;
};
struct WarpPush {
    Ivec4 image;
    std::int32_t flow;
    std::int32_t dst;
};
struct BinaryPush {
    Ivec4 a;
    Ivec4 b;
    Ivec4 dst;
    std::int32_t op;
    float scalar;
    float aWeight;
    float bWeight;
};
struct UnaryPush {
    std::int32_t src;
    std::int32_t dst;
    std::int32_t count;
    std::int32_t sigmoid;
    float slope;
};
struct ShufflePush {
    Ivec4 src;
    std::int32_t dst;
    std::int32_t factor;
};

constexpr std::uint32_t groupSide = 8;
constexpr std::uint32_t unaryGroup = 64;
/// Blobs start this many floats apart, so every one is a copy offset any device takes.
constexpr std::int32_t blobAlign = 64;

Ivec4 vec(const RifeNet::Shape& shape) {
    return {shape.offset, shape.width, shape.height, shape.channels};
}

std::uint64_t floats(const RifeNet::Shape& shape) {
    return std::uint64_t{static_cast<std::uint32_t>(shape.width)} *
           static_cast<std::uint32_t>(shape.height) * static_cast<std::uint32_t>(shape.channels);
}

std::uint32_t groups(std::int32_t count, std::uint32_t side) {
    return (static_cast<std::uint32_t>(count) + side - 1) / side;
}

[[noreturn]] void fail(const ncnn::Layer& layer, const std::string& what) {
    throw std::runtime_error{layer.name + " (" + layer.type + "): " + what};
}

void barrier(VkCommandBuffer cmd) {
    memoryBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
                  VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
                  VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
                  VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT |
                      VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
}

/// How many input pixels one pixel of the graph's smallest blob covers, along a side.
std::uint32_t downscale(const ncnn::Model& model) {
    std::vector<double> scale(model.blobs.size(), 0.0);
    double deepest = 1.0;
    for (const ncnn::Layer& layer : model.layers) {
        double in = layer.type == "Input" ? 1.0 : 0.0;
        for (const int b : layer.bottoms) {
            in = std::max(in, scale[static_cast<std::size_t>(b)]);
        }
        const ncnn::Params& p = layer.params;
        double out = in;
        if (layer.type == "Convolution") {
            out = in * p.integer(3, 1);
        } else if (layer.type == "Deconvolution") {
            out = in / p.integer(3, 1);
        } else if (layer.type == "PixelShuffle") {
            out = in / p.integer(0, 1);
        } else if (layer.type == "Interp") {
            out = in / std::min(p.real(1, 1), p.real(2, 1));
        }
        for (const int t : layer.tops) {
            scale[static_cast<std::size_t>(t)] = out;
        }
        deepest = std::max(deepest, out);
    }
    return static_cast<std::uint32_t>(std::lround(deepest));
}

std::pair<Buffer, Memory> deviceBuffer(const GeneratorDevice& gpu, VkDeviceSize bytes) {
    const VkBufferCreateInfo info{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                  .pNext = nullptr,
                                  .flags = 0,
                                  .size = bytes,
                                  .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                           VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                           VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                  .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                  .queueFamilyIndexCount = 0,
                                  .pQueueFamilyIndices = nullptr};
    VkBuffer buffer = VK_NULL_HANDLE;
    check(vkCreateBuffer(gpu.device(), &info, nullptr, &buffer), "vkCreateBuffer");
    Buffer owned{gpu.device(), buffer};
    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(gpu.device(), buffer, &requirements);
    Memory memory{gpu.device(), gpu.allocate(requirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)};
    check(vkBindBufferMemory(gpu.device(), buffer, memory.get(), 0), "vkBindBufferMemory");
    return {std::move(owned), std::move(memory)};
}

} // namespace

/// One recorded layer: a dispatch of a pass with its push constants, or buffer copies.
struct RifeNet::Step {
    std::string type;
    const Pass* pass{nullptr};
    std::vector<std::uint8_t> push;
    std::array<std::uint32_t, 3> groups{1, 1, 1};
    std::vector<VkBufferCopy> copies;
    GpuWork work;
};

RifeNet::RifeNet(GeneratorDevice& gpu, ncnn::Model model)
    : gpu_{gpu}, device_{gpu.device()}, model_{std::move(model)},
      setLayout_{frame_gen::setLayout(device_, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2)} {
    const std::array<VkDescriptorSetLayout, 1> sets{setLayout_.get()};
    conv_ = computePass(device_, convCode, sets, sizeof(ConvPush), nullptr, groupSide * groupSide);
    deconv_ =
        computePass(device_, deconvCode, sets, sizeof(DeconvPush), nullptr, groupSide * groupSide);
    interp_ =
        computePass(device_, interpCode, sets, sizeof(InterpPush), nullptr, groupSide * groupSide);
    warp_ = computePass(device_, warpCode, sets, sizeof(WarpPush), nullptr, groupSide * groupSide);
    binary_ =
        computePass(device_, binaryCode, sets, sizeof(BinaryPush), nullptr, groupSide * groupSide);
    unary_ = computePass(device_, unaryCode, sets, sizeof(UnaryPush), nullptr, unaryGroup);
    shuffle_ = computePass(device_, shuffleCode, sets, sizeof(ShufflePush), nullptr,
                           groupSide * groupSide);

    multiple_ = downscale(model_);
    std::vector<float> weights;
    for (const ncnn::Layer& layer : model_.layers) {
        std::pair<std::int32_t, std::int32_t> at{-1, -1};
        if (layer.type == "Convolution" || layer.type == "Deconvolution") {
            at.first = static_cast<std::int32_t>(weights.size());
            weights.insert(weights.end(), layer.weight.begin(), layer.weight.end());
            if (!layer.bias.empty()) {
                at.second = static_cast<std::int32_t>(weights.size());
                weights.insert(weights.end(), layer.bias.begin(), layer.bias.end());
            }
        }
        weightAt_.push_back(at);
    }
    auto [buffer, memory] = deviceBuffer(gpu_, std::max<VkDeviceSize>(weights.size(), 1) * 4);
    weights_ = std::move(buffer);
    weightsMemory_ = std::move(memory);
    upload(weights_.get(), 0, weights);

    const VkDescriptorPoolSize size{.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                    .descriptorCount = 2};
    const VkDescriptorPoolCreateInfo poolInfo{.sType =
                                                  VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                              .pNext = nullptr,
                                              .flags = 0,
                                              .maxSets = 1,
                                              .poolSizeCount = 1,
                                              .pPoolSizes = &size};
    VkDescriptorPool pool = VK_NULL_HANDLE;
    check(vkCreateDescriptorPool(device_, &poolInfo, nullptr, &pool), "vkCreateDescriptorPool");
    pool_ = Pool{device_, pool};
    const VkDescriptorSetLayout layout = setLayout_.get();
    const VkDescriptorSetAllocateInfo allocate{.sType =
                                                   VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                               .pNext = nullptr,
                                               .descriptorPool = pool,
                                               .descriptorSetCount = 1,
                                               .pSetLayouts = &layout};
    check(vkAllocateDescriptorSets(device_, &allocate, &set_), "vkAllocateDescriptorSets");
}

RifeNet::~RifeNet() {
    static_cast<void>(gpu_.idle());
}

RifeNet::Shape RifeNet::shape(int blob) const {
    return shapes_.at(static_cast<std::size_t>(blob));
}

void RifeNet::size(std::uint32_t width, std::uint32_t height) {
    if (gpu_.idle() != VK_SUCCESS) {
        throw std::runtime_error{"the generator device is lost"};
    }
    steps_.clear();
    shapes_.assign(model_.blobs.size(), Shape{});
    std::vector<bool> known(model_.blobs.size(), false);
    std::int32_t next = 0;
    const auto place = [&](int blob, std::int32_t w, std::int32_t h, std::int32_t c) {
        Shape& s = shapes_[static_cast<std::size_t>(blob)];
        s = Shape{.offset = next, .width = w, .height = h, .channels = c};
        known[static_cast<std::size_t>(blob)] = true;
        const auto count = static_cast<std::int64_t>(w) * h * c;
        const std::int64_t end = next + (count + blobAlign - 1) / blobAlign * blobAlign;
        if (end > INT_MAX) {
            throw std::runtime_error{"RIFE's blobs do not fit at this size"};
        }
        next = static_cast<std::int32_t>(end);
        return s;
    };
    const auto w = static_cast<std::int32_t>(width);
    const auto h = static_cast<std::int32_t>(height);
    std::vector<std::pair<int, std::span<const float>>> constants;

    for (std::size_t index = 0; index < model_.layers.size(); ++index) {
        const ncnn::Layer& layer = model_.layers[index];
        std::vector<Shape> in;
        for (const int b : layer.bottoms) {
            if (!known[static_cast<std::size_t>(b)]) {
                fail(layer, "input " + model_.blobs[static_cast<std::size_t>(b)] + " is not made");
            }
            in.push_back(shapes_[static_cast<std::size_t>(b)]);
        }
        const ncnn::Params& p = layer.params;
        const int top = layer.tops.empty() ? -1 : layer.tops.front();
        Step step;
        step.type = layer.type;
        const auto dispatch = [&](const Pass& pass, const auto& push, std::uint32_t x,
                                  std::uint32_t y, std::uint32_t z, std::uint64_t read,
                                  std::uint64_t written) {
            step.pass = &pass;
            step.push.resize(sizeof(push));
            std::memcpy(step.push.data(), &push, sizeof(push));
            step.groups = {x, y, z};
            step.work = GpuWork{.dispatches = 1,
                                .invocations = std::uint64_t{x} * y * z * pass.threads,
                                .bytesRead = read * 4,
                                .bytesWritten = written * 4,
                                .texelReads = 0,
                                .sharedReads = 0};
        };
        const std::string& type = layer.type;
        if (type == "Input") {
            const std::string& name = model_.blobs[static_cast<std::size_t>(top)];
            place(top, w, h, name == "in2" ? 1 : 3);
            continue;
        }
        if (type == "Split") {
            for (const int t : layer.tops) {
                shapes_[static_cast<std::size_t>(t)] = in.at(0);
                known[static_cast<std::size_t>(t)] = true;
            }
            continue;
        }
        if (type == "MemoryData") {
            const Shape s = place(top, p.integer(0, 1), std::max(p.integer(1, 1), 1),
                                  std::max(p.integer(2, 1), 1));
            if (floats(s) != layer.weight.size()) {
                fail(layer, "its constant does not fill its shape");
            }
            constants.emplace_back(top, layer.weight);
            continue;
        }
        if (type == "Convolution" || type == "Deconvolution") {
            const Shape& src = in.at(0);
            const std::int32_t k = p.integer(1, 1);
            const std::int32_t stride = p.integer(3, 1);
            const std::int32_t pad = p.integer(4, 0);
            const std::int32_t outc = p.integer(0, 0);
            if (layer.weight.size() != static_cast<std::size_t>(outc) * src.channels * k * k) {
                fail(layer, "weights do not match the input's channels");
            }
            const auto [weight, bias] = weightAt_[index];
            if (type == "Convolution") {
                const Shape dst = place(top, ((src.width + (2 * pad) - k) / stride) + 1,
                                        ((src.height + (2 * pad) - k) / stride) + 1, outc);
                const ConvPush push{
                    .src = vec(src),
                    .dst = vec(dst),
                    .weight = weight,
                    .bias = bias,
                    .kernel = k,
                    .stride = stride,
                    .pad = pad,
                    .slope = p.integer(9, 0) == 2 ? static_cast<float>(p.array(10).at(0)) : 1.0F};
                dispatch(conv_, push, groups(dst.width, groupSide), groups(dst.height, groupSide),
                         static_cast<std::uint32_t>(outc), floats(src) + layer.weight.size(),
                         floats(dst));
            } else {
                const Shape dst = place(top, ((src.width - 1) * stride) + k - (2 * pad),
                                        ((src.height - 1) * stride) + k - (2 * pad), outc);
                const DeconvPush push{.src = vec(src),
                                      .dst = vec(dst),
                                      .weight = weight,
                                      .bias = bias,
                                      .kernel = k,
                                      .stride = stride,
                                      .pad = pad};
                dispatch(deconv_, push, groups(dst.width, groupSide), groups(dst.height, groupSide),
                         static_cast<std::uint32_t>(outc), floats(src) + layer.weight.size(),
                         floats(dst));
            }
        } else if (type == "Interp") {
            const Shape& src = in.at(0);
            const float hScale = p.real(1, 1);
            const float wScale = p.real(2, 1);
            const auto outw = static_cast<std::int32_t>(static_cast<float>(src.width) * wScale);
            const auto outh = static_cast<std::int32_t>(static_cast<float>(src.height) * hScale);
            if (outw == src.width && outh == src.height) {
                shapes_[static_cast<std::size_t>(top)] = src;
                known[static_cast<std::size_t>(top)] = true;
                continue;
            }
            if (src.width < 2 || src.height < 2) {
                fail(layer, "an input under 2 pixels is not resized");
            }
            const Shape dst = place(top, outw, outh, src.channels);
            const InterpPush push{.src = vec(src),
                                  .dst = vec(dst),
                                  .scale = {outw == src.width ? 1.0F : 1.0F / wScale,
                                            outh == src.height ? 1.0F : 1.0F / hScale}};
            dispatch(interp_, push, groups(outw, groupSide), groups(outh, groupSide),
                     static_cast<std::uint32_t>(src.channels), floats(src), floats(dst));
        } else if (type == "rife.Warp") {
            const Shape& image = in.at(0);
            const Shape& flow = in.at(1);
            if (flow.width != image.width || flow.height != image.height || flow.channels < 2) {
                fail(layer, "a flow of another size");
            }
            const Shape dst = place(top, image.width, image.height, image.channels);
            const WarpPush push{.image = vec(image), .flow = flow.offset, .dst = dst.offset};
            dispatch(warp_, push, groups(image.width, groupSide), groups(image.height, groupSide),
                     static_cast<std::uint32_t>(image.channels),
                     floats(image) + (2 * floats(dst) / image.channels), floats(dst));
        } else if (type == "BinaryOp" || type == "Eltwise") {
            const Shape& a = in.at(0);
            const bool scalar = in.size() == 1;
            const Shape b =
                scalar ? Shape{.offset = -1, .width = 1, .height = 1, .channels = 1} : in.at(1);
            const auto side = [&](std::int32_t x, std::int32_t y) {
                if (x != y && x != 1 && y != 1) {
                    fail(layer, "shapes that do not broadcast");
                }
                return std::max(x, y);
            };
            const Shape dst = place(top, side(a.width, b.width), side(a.height, b.height),
                                    side(a.channels, b.channels));
            BinaryPush push{.a = vec(a),
                            .b = vec(b),
                            .dst = vec(dst),
                            .op = 0,
                            .scalar = 0,
                            .aWeight = 1,
                            .bWeight = 1};
            if (type == "BinaryOp") {
                push.op = p.integer(0, 0);
                push.scalar = p.real(2, 0);
            } else {
                const std::vector<double> coeffs = p.array(1);
                if (!coeffs.empty()) {
                    push.aWeight = static_cast<float>(coeffs.at(0));
                    push.bWeight = static_cast<float>(coeffs.at(1));
                }
            }
            dispatch(binary_, push, groups(dst.width, groupSide), groups(dst.height, groupSide),
                     static_cast<std::uint32_t>(dst.channels), floats(a) + (scalar ? 0 : floats(b)),
                     floats(dst));
        } else if (type == "ReLU" || type == "Sigmoid") {
            const Shape& src = in.at(0);
            const Shape dst = place(top, src.width, src.height, src.channels);
            const auto count = static_cast<std::int32_t>(floats(src));
            const UnaryPush push{.src = src.offset,
                                 .dst = dst.offset,
                                 .count = count,
                                 .sigmoid = type == "Sigmoid" ? 1 : 0,
                                 .slope = p.real(0, 0)};
            dispatch(unary_, push, groups(count, unaryGroup), 1, 1, floats(src), floats(dst));
        } else if (type == "PixelShuffle") {
            const Shape& src = in.at(0);
            const std::int32_t r = p.integer(0, 1);
            const Shape dst = place(top, src.width * r, src.height * r, src.channels / (r * r));
            const ShufflePush push{.src = vec(src), .dst = dst.offset, .factor = r};
            dispatch(shuffle_, push, groups(dst.width, groupSide), groups(dst.height, groupSide),
                     static_cast<std::uint32_t>(dst.channels), floats(src), floats(dst));
        } else if (type == "Crop") {
            const Shape& src = in.at(0);
            auto start = static_cast<std::int32_t>(p.array(9).at(0));
            const double rawEnd = p.array(10).at(0);
            std::int32_t end = rawEnd >= INT_MAX ? src.channels : static_cast<std::int32_t>(rawEnd);
            start = start < 0 ? start + src.channels : start;
            end = end < 0 ? end + src.channels : std::min(end, src.channels);
            if (start < 0 || end <= start) {
                fail(layer, "an empty channel range");
            }
            const Shape dst = place(top, src.width, src.height, end - start);
            const VkDeviceSize plane = VkDeviceSize{static_cast<std::uint32_t>(src.width)} *
                                       static_cast<std::uint32_t>(src.height) * 4;
            step.copies.push_back(VkBufferCopy{
                .srcOffset = (VkDeviceSize{static_cast<std::uint32_t>(src.offset)} * 4) +
                             (plane * static_cast<std::uint32_t>(start)),
                .dstOffset = VkDeviceSize{static_cast<std::uint32_t>(dst.offset)} * 4,
                .size = plane * static_cast<std::uint32_t>(end - start)});
            step.work.bytesRead = floats(dst) * 4;
            step.work.bytesWritten = floats(dst) * 4;
        } else if (type == "Concat") {
            std::int32_t channels = 0;
            for (const Shape& s : in) {
                if (s.width != in.front().width || s.height != in.front().height) {
                    fail(layer, "inputs of different sizes");
                }
                channels += s.channels;
            }
            const Shape dst = place(top, in.front().width, in.front().height, channels);
            VkDeviceSize at = VkDeviceSize{static_cast<std::uint32_t>(dst.offset)} * 4;
            for (const Shape& s : in) {
                const VkDeviceSize bytes = floats(s) * 4;
                step.copies.push_back(VkBufferCopy{
                    .srcOffset = VkDeviceSize{static_cast<std::uint32_t>(s.offset)} * 4,
                    .dstOffset = at,
                    .size = bytes});
                at += bytes;
            }
            step.work.bytesRead = floats(dst) * 4;
            step.work.bytesWritten = floats(dst) * 4;
        } else {
            fail(layer, "not run");
        }
        steps_.push_back(std::make_unique<Step>(std::move(step)));
    }

    blobBytes_ = std::max<VkDeviceSize>(static_cast<VkDeviceSize>(next), 1) * 4;
    blobs_ = Buffer{};
    blobsMemory_ = Memory{};
    auto [buffer, memory] = deviceBuffer(gpu_, blobBytes_);
    blobs_ = std::move(buffer);
    blobsMemory_ = std::move(memory);
    const std::array<VkDescriptorBufferInfo, 2> infos{
        VkDescriptorBufferInfo{.buffer = blobs_.get(), .offset = 0, .range = VK_WHOLE_SIZE},
        VkDescriptorBufferInfo{.buffer = weights_.get(), .offset = 0, .range = VK_WHOLE_SIZE}};
    std::array<VkWriteDescriptorSet, 2> writes{};
    for (std::uint32_t i = 0; i < writes.size(); ++i) {
        writes[i] = VkWriteDescriptorSet{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                         .pNext = nullptr,
                                         .dstSet = set_,
                                         .dstBinding = i,
                                         .dstArrayElement = 0,
                                         .descriptorCount = 1,
                                         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                         .pImageInfo = nullptr,
                                         .pBufferInfo = &infos[i],
                                         .pTexelBufferView = nullptr};
    }
    vkUpdateDescriptorSets(device_, static_cast<std::uint32_t>(writes.size()), writes.data(), 0,
                           nullptr);
    for (const auto& [blob, values] : constants) {
        upload(blobs_.get(),
               VkDeviceSize{
                   static_cast<std::uint32_t>(shapes_[static_cast<std::size_t>(blob)].offset)} *
                   4,
               values);
    }
}

void RifeNet::record(VkCommandBuffer cmd, std::vector<NamedWork>& work) const {
    for (const auto& step : steps_) {
        barrier(cmd);
        if (step->pass != nullptr) {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, step->pass->pipeline.get());
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, step->pass->layout.get(),
                                    0, 1, &set_, 0, nullptr);
            vkCmdPushConstants(cmd, step->pass->layout.get(), VK_SHADER_STAGE_COMPUTE_BIT, 0,
                               static_cast<std::uint32_t>(step->push.size()), step->push.data());
            vkCmdDispatch(cmd, step->groups[0], step->groups[1], step->groups[2]);
        } else {
            vkCmdCopyBuffer(cmd, blobs_.get(), blobs_.get(),
                            static_cast<std::uint32_t>(step->copies.size()), step->copies.data());
        }
        const auto same = std::ranges::find(work, step->type, &NamedWork::name);
        if (same == work.end()) {
            work.push_back(NamedWork{.name = step->type, .work = step->work});
        } else {
            same->work += step->work;
        }
    }
    barrier(cmd);
}

void RifeNet::fill(VkCommandBuffer cmd, int blob, float value) const {
    const Shape& s = shapes_.at(static_cast<std::size_t>(blob));
    vkCmdFillBuffer(cmd, blobs_.get(), VkDeviceSize{static_cast<std::uint32_t>(s.offset)} * 4,
                    floats(s) * 4, std::bit_cast<std::uint32_t>(value));
}

void RifeNet::copy(VkCommandBuffer cmd, int from, int to) const {
    const Shape& a = shapes_.at(static_cast<std::size_t>(from));
    const Shape& b = shapes_.at(static_cast<std::size_t>(to));
    if (floats(a) != floats(b)) {
        throw std::invalid_argument{"a copy between blobs of different sizes"};
    }
    const VkBufferCopy region{.srcOffset = VkDeviceSize{static_cast<std::uint32_t>(a.offset)} * 4,
                              .dstOffset = VkDeviceSize{static_cast<std::uint32_t>(b.offset)} * 4,
                              .size = floats(a) * 4};
    vkCmdCopyBuffer(cmd, blobs_.get(), blobs_.get(), 1, &region);
}

void RifeNet::upload(VkBuffer target, VkDeviceSize at, std::span<const float> values) {
    if (values.empty()) {
        return;
    }
    const HostBuffer staging{gpu_, values.size_bytes(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT};
    std::memcpy(staging.bytes().data(), values.data(), values.size_bytes());
    VkCommandBuffer cmd = gpu_.Begin();
    if (cmd == VK_NULL_HANDLE) {
        throw std::runtime_error{"no command buffer for RIFE's upload"};
    }
    const VkBufferCopy region{.srcOffset = 0, .dstOffset = at, .size = values.size_bytes()};
    vkCmdCopyBuffer(cmd, staging.buffer(), target, 1, &region);
    barrier(cmd);
    gpu_.Wait(gpu_.Submit(cmd));
}

void RifeNet::write(int blob, std::span<const float> values) {
    const Shape& s = shapes_.at(static_cast<std::size_t>(blob));
    if (values.size() != floats(s)) {
        throw std::invalid_argument{"values of another size than the blob"};
    }
    upload(blobs_.get(), VkDeviceSize{static_cast<std::uint32_t>(s.offset)} * 4, values);
}

std::vector<float> RifeNet::read(int blob) {
    const Shape& s = shapes_.at(static_cast<std::size_t>(blob));
    std::vector<float> values(floats(s));
    if (values.empty()) {
        return values;
    }
    const HostBuffer staging{gpu_, values.size() * 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT};
    VkCommandBuffer cmd = gpu_.Begin();
    if (cmd == VK_NULL_HANDLE) {
        throw std::runtime_error{"no command buffer for RIFE's read"};
    }
    barrier(cmd);
    const VkBufferCopy region{.srcOffset = VkDeviceSize{static_cast<std::uint32_t>(s.offset)} * 4,
                              .dstOffset = 0,
                              .size = values.size() * 4};
    vkCmdCopyBuffer(cmd, blobs_.get(), staging.buffer(), 1, &region);
    memoryBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                  VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_HOST_READ_BIT);
    gpu_.Wait(gpu_.Submit(cmd));
    std::memcpy(values.data(), staging.bytes().data(), values.size() * 4);
    return values;
}

} // namespace opensu::frame_gen
