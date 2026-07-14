#pragma once
#include <vulkan/vulkan.h>
#include <vector>

struct FrameContext {
    VkCommandBuffer cmd;
    VkRenderPass renderPass;
    VkExtent2D extent;
};

// Platform-agnostic Vulkan bootstrap: instance/device/swapchain/render pass/
// sync objects. Owns no native window type — the platform layer creates the
// VkSurfaceKHR (see e.g. platform/windows/vk_surface_windows.h) and hands it
// to init(), which takes ownership (destroyed in cleanup()).
class VkCore {
 public:
    // Creates the VkInstance, always enabling VK_KHR_surface plus whatever
    // platform-specific surface extension(s) the caller needs (e.g.
    // VK_KHR_WIN32_SURFACE_EXTENSION_NAME on Windows). Call before creating
    // the platform surface, since surface creation needs a live VkInstance.
    void createInstance(const std::vector<const char*>& surfaceExtensions);
    VkInstance instance() const { return instance_; }
    // Continues bootstrap from an already-created surface (device selection,
    // swapchain, render pass, sync objects). VkCore takes ownership of
    // `surface` from this point on.
    bool init(VkSurfaceKHR surface, int width, int height);
    void notifyResize(int width, int height);
    bool beginFrame(FrameContext& outCtx);
    void endFrame();
    VkDevice device() const { return device_; }
    VkPhysicalDevice physicalDevice() const { return physicalDevice_; }
    void cleanup();

 private:
    static constexpr int kFramesInFlight = 2;
    void pickPhysicalDevice();
    void createDevice();
    void createSwapchain(int width, int height);
    void createRenderPass();
    void createFramebuffers();
    void createCommandObjects();
    void createSyncObjects();
    void createImageSemaphores();
    void destroyImageSemaphores();
    void destroySwapchain();
    void recreateSwapchain();

    VkInstance instance_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    uint32_t graphicsQueueFamily_ = 0;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;

    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkFormat swapchainFormat_ = VK_FORMAT_B8G8R8A8_UNORM;
    VkExtent2D swapchainExtent_ = {};
    std::vector<VkImage> swapchainImages_;
    std::vector<VkImageView> swapchainViews_;
    std::vector<VkFramebuffer> framebuffers_;
    VkRenderPass renderPass_ = VK_NULL_HANDLE;

    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers_;
    // Sized to swapchainImages_.size() (not kFramesInFlight): the swapchain can have more
    // images than frames-in-flight, and vkAcquireNextImageKHR does not return images in
    // strict round-robin order, so frame-indexed semaphores can be re-signaled before a
    // prior wait on them completes. renderFinished_ is indexed by currentImageIndex_ (the
    // image it's actually associated with at submit/present time). imageAvailable_ is
    // indexed by semaphoreIndex_, a simple rotating counter over the same-sized pool,
    // since the acquired image index isn't known until after the semaphore is submitted
    // to vkAcquireNextImageKHR.
    std::vector<VkSemaphore> imageAvailable_;
    std::vector<VkSemaphore> renderFinished_;
    std::vector<VkFence> inFlightFences_;
    uint32_t currentFrame_ = 0;
    uint32_t currentImageIndex_ = 0;
    uint32_t semaphoreIndex_ = 0;

    int pendingWidth_ = 0, pendingHeight_ = 0;
    bool resizePending_ = false;
};
