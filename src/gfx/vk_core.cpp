#include "vk_core.h"
#include <vulkan/vulkan_win32.h>
#include <cstdio>
#include <stdexcept>

static void fatal(const char* msg) {
    MessageBoxA(nullptr, msg, "windows_ui_demo — fatal Vulkan error", MB_OK | MB_ICONERROR);
    ExitProcess(1);
}

void VkCore::createInstance() {
    VkApplicationInfo appInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    appInfo.pApplicationName = "windows_ui_demo";
    appInfo.apiVersion = VK_API_VERSION_1_3;

    const char* extensions[] = { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME };

    VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ci.pApplicationInfo = &appInfo;
    ci.enabledExtensionCount = 2;
    ci.ppEnabledExtensionNames = extensions;
#ifdef _DEBUG
    const char* layers[] = { "VK_LAYER_KHRONOS_validation" };
    ci.enabledLayerCount = 1;
    ci.ppEnabledLayerNames = layers;
#endif
    if (vkCreateInstance(&ci, nullptr, &instance_) != VK_SUCCESS) fatal("vkCreateInstance failed");
}

void VkCore::createSurface(HINSTANCE hInst, HWND hwnd) {
    VkWin32SurfaceCreateInfoKHR ci{VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
    ci.hinstance = hInst;
    ci.hwnd = hwnd;
    if (vkCreateWin32SurfaceKHR(instance_, &ci, nullptr, &surface_) != VK_SUCCESS)
        fatal("vkCreateWin32SurfaceKHR failed");
}

void VkCore::pickPhysicalDevice() {
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(instance_, &count, nullptr);
    if (count == 0) fatal("No Vulkan-capable GPU found");
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(instance_, &count, devices.data());

    for (VkPhysicalDevice dev : devices) {
        uint32_t qCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &qCount, nullptr);
        std::vector<VkQueueFamilyProperties> qProps(qCount);
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &qCount, qProps.data());
        for (uint32_t i = 0; i < qCount; i++) {
            VkBool32 presentSupport = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface_, &presentSupport);
            if ((qProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && presentSupport) {
                physicalDevice_ = dev;
                graphicsQueueFamily_ = i;
                return;
            }
        }
    }
    fatal("No GPU with combined graphics+present queue found");
}

void VkCore::createDevice() {
    float priority = 1.0f;
    VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qci.queueFamilyIndex = graphicsQueueFamily_;
    qci.queueCount = 1;
    qci.pQueuePriorities = &priority;

    const char* deviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    VkDeviceCreateInfo ci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    ci.queueCreateInfoCount = 1;
    ci.pQueueCreateInfos = &qci;
    ci.enabledExtensionCount = 1;
    ci.ppEnabledExtensionNames = deviceExtensions;

    if (vkCreateDevice(physicalDevice_, &ci, nullptr, &device_) != VK_SUCCESS)
        fatal("vkCreateDevice failed");
    vkGetDeviceQueue(device_, graphicsQueueFamily_, 0, &graphicsQueue_);
}

void VkCore::createSwapchain(int width, int height) {
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice_, surface_, &caps);

    swapchainExtent_.width = (caps.currentExtent.width != 0xFFFFFFFF)
        ? caps.currentExtent.width : (uint32_t)width;
    swapchainExtent_.height = (caps.currentExtent.width != 0xFFFFFFFF)
        ? caps.currentExtent.height : (uint32_t)height;
    if (swapchainExtent_.width == 0) swapchainExtent_.width = 1;
    if (swapchainExtent_.height == 0) swapchainExtent_.height = 1;

    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) imageCount = caps.maxImageCount;

    VkSwapchainCreateInfoKHR ci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    ci.surface = surface_;
    ci.minImageCount = imageCount;
    ci.imageFormat = swapchainFormat_;
    ci.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    ci.imageExtent = swapchainExtent_;
    ci.imageArrayLayers = 1;
    ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.preTransform = caps.currentTransform;
    ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode = VK_PRESENT_MODE_FIFO_KHR; // always supported; vsynced
    ci.clipped = VK_TRUE;

    if (vkCreateSwapchainKHR(device_, &ci, nullptr, &swapchain_) != VK_SUCCESS)
        fatal("vkCreateSwapchainKHR failed");

    uint32_t actualCount = 0;
    vkGetSwapchainImagesKHR(device_, swapchain_, &actualCount, nullptr);
    swapchainImages_.resize(actualCount);
    vkGetSwapchainImagesKHR(device_, swapchain_, &actualCount, swapchainImages_.data());

    swapchainViews_.resize(actualCount);
    for (uint32_t i = 0; i < actualCount; i++) {
        VkImageViewCreateInfo vci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        vci.image = swapchainImages_[i];
        vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vci.format = swapchainFormat_;
        vci.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        if (vkCreateImageView(device_, &vci, nullptr, &swapchainViews_[i]) != VK_SUCCESS)
            fatal("vkCreateImageView failed");
    }
}

