#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>

// Creates a Win32 VkSurfaceKHR for `hwnd` against an already-created
// `instance` (see VkCore::createInstance, which must have been called with
// VK_KHR_WIN32_SURFACE_EXTENSION_NAME first). Fatals on failure. Caller
// (main.cpp) hands the returned surface to VkCore::init(), which takes
// ownership of it.
VkSurfaceKHR createWin32Surface(VkInstance instance, HINSTANCE hInst, HWND hwnd);
