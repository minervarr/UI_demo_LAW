#include "vk_surface_windows.h"
#include "../fatal.h"

VkSurfaceKHR createWin32Surface(VkInstance instance, HINSTANCE hInst, HWND hwnd) {
    VkWin32SurfaceCreateInfoKHR ci{VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
    ci.hinstance = hInst;
    ci.hwnd = hwnd;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (vkCreateWin32SurfaceKHR(instance, &ci, nullptr, &surface) != VK_SUCCESS)
        fatal("vkCreateWin32SurfaceKHR failed");
    return surface;
}