void VkCore::createRenderPass() {
    VkAttachmentDescription color{};
    color.format = swapchainFormat_;
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;

    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo ci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    ci.attachmentCount = 1;
    ci.pAttachments = &color;
    ci.subpassCount = 1;
    ci.pSubpasses = &subpass;
    ci.dependencyCount = 1;
    ci.pDependencies = &dep;

    if (vkCreateRenderPass(device_, &ci, nullptr, &renderPass_) != VK_SUCCESS)
        fatal("vkCreateRenderPass failed");
}

void VkCore::createFramebuffers() {
    framebuffers_.resize(swapchainViews_.size());
    for (size_t i = 0; i < swapchainViews_.size(); i++) {
        VkFramebufferCreateInfo ci{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        ci.renderPass = renderPass_;
        ci.attachmentCount = 1;
        ci.pAttachments = &swapchainViews_[i];
        ci.width = swapchainExtent_.width;
        ci.height = swapchainExtent_.height;
        ci.layers = 1;
        if (vkCreateFramebuffer(device_, &ci, nullptr, &framebuffers_[i]) != VK_SUCCESS)
            fatal("vkCreateFramebuffer failed");
    }
}

void VkCore::createCommandObjects() {
    VkCommandPoolCreateInfo pci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pci.queueFamilyIndex = graphicsQueueFamily_;
    if (vkCreateCommandPool(device_, &pci, nullptr, &commandPool_) != VK_SUCCESS)
        fatal("vkCreateCommandPool failed");

    commandBuffers_.resize(kFramesInFlight);
    VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ai.commandPool = commandPool_;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = kFramesInFlight;
    if (vkAllocateCommandBuffers(device_, &ai, commandBuffers_.data()) != VK_SUCCESS)
        fatal("vkAllocateCommandBuffers failed");
}

void VkCore::createSyncObjects() {
    // Fences track frames-in-flight and live for the lifetime of the device (not
    // recreated on swapchain recreation).
    inFlightFences_.resize(kFramesInFlight);
    VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (int i = 0; i < kFramesInFlight; i++) {
        if (vkCreateFence(device_, &fci, nullptr, &inFlightFences_[i]) != VK_SUCCESS)
            fatal("vkCreateFence failed");
    }

    // Semaphores track swapchain images (see field comments in vk_core.h) and must be
    // created after the swapchain exists; also (re)created whenever the swapchain is.
    createImageSemaphores();
}

void VkCore::createImageSemaphores() {
    size_t count = swapchainImages_.size();
    imageAvailable_.resize(count);
    renderFinished_.resize(count);
    VkSemaphoreCreateInfo sci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    for (size_t i = 0; i < count; i++) {
        if (vkCreateSemaphore(device_, &sci, nullptr, &imageAvailable_[i]) != VK_SUCCESS)
            fatal("vkCreateSemaphore failed");
        if (vkCreateSemaphore(device_, &sci, nullptr, &renderFinished_[i]) != VK_SUCCESS)
            fatal("vkCreateSemaphore failed");
    }
}

void VkCore::destroyImageSemaphores() {
    for (auto s : imageAvailable_) vkDestroySemaphore(device_, s, nullptr);
    imageAvailable_.clear();
    for (auto s : renderFinished_) vkDestroySemaphore(device_, s, nullptr);
    renderFinished_.clear();
}

bool VkCore::init(HINSTANCE hInst, HWND hwnd, int width, int height) {
    hwnd_ = hwnd;
    createInstance();
    createSurface(hInst, hwnd);
    pickPhysicalDevice();
    createDevice();
    createSwapchain(width, height);
    createRenderPass();
    createFramebuffers();
    createCommandObjects();
    createSyncObjects();
    return true;
}

void VkCore::notifyResize(int width, int height) {
    pendingWidth_ = width;
    pendingHeight_ = height;
    resizePending_ = true;
}

void VkCore::destroySwapchain() {
    // Per-image semaphores are tied 1:1 to swapchain images; they must go whenever the
    // swapchain does, and get recreated (possibly at a different count) alongside it.
    destroyImageSemaphores();
    for (auto fb : framebuffers_) vkDestroyFramebuffer(device_, fb, nullptr);
    framebuffers_.clear();
    for (auto v : swapchainViews_) vkDestroyImageView(device_, v, nullptr);
    swapchainViews_.clear();
    if (swapchain_) vkDestroySwapchainKHR(device_, swapchain_, nullptr);
    swapchain_ = VK_NULL_HANDLE;
}

void VkCore::recreateSwapchain() {
    vkDeviceWaitIdle(device_);
    destroySwapchain();
    if (pendingWidth_ == 0 || pendingHeight_ == 0) return; // minimized
    createSwapchain(pendingWidth_, pendingHeight_);
    createFramebuffers();
    createImageSemaphores();
    // The per-image semaphore pool may have been reallocated at a different size; make
    // sure the rotating index used to pick from it is back in range.
    semaphoreIndex_ = 0;
}

bool VkCore::beginFrame(FrameContext& outCtx) {
    if (resizePending_) {
        resizePending_ = false;
        recreateSwapchain();
        if (swapchain_ == VK_NULL_HANDLE) return false; // still minimized
    }

    vkWaitForFences(device_, 1, &inFlightFences_[currentFrame_], VK_TRUE, UINT64_MAX);

    // The acquired image index isn't known until vkAcquireNextImageKHR returns, so the
    // semaphore passed to it can't be indexed by image. Use a rotating counter over the
    // image-sized pool instead; it's safe because the fence wait above already throttles
    // reuse to at most kFramesInFlight submissions ahead.
    VkResult acquireResult = vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX,
        imageAvailable_[semaphoreIndex_], VK_NULL_HANDLE, &currentImageIndex_);
    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
        notifyResize(swapchainExtent_.width, swapchainExtent_.height);
        return false;
    }

    vkResetFences(device_, 1, &inFlightFences_[currentFrame_]);

    VkCommandBuffer cmd = commandBuffers_[currentFrame_];
    vkResetCommandBuffer(cmd, 0);
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    vkBeginCommandBuffer(cmd, &bi);

    VkClearValue clear{};
    clear.color = { {0.5f, 0.5f, 0.5f, 1.0f} }; // mid-gray: proves the pipeline is grayscale-only
    VkRenderPassBeginInfo rpbi{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rpbi.renderPass = renderPass_;
    rpbi.framebuffer = framebuffers_[currentImageIndex_];
    rpbi.renderArea = { {0, 0}, swapchainExtent_ };
    rpbi.clearValueCount = 1;
    rpbi.pClearValues = &clear;
    vkCmdBeginRenderPass(cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);

    outCtx.cmd = cmd;
    outCtx.renderPass = renderPass_;
    outCtx.extent = swapchainExtent_;
    return true;
}

