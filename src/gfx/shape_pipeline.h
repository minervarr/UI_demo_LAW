#pragma once
#include <vulkan/vulkan.h>
#include "primitives.h"

class ShapePipeline {
 public:
    void init(VkDevice device, VkPhysicalDevice physicalDevice, VkRenderPass renderPass);
    void draw(VkCommandBuffer cmd, VkExtent2D extent, const PrimitiveBatch& batch);
    void cleanup(VkDevice device);

 private:
    static constexpr uint32_t kMaxVerts = 65536;
    VkPipelineLayout layout_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    VkBuffer vertexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory vertexMemory_ = VK_NULL_HANDLE;
    void* mapped_ = nullptr;
    VkDevice device_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
};
