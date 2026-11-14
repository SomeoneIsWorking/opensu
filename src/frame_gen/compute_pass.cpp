#include "frame_gen/compute_pass.hpp"

#include <vector>

#include "frame_gen/generator_gpu.hpp"

namespace opensu::frame_gen {

SetLayout setLayout(VkDevice device, VkDescriptorType type, std::uint32_t count) {
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    bindings.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        bindings.push_back(VkDescriptorSetLayoutBinding{.binding = i,
                                                        .descriptorType = type,
                                                        .descriptorCount = 1,
                                                        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                                                        .pImmutableSamplers = nullptr});
    }
    const VkDescriptorSetLayoutCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .bindingCount = count,
        .pBindings = bindings.data()};
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    check(vkCreateDescriptorSetLayout(device, &info, nullptr, &layout),
          "vkCreateDescriptorSetLayout");
    return SetLayout{device, layout};
}

Pass computePass(VkDevice device, std::span<const std::uint32_t> code,
                 std::span<const VkDescriptorSetLayout> sets, std::uint32_t pushBytes,
                 const VkSpecializationInfo* specialization, std::uint32_t threads) {
    const VkPushConstantRange push{
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT, .offset = 0, .size = pushBytes};
    const VkPipelineLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .setLayoutCount = static_cast<std::uint32_t>(sets.size()),
        .pSetLayouts = sets.data(),
        .pushConstantRangeCount = pushBytes > 0 ? 1U : 0U,
        .pPushConstantRanges = &push};
    VkPipelineLayout layout = VK_NULL_HANDLE;
    check(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &layout), "vkCreatePipelineLayout");
    Pass pass{.layout = PipelineLayout{device, layout}, .pipeline = {}, .threads = threads};
    const VkShaderModuleCreateInfo moduleInfo{.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                              .pNext = nullptr,
                                              .flags = 0,
                                              .codeSize = code.size_bytes(),
                                              .pCode = code.data()};
    VkShaderModule module = VK_NULL_HANDLE;
    check(vkCreateShaderModule(device, &moduleInfo, nullptr, &module), "vkCreateShaderModule");
    const VkComputePipelineCreateInfo pipelineInfo{
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stage = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                  .pNext = nullptr,
                  .flags = 0,
                  .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                  .module = module,
                  .pName = "main",
                  .pSpecializationInfo = specialization},
        .layout = layout,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1};
    VkPipeline pipeline = VK_NULL_HANDLE;
    const VkResult made =
        vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);
    vkDestroyShaderModule(device, module, nullptr);
    check(made, "vkCreateComputePipelines");
    pass.pipeline = Pipeline{device, pipeline};
    return pass;
}

ViewSets::ViewSets(VkDevice device, VkDescriptorSetLayout layout)
    : device_{device}, layout_{layout} {
    const VkDescriptorPoolSize size{.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                    .descriptorCount = capacity};
    const VkDescriptorPoolCreateInfo info{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                          .pNext = nullptr,
                                          .flags = 0,
                                          .maxSets = capacity,
                                          .poolSizeCount = 1,
                                          .pPoolSizes = &size};
    VkDescriptorPool pool = VK_NULL_HANDLE;
    check(vkCreateDescriptorPool(device, &info, nullptr, &pool), "vkCreateDescriptorPool");
    pool_ = Pool{device, pool};
}

VkDescriptorSet ViewSets::set(VkImageView view) {
    if (const auto known = sets_.find(view); known != sets_.end()) {
        return known->second;
    }
    if (sets_.size() >= capacity) {
        return VK_NULL_HANDLE;
    }
    const VkDescriptorSetAllocateInfo allocate{.sType =
                                                   VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                               .pNext = nullptr,
                                               .descriptorPool = pool_.get(),
                                               .descriptorSetCount = 1,
                                               .pSetLayouts = &layout_};
    VkDescriptorSet set = VK_NULL_HANDLE;
    if (vkAllocateDescriptorSets(device_, &allocate, &set) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }
    const VkDescriptorImageInfo image{
        .sampler = VK_NULL_HANDLE, .imageView = view, .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
    const VkWriteDescriptorSet write{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                     .pNext = nullptr,
                                     .dstSet = set,
                                     .dstBinding = 0,
                                     .dstArrayElement = 0,
                                     .descriptorCount = 1,
                                     .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                     .pImageInfo = &image,
                                     .pBufferInfo = nullptr,
                                     .pTexelBufferView = nullptr};
    vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
    sets_.emplace(view, set);
    return set;
}

void ViewSets::clear() {
    check(vkResetDescriptorPool(device_, pool_.get(), 0), "vkResetDescriptorPool");
    sets_.clear();
}

void computeBarrier(VkCommandBuffer cmd) {
    memoryBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                  VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                  VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
}

void reportWork(GpuSpans& spans, const std::string& key, std::span<const NamedWork> parts) {
    GpuWork total;
    for (const NamedWork& part : parts) {
        spans.work(key + " " + part.name, part.work);
        total += part.work;
    }
    spans.work(key, total);
}

} // namespace opensu::frame_gen