void VkCore::endFrame() {
    VkCommandBuffer cmd = commandBuffers_[currentFrame_];
    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);

    // renderFinished_ is indexed by the acquired image (currentImageIndex_), not the
    // frame slot: it's what vkQueuePresentKHR below waits on for this specific image, and
    // the swapchain can have more images than frames-in-flight (see vk_core.h comment).
    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.waitSemaphoreCount = 1;
    si.pWaitSemaphores = &imageAvailable_[semaphoreIndex_];
    si.pWaitDstStageMask = &waitStage;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores = &renderFinished_[currentImageIndex_];
    vkQueueSubmit(graphicsQueue_, 1, &si, inFlightFences_[currentFrame_]);

    VkPresentInfoKHR pi{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores = &renderFinished_[currentImageIndex_];
    pi.swapchainCount = 1;
    pi.pSwapchains = &swapchain_;
    pi.pImageIndices = &currentImageIndex_;
    VkResult presentResult = vkQueuePresentKHR(graphicsQueue_, &pi);
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
        notifyResize(swapchainExtent_.width, swapchainExtent_.height);
    }

    currentFrame_ = (currentFrame_ + 1) % kFramesInFlight;
    semaphoreIndex_ = (semaphoreIndex_ + 1) % imageAvailable_.size();
}

void VkCore::cleanup() {
    vkDeviceWaitIdle(device_);
    for (int i = 0; i < kFramesInFlight; i++) {
        vkDestroyFence(device_, inFlightFences_[i], nullptr);
    }
    vkDestroyCommandPool(device_, commandPool_, nullptr);
    destroySwapchain(); // also destroys the per-image imageAvailable_/renderFinished_ semaphores
    vkDestroyRenderPass(device_, renderPass_, nullptr);
    vkDestroyDevice(device_, nullptr);
    vkDestroySurfaceKHR(instance_, surface_, nullptr);
    vkDestroyInstance(instance_, nullptr);
}
