#include "shape_pipeline.h"
#include "../platform/fatal.h"
#include "../platform/paths.h"
#include <algorithm>
#include <fstream>
#include <vector>
#include <cstring>
#include <stdexcept>
#include <cstdio>

static std::vector<char> readFile(const char* path) {
    std::ifstream f(path, std::ios::ate | std::ios::binary);
    if (!f) throw std::runtime_error(std::string("failed to open shader: ") + path);
    size_t size = (size_t)f.tellg();
    std::vector<char> buf(size);
    f.seekg(0);
    f.read(buf.data(), size);
    return buf;
}

static VkShaderModule loadShaderModule(VkDevice device, const char* path) {
    auto code = readFile(path);
    VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    ci.codeSize = code.size();
    ci.pCode = reinterpret_cast<const uint32_t*>(code.data());
    VkShaderModule module;
    vkCreateShaderModule(device, &ci, nullptr, &module);
    return module;
}

static uint32_t findMemoryType(VkPhysicalDevice phys, uint32_t typeBits, VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(phys, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        if ((typeBits & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props)
            return i;
    }
    return UINT32_MAX;
}

void ShapePipeline::init(VkDevice device, VkPhysicalDevice physicalDevice, VkRenderPass renderPass) {
    physicalDevice_ = physicalDevice;
    device_ = device;

    // Host-visible, persistently-mapped vertex buffer: the batch is rebuilt
    // and re-uploaded every frame (immediate-mode UI), so there's no benefit
    // to a device-local staging path for this data volume.
    VkBufferCreateInfo bci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bci.size = kMaxVerts * sizeof(Vertex);
    bci.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vkCreateBuffer(device, &bci, nullptr, &vertexBuffer_);

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(device, vertexBuffer_, &memReq);
    VkMemoryAllocateInfo mai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    mai.allocationSize = memReq.size;
    mai.memoryTypeIndex = findMemoryType(physicalDevice_, memReq.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vkAllocateMemory(device, &mai, nullptr, &vertexMemory_);
    vkBindBufferMemory(device, vertexBuffer_, vertexMemory_, 0);
    vkMapMemory(device, vertexMemory_, 0, bci.size, 0, &mapped_);

    // Resolve shader paths relative to the exe's own directory rather than
    // the process's current working directory, so this works regardless of
    // where the app was launched from.
    std::string exeDir = exeDirectory();
    std::string vertPath = exeDir + "\\shaders\\shape.vert.spv";
    std::string fragPath = exeDir + "\\shaders\\shape.frag.spv";
    VkShaderModule vert = loadShaderModule(device, vertPath.c_str());
    VkShaderModule frag = loadShaderModule(device, fragPath.c_str());

    VkPipelineShaderStageCreateInfo stages[2] = {};
    stages[0] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert;
    stages[0].pName = "main";
    stages[1] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag;
    stages[1].pName = "main";

    VkVertexInputBindingDescription binding{0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription attrs[7] = {
        {0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, x)},
        {1, 0, VK_FORMAT_R32_SFLOAT,    offsetof(Vertex, gray)},
        {2, 0, VK_FORMAT_R32_SFLOAT,    offsetof(Vertex, alpha)},
        {3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, ax)},
        {4, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, bx)},
        {5, 0, VK_FORMAT_R32_SFLOAT,    offsetof(Vertex, radius)},
        {6, 0, VK_FORMAT_R32_SFLOAT,    offsetof(Vertex, kind)},
    };
    VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vi.vertexBindingDescriptionCount = 1;
    vi.pVertexBindingDescriptions = &binding;
    vi.vertexAttributeDescriptionCount = 7;
    vi.pVertexAttributeDescriptions = attrs;

    VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    vp.viewportCount = 1;
    vp.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState blend{};
    blend.blendEnable = VK_TRUE;
    blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend.colorBlendOp = VK_BLEND_OP_ADD;
    blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    blend.alphaBlendOp = VK_BLEND_OP_ADD;
    blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                           VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    cb.attachmentCount = 1;
    cb.pAttachments = &blend;

    VkDynamicState dynStates[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dyn{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dyn.dynamicStateCount = 2;
    dyn.pDynamicStates = dynStates;

    VkPushConstantRange pcRange{VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(float) * 2};
    VkPipelineLayoutCreateInfo plci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    plci.pushConstantRangeCount = 1;
    plci.pPushConstantRanges = &pcRange;
    vkCreatePipelineLayout(device, &plci, nullptr, &layout_);

    VkGraphicsPipelineCreateInfo gpci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    gpci.stageCount = 2;
    gpci.pStages = stages;
    gpci.pVertexInputState = &vi;
    gpci.pInputAssemblyState = &ia;
    gpci.pViewportState = &vp;
    gpci.pRasterizationState = &rs;
    gpci.pMultisampleState = &ms;
    gpci.pColorBlendState = &cb;
    gpci.pDynamicState = &dyn;
    gpci.layout = layout_;
    gpci.renderPass = renderPass;
    gpci.subpass = 0;
    vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &gpci, nullptr, &pipeline_);

    vkDestroyShaderModule(device, vert, nullptr);
    vkDestroyShaderModule(device, frag, nullptr);
}

void ShapePipeline::draw(VkCommandBuffer cmd, VkExtent2D extent, const PrimitiveBatch& batch) {
    const auto& verts = batch.vertices();
    if (verts.empty()) return;
    if (verts.size() > kMaxVerts) {
        char msg[128];
        snprintf(msg, sizeof(msg),
            "ShapePipeline::draw: batch of %zu vertices exceeds kMaxVerts (%u)",
            verts.size(), (unsigned)kMaxVerts);
        fatal(msg);
    }
    size_t bytes = verts.size() * sizeof(Vertex);
    std::memcpy(mapped_, verts.data(), bytes);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

    VkViewport viewport{0, 0, (float)extent.width, (float)extent.height, 0.0f, 1.0f};
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    float screenSize[2] = { (float)extent.width, (float)extent.height };
    vkCmdPushConstants(cmd, layout_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(screenSize), screenSize);

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer_, &offset);

    // One draw per clip range: the scissor is dynamic state, so scrollable
    // page content clips at its container edge while unclipped ranges (nav,
    // other pages) cover the whole framebuffer. Clip rects are intersected
    // with the framebuffer so a partially off-screen container can't produce
    // an invalid scissor.
    for (const DrawRange& r : batch.ranges()) {
        if (r.vertCount == 0) continue;
        VkRect2D scissor{{0, 0}, extent};
        if (r.hasClip) {
            int32_t x0 = std::max(0, (int32_t)r.clipX);
            int32_t y0 = std::max(0, (int32_t)r.clipY);
            int32_t x1 = std::min((int32_t)extent.width, (int32_t)(r.clipX + r.clipW + 0.5f));
            int32_t y1 = std::min((int32_t)extent.height, (int32_t)(r.clipY + r.clipH + 0.5f));
            if (x1 <= x0 || y1 <= y0) continue;
            scissor = {{x0, y0}, {(uint32_t)(x1 - x0), (uint32_t)(y1 - y0)}};
        }
        vkCmdSetScissor(cmd, 0, 1, &scissor);
        vkCmdDraw(cmd, (uint32_t)r.vertCount, 1, (uint32_t)r.firstVert, 0);
    }
}

void ShapePipeline::cleanup(VkDevice device) {
    vkDestroyPipeline(device, pipeline_, nullptr);
    vkDestroyPipelineLayout(device, layout_, nullptr);
    vkUnmapMemory(device, vertexMemory_);
    vkFreeMemory(device, vertexMemory_, nullptr);
    vkDestroyBuffer(device, vertexBuffer_, nullptr);
}
