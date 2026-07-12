# Vulkan Grayscale UI Showcase Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build `windows_UI_demo`, a standalone Win32 + Vulkan app that showcases a brand-new minimal grayscale renderer (text, shapes/curves, interactive widgets, animation) with zero audio_engine content.

**Architecture:** A thin Vulkan bootstrap (`gfx/vk_core`) feeds a single CPU-side triangle batcher (`gfx/primitives`) that draws every shape/line/curve as grayscale triangles in one pipeline. Text goes through the vendored `vulkan_font_engine` submodule's MTSDF atlas renderer (`MsdfFont` + `MsdfTextRenderer`), which owns its own pipeline against the same render pass. An app-level `anim` module drives interpolation (the renderer itself has no animation primitive). Four pages (Text, Shapes & Curves, Widgets, Animation) sit behind a top nav in one window.

**Tech Stack:** C++17, Vulkan 1.3 (traditional VkRenderPass/VkFramebuffer — required because `MsdfTextRenderer::createResources` takes a `VkRenderPass`), Win32, CMake + Ninja + MSVC (matches `windows_matrix_player`), GLSL compiled with `glslc` for the new primitive pipeline, Slang compiled with `slangc` for the two pre-existing MSDF shaders, `vulkan_font_engine` git submodule.

## Global Constraints

- Zero audio_engine content: no audio_engine headers, code, or concepts anywhere in this repo.
- Only submodule: `libs/firstparty/vulkan_font_engine` → `https://github.com/minervarr/vulkan_font_engine`. No vk_canvas, no audio_engine, no libusb.
- Grayscale-by-construction: every fragment shader writes only `R == G == B`, no chroma, anywhere.
- All offscreen/procedural textures use `R8_UNORM`, except the MTSDF font atlas (inherently multi-channel — its sampled *output* is still grayscale-only by the same shader discipline).
- No true 1-bit/dithering — many shades of gray are allowed.
- Build: CMake + Ninja + MSVC via `build.bat` (vcvars64 → cmake configure → ninja build), same pattern as `windows_matrix_player`. No .sln files.
- Error handling: fail-fast (message box + exit) on Vulkan init failure. No retry/fallback logic anywhere. Validation layers on in Debug only.
- No automated test suite for rendering itself (verified by running the app) — but pure-logic pieces (batcher vertex math, animation easing, widget hit-testing) get real unit tests, since they need no GPU to verify.
- Commit/push exclusively via `git_wrapper.exe` (`git_wrapper commit "..."` / `git_wrapper push`), never plain `git commit`/`git push`. It's a global tool already on this machine's PATH.
- Font: bundle only `fonts/lm/lmroman10-regular.otf` and `fonts/lm/lmroman10-bold.otf` (Latin Modern, GUST Font License, already vendored the same way in the sibling `windows_matrix_player` repo) — copy them from `C:\Users\incxiuefb\Documents\Files\clone\windows_matrix_player\fonts\lm\`.
- Layout: no hardcoded absolute pixel positions in the four pages by the end of the plan. Use the proportional-scale + edge-dock + gap-chain technique `windows_matrix_player` already validated across monitor resolutions (`libs/firstparty/vk_canvas/core/responsive_text.hh`'s `ResponsiveTextScale` + `player_window.cpp`'s `recalcLayout()`) — reimplemented standalone in `ui/layout.h` (Task 11), since this repo doesn't depend on vk_canvas.

---

### Task 1: Repo scaffolding, submodule, and build script

**Files:**
- Create: `CMakeLists.txt`
- Create: `build.bat`
- Create: `cmake/CompileShaders.cmake`
- Create: `src/app/main.cpp` (placeholder — replaced in Task 9/10)
- Create: `.gitmodules` (via `git submodule add`)
- Create: `fonts/lm/lmroman10-regular.otf`, `fonts/lm/lmroman10-bold.otf` (copied)
- Create: `.gitignore`

**Interfaces:**
- Produces: a buildable "hello window" exe (`windows_ui_demo.exe`) that later tasks extend in place. `cmake/CompileShaders.cmake` exposes two functions later tasks call: `compile_glsl_shaders(<target> <out_dir> <src_dir> <name>...)` and `compile_slang_shaders(<target> <out_dir> <src_dir> <name>...)`.

- [ ] **Step 1: Add the vulkan_font_engine submodule**

```bash
cd "C:\Users\incxiuefb\Documents\Files\clone\windows_UI_demo"
git submodule add https://github.com/minervarr/vulkan_font_engine.git libs/firstparty/vulkan_font_engine
git submodule update --init --recursive
```

Expected: `.gitmodules` created with one entry; `libs/firstparty/vulkan_font_engine/third_party/freetype` and `.../third_party/msdfgen` are populated (nested submodules pulled recursively).

- [ ] **Step 2: Copy the two bundled font files**

```bash
mkdir -p "C:\Users\incxiuefb\Documents\Files\clone\windows_UI_demo\fonts\lm"
cp "C:\Users\incxiuefb\Documents\Files\clone\windows_matrix_player\fonts\lm\lmroman10-regular.otf" "C:\Users\incxiuefb\Documents\Files\clone\windows_UI_demo\fonts\lm\"
cp "C:\Users\incxiuefb\Documents\Files\clone\windows_matrix_player\fonts\lm\lmroman10-bold.otf" "C:\Users\incxiuefb\Documents\Files\clone\windows_UI_demo\fonts\lm\"
```

Expected: two `.otf` files present under `fonts/lm/`.

- [ ] **Step 3: Write `.gitignore`**

```
build/
build_debug/
.vs/
*.msdf.cache
```

- [ ] **Step 4: Write `cmake/CompileShaders.cmake`**

```cmake
# Compiles GLSL shaders with glslc (new grayscale primitive pipeline) into SPIR-V.
# Usage: compile_glsl_shaders(<target_name> <out_dir> <src_dir> <name> [<name> ...])
# For each <name>, expects <src_dir>/<name> to exist (e.g. "shape.vert") and
# produces <out_dir>/<name>.spv.
function(compile_glsl_shaders TARGET_NAME OUT_DIR SRC_DIR)
    find_program(GLSLC_EXE glslc HINTS "$ENV{VULKAN_SDK}/Bin" "C:/VulkanSDK/1.4.341.1/Bin")
    if(NOT GLSLC_EXE)
        message(FATAL_ERROR "glslc not found — install the Vulkan SDK or set VULKAN_SDK")
    endif()
    file(MAKE_DIRECTORY ${OUT_DIR})
    set(_outputs "")
    foreach(_name ${ARGN})
        set(_src "${SRC_DIR}/${_name}")
        set(_out "${OUT_DIR}/${_name}.spv")
        add_custom_command(
            OUTPUT ${_out}
            COMMAND ${GLSLC_EXE} ${_src} -o ${_out}
            DEPENDS ${_src}
            COMMENT "Compiling GLSL shader ${_name}")
        list(APPEND _outputs ${_out})
    endforeach()
    add_custom_target(${TARGET_NAME} ALL DEPENDS ${_outputs})
endfunction()

# Compiles Slang shaders with slangc (vulkan_font_engine's pre-existing MSDF
# shaders) into SPIR-V. Usage identical to compile_glsl_shaders, but <name>
# has no extension (e.g. "msdf_vert") and the source is "<name>.slang".
function(compile_slang_shaders TARGET_NAME OUT_DIR SRC_DIR)
    find_program(SLANGC_EXE slangc HINTS "$ENV{VULKAN_SDK}/Bin" "C:/VulkanSDK/1.4.341.1/Bin")
    if(NOT SLANGC_EXE)
        message(FATAL_ERROR "slangc not found — install the Vulkan SDK or set VULKAN_SDK")
    endif()
    file(MAKE_DIRECTORY ${OUT_DIR})
    set(_outputs "")
    foreach(_name ${ARGN})
        set(_src "${SRC_DIR}/${_name}.slang")
        set(_out "${OUT_DIR}/${_name}.spv")
        add_custom_command(
            OUTPUT ${_out}
            COMMAND ${SLANGC_EXE} ${_src} -target spirv -o ${_out}
            DEPENDS ${_src}
            COMMENT "Compiling Slang shader ${_name}")
        list(APPEND _outputs ${_out})
    endforeach()
    add_custom_target(${TARGET_NAME} ALL DEPENDS ${_outputs})
endfunction()
```

- [ ] **Step 5: Write the top-level `CMakeLists.txt`**

```cmake
cmake_minimum_required(VERSION 3.22.1)
project("windows_ui_demo")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

include(cmake/CompileShaders.cmake)

# ---------- vulkan_font_engine's platform-agnostic core (MTSDF text) ----------
add_subdirectory(libs/firstparty/vulkan_font_engine/core vk_font_core_build)

# ---------- app executable ----------
add_executable(windows_ui_demo WIN32
    src/app/main.cpp
)
target_link_libraries(windows_ui_demo PRIVATE vk_font_core)
target_compile_definitions(windows_ui_demo PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)

# Copy bundled fonts next to the executable
add_custom_command(TARGET windows_ui_demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/fonts
        $<TARGET_FILE_DIR:windows_ui_demo>/fonts)
```

(Later tasks append more `target_sources`/`add_dependencies` calls to this file — each task's steps say exactly what to add.)

- [ ] **Step 6: Write a placeholder `src/app/main.cpp`**

```cpp
#include <windows.h>

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    MessageBoxW(nullptr, L"Scaffolding OK", L"windows_ui_demo", MB_OK);
    return 0;
}
```

- [ ] **Step 7: Write `build.bat`**

```bat
@echo off
setlocal enabledelayedexpansion

set "TARGET_EXE=windows_ui_demo.exe"
set "BUILD_TYPE="
set "CLEAN_REQUESTED="

:ParseArgs
if "%~1"=="" goto :DoneParsing
if "%~1"=="clean" ( set "CLEAN_REQUESTED=1" & shift & goto :ParseArgs )
if "%~1"=="debug" ( set "BUILD_TYPE=Debug" & shift & goto :ParseArgs )
if "%~1"=="release" ( set "BUILD_TYPE=Release" & shift & goto :ParseArgs )
echo Unknown argument: %1
exit /b 1
:DoneParsing

if not defined BUILD_TYPE (
    echo   [1] Release  - optimized ^(default^)
    echo   [2] Debug    - unoptimized, full debug symbols
    set "BUILD_CHOICE="
    set /p "BUILD_CHOICE=Enter 1 or 2 (Enter = Release): "
    if "!BUILD_CHOICE!"=="2" ( set "BUILD_TYPE=Debug" ) else ( set "BUILD_TYPE=Release" )
)
echo [Config] Build type: %BUILD_TYPE%

if /i "%BUILD_TYPE%"=="Debug" ( set "BUILD_DIR=build_debug" ) else ( set "BUILD_DIR=build" )

if defined CLEAN_REQUESTED (
    if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
)

for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    set "VS_PATH=%%i"
)
if not defined VS_PATH (
    echo ERROR: Visual Studio Build Tools not found.
    exit /b 1
)
call "%VS_PATH%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 ( echo ERROR: Failed to initialize MSVC environment. & exit /b 1 )

where git >nul 2>&1
if not errorlevel 1 (
    git submodule update --init --recursive
    if errorlevel 1 ( echo ERROR: submodule update failed. & exit /b 1 )
)

cmake -G Ninja -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE=%BUILD_TYPE%
if errorlevel 1 ( echo ERROR: CMake configuration failed. & exit /b 1 )

cmake --build "%BUILD_DIR%" --parallel
if errorlevel 1 ( echo ERROR: Build failed. & exit /b 1 )

echo Success! Output: %BUILD_DIR%\%TARGET_EXE%
endlocal
exit /b 0
```

- [ ] **Step 8: Build and verify**

```bash
cd "C:\Users\incxiuefb\Documents\Files\clone\windows_UI_demo"
./build.bat debug
```

Expected: `build_debug\windows_ui_demo.exe` produced; running it shows a "Scaffolding OK" message box.

- [ ] **Step 9: Commit**

```bash
cd "C:\Users\incxiuefb\Documents\Files\clone\windows_UI_demo"
git_wrapper commit "Scaffold windows_ui_demo: CMake+Ninja+MSVC build, vulkan_font_engine submodule, bundled fonts"
```

---

### Task 2: Win32 window shell + input state

**Files:**
- Create: `src/platform/window.h`, `src/platform/window.cpp`
- Create: `src/platform/input_state.h`, `src/platform/input_state.cpp`
- Test: `src/platform/input_state_test.cpp`
- Modify: `CMakeLists.txt` (add sources, add a test executable target)

**Interfaces:**
- Produces:
  - `struct InputState { float mouseX, mouseY; bool mouseDown, mouseWentDown, mouseWentUp; };`
  - `void InputState::beginFrame();` — clears the per-frame edge flags (call once at the top of each frame, before processing Win32 messages)
  - `class Window { bool create(HINSTANCE hInst, int width, int height, const wchar_t* title); bool pumpMessages(); /* returns false on WM_QUIT */ HWND hwnd() const; const InputState& input() const; bool consumeResized(int& outW, int& outH); };`

- [ ] **Step 1: Write the failing test for edge detection**

```cpp
// src/platform/input_state_test.cpp
#include "input_state.h"
#include <cassert>
#include <cstdio>

int main() {
    InputState s{};

    // Frame 1: mouse goes down
    s.beginFrame();
    s.mouseDown = true;
    s.mouseWentDown = true;
    assert(s.mouseWentDown == true);

    // Frame 2: mouse stays down — went-down edge must clear
    s.beginFrame();
    assert(s.mouseWentDown == false);
    assert(s.mouseDown == true);

    // Frame 3: mouse released
    s.mouseDown = false;
    s.mouseWentUp = true;
    assert(s.mouseWentUp == true);

    // Frame 4: went-up edge must clear even though mouseDown stays false
    s.beginFrame();
    assert(s.mouseWentUp == false);
    assert(s.mouseDown == false);

    printf("input_state_test: OK\n");
    return 0;
}
```

- [ ] **Step 2: Add the test target to CMakeLists.txt and run it to verify it fails**

Add to `CMakeLists.txt`:

```cmake
add_executable(input_state_test src/platform/input_state.cpp src/platform/input_state_test.cpp)
```

```bash
cmake -G Ninja -B build_debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build_debug --target input_state_test
```

Expected: FAIL — `input_state.h`/`input_state.cpp` don't exist yet.

- [ ] **Step 3: Write `src/platform/input_state.h`**

```cpp
#pragma once

struct InputState {
    float mouseX = 0.0f;
    float mouseY = 0.0f;
    bool mouseDown = false;      // held state, persists across frames
    bool mouseWentDown = false;  // true only on the frame the button was pressed
    bool mouseWentUp = false;    // true only on the frame the button was released

    // Clears the per-frame edge flags. Call once per frame before pumping
    // Win32 messages (WM_LBUTTONDOWN/UP set the edge flags for that frame).
    void beginFrame();
};
```

- [ ] **Step 4: Write `src/platform/input_state.cpp`**

```cpp
#include "input_state.h"

void InputState::beginFrame() {
    mouseWentDown = false;
    mouseWentUp = false;
}
```

- [ ] **Step 5: Run test to verify it passes**

```bash
cmake --build build_debug --target input_state_test
./build_debug/input_state_test.exe
```

Expected: prints `input_state_test: OK`, exit code 0.

- [ ] **Step 6: Write `src/platform/window.h`**

```cpp
#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "input_state.h"

class Window {
 public:
    bool create(HINSTANCE hInst, int width, int height, const wchar_t* title);
    // Pumps all pending Win32 messages. Returns false once WM_QUIT arrives.
    bool pumpMessages();
    HWND hwnd() const { return hwnd_; }
    const InputState& input() const { return input_; }
    // Returns true (and fills outW/outH) exactly once per resize, then clears
    // the pending flag — callers use this to know when to recreate a swapchain.
    bool consumeResized(int& outW, int& outH);

 private:
    static LRESULT CALLBACK wndProc(HWND, UINT, WPARAM, LPARAM);
    HWND hwnd_ = nullptr;
    InputState input_;
    bool resizedPending_ = false;
    int pendingW_ = 0, pendingH_ = 0;
};
```

- [ ] **Step 7: Write `src/platform/window.cpp`**

```cpp
#include "window.h"

static const wchar_t* kClassName = L"WindowsUIDemoWindow";

bool Window::create(HINSTANCE hInst, int width, int height, const wchar_t* title) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = wndProc;
    wc.hInstance = hInst;
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = kClassName;
    RegisterClassExW(&wc);

    hwnd_ = CreateWindowExW(0, kClassName, title, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, width, height,
        nullptr, nullptr, hInst, this);
    if (!hwnd_) return false;

    SetWindowLongPtrW(hwnd_, GWLP_USERDATA, (LONG_PTR)this);
    ShowWindow(hwnd_, SW_SHOW);
    return true;
}

bool Window::pumpMessages() {
    input_.beginFrame();
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) return false;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return true;
}

bool Window::consumeResized(int& outW, int& outH) {
    if (!resizedPending_) return false;
    outW = pendingW_;
    outH = pendingH_;
    resizedPending_ = false;
    return true;
}

LRESULT CALLBACK Window::wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    Window* self = (Window*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    switch (msg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_SIZE:
            if (self) {
                self->resizedPending_ = true;
                self->pendingW_ = LOWORD(lParam);
                self->pendingH_ = HIWORD(lParam);
            }
            return 0;
        case WM_MOUSEMOVE:
            if (self) {
                self->input_.mouseX = (float)GET_X_LPARAM(lParam);
                self->input_.mouseY = (float)GET_Y_LPARAM(lParam);
            }
            return 0;
        case WM_LBUTTONDOWN:
            if (self) {
                if (!self->input_.mouseDown) self->input_.mouseWentDown = true;
                self->input_.mouseDown = true;
                SetCapture(hwnd);
            }
            return 0;
        case WM_LBUTTONUP:
            if (self) {
                if (self->input_.mouseDown) self->input_.mouseWentUp = true;
                self->input_.mouseDown = false;
                ReleaseCapture();
            }
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
```

Add `#include <windowsx.h>` is required for `GET_X_LPARAM`/`GET_Y_LPARAM` — add it to the top of `window.cpp`.

- [ ] **Step 8: Add sources to `CMakeLists.txt`**

```cmake
target_sources(windows_ui_demo PRIVATE
    src/platform/window.cpp
    src/platform/input_state.cpp
)
```

- [ ] **Step 9: Update `main.cpp` to open the window and pump messages**

```cpp
#include "platform/window.h"

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int) {
    Window window;
    if (!window.create(hInst, 1280, 800, L"windows_ui_demo")) return 1;
    while (window.pumpMessages()) {
        Sleep(1);
    }
    return 0;
}
```

- [ ] **Step 10: Build and manually verify**

```bash
cmake --build build_debug --target windows_ui_demo
./build_debug/windows_ui_demo.exe
```

Expected: a black, resizable window opens and closes cleanly via the close button. No crash on resize.

- [ ] **Step 11: Commit**

```bash
git_wrapper commit "Add Win32 window shell and edge-detected input state"
```

---

### Task 3: Vulkan bootstrap (gfx/vk_core) — clears to gray each frame

**Files:**
- Create: `src/gfx/vk_core.h`, `src/gfx/vk_core.cpp`
- Modify: `CMakeLists.txt` (link `vulkan-1.lib`, add sources)
- Modify: `src/app/main.cpp`

**Interfaces:**
- Consumes: `Window::hwnd()`, `HINSTANCE` from `wWinMain`.
- Produces:
  ```cpp
  struct FrameContext {
      VkCommandBuffer cmd;
      VkRenderPass renderPass;   // consumed by Task 4 and Task 6 to build pipelines
      VkExtent2D extent;
  };
  class VkCore {
   public:
      bool init(HINSTANCE hInst, HWND hwnd, int width, int height);
      void notifyResize(int width, int height);
      // Returns false (skip the frame) only in the rare case the swapchain is
      // still being recreated after a minimize; never fails silently otherwise.
      bool beginFrame(FrameContext& outCtx);
      void endFrame();
      VkDevice device() const { return device_; }
      VkPhysicalDevice physicalDevice() const { return physicalDevice_; }
      void cleanup();
  };
  ```

- [ ] **Step 1: Write `src/gfx/vk_core.h`**

```cpp
#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <vulkan/vulkan.h>
#include <vector>

struct FrameContext {
    VkCommandBuffer cmd;
    VkRenderPass renderPass;
    VkExtent2D extent;
};

class VkCore {
 public:
    bool init(HINSTANCE hInst, HWND hwnd, int width, int height);
    void notifyResize(int width, int height);
    bool beginFrame(FrameContext& outCtx);
    void endFrame();
    VkDevice device() const { return device_; }
    VkPhysicalDevice physicalDevice() const { return physicalDevice_; }
    void cleanup();

 private:
    static constexpr int kFramesInFlight = 2;
    void createInstance();
    void createSurface(HINSTANCE hInst, HWND hwnd);
    void pickPhysicalDevice();
    void createDevice();
    void createSwapchain(int width, int height);
    void createRenderPass();
    void createFramebuffers();
    void createCommandObjects();
    void createSyncObjects();
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
    std::vector<VkSemaphore> imageAvailable_;
    std::vector<VkSemaphore> renderFinished_;
    std::vector<VkFence> inFlightFences_;
    uint32_t currentFrame_ = 0;
    uint32_t currentImageIndex_ = 0;

    HWND hwnd_ = nullptr;
    int pendingWidth_ = 0, pendingHeight_ = 0;
    bool resizePending_ = false;
};
```

- [ ] **Step 2: Write `src/gfx/vk_core.cpp` — instance, surface, physical device, logical device**

```cpp
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
```

- [ ] **Step 3: Continue `vk_core.cpp` — swapchain, render pass, framebuffers**

```cpp
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
    imageAvailable_.resize(kFramesInFlight);
    renderFinished_.resize(kFramesInFlight);
    inFlightFences_.resize(kFramesInFlight);
    VkSemaphoreCreateInfo sci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (int i = 0; i < kFramesInFlight; i++) {
        vkCreateSemaphore(device_, &sci, nullptr, &imageAvailable_[i]);
        vkCreateSemaphore(device_, &sci, nullptr, &renderFinished_[i]);
        vkCreateFence(device_, &fci, nullptr, &inFlightFences_[i]);
    }
}
```

- [ ] **Step 4: Continue `vk_core.cpp` — public init/resize/begin/end/cleanup**

```cpp
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
}

bool VkCore::beginFrame(FrameContext& outCtx) {
    if (resizePending_) {
        resizePending_ = false;
        recreateSwapchain();
        if (swapchain_ == VK_NULL_HANDLE) return false; // still minimized
    }

    vkWaitForFences(device_, 1, &inFlightFences_[currentFrame_], VK_TRUE, UINT64_MAX);

    VkResult acquireResult = vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX,
        imageAvailable_[currentFrame_], VK_NULL_HANDLE, &currentImageIndex_);
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

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.waitSemaphoreCount = 1;
    si.pWaitSemaphores = &imageAvailable_[currentFrame_];
    si.pWaitDstStageMask = &waitStage;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores = &renderFinished_[currentFrame_];
    vkQueueSubmit(graphicsQueue_, 1, &si, inFlightFences_[currentFrame_]);

    VkPresentInfoKHR pi{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores = &renderFinished_[currentFrame_];
    pi.swapchainCount = 1;
    pi.pSwapchains = &swapchain_;
    pi.pImageIndices = &currentImageIndex_;
    VkResult presentResult = vkQueuePresentKHR(graphicsQueue_, &pi);
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
        notifyResize(swapchainExtent_.width, swapchainExtent_.height);
    }

    currentFrame_ = (currentFrame_ + 1) % kFramesInFlight;
}

void VkCore::cleanup() {
    vkDeviceWaitIdle(device_);
    for (int i = 0; i < kFramesInFlight; i++) {
        vkDestroySemaphore(device_, imageAvailable_[i], nullptr);
        vkDestroySemaphore(device_, renderFinished_[i], nullptr);
        vkDestroyFence(device_, inFlightFences_[i], nullptr);
    }
    vkDestroyCommandPool(device_, commandPool_, nullptr);
    destroySwapchain();
    vkDestroyRenderPass(device_, renderPass_, nullptr);
    vkDestroyDevice(device_, nullptr);
    vkDestroySurfaceKHR(instance_, surface_, nullptr);
    vkDestroyInstance(instance_, nullptr);
}
```

- [ ] **Step 5: Link Vulkan and add sources in `CMakeLists.txt`**

```cmake
find_package(Vulkan REQUIRED)
target_sources(windows_ui_demo PRIVATE src/gfx/vk_core.cpp)
target_link_libraries(windows_ui_demo PRIVATE Vulkan::Vulkan)
```

- [ ] **Step 6: Wire it into `main.cpp`**

```cpp
#include "platform/window.h"
#include "gfx/vk_core.h"

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int) {
    Window window;
    if (!window.create(hInst, 1280, 800, L"windows_ui_demo")) return 1;

    VkCore vk;
    if (!vk.init(hInst, window.hwnd(), 1280, 800)) return 1;

    while (window.pumpMessages()) {
        int w, h;
        if (window.consumeResized(w, h)) vk.notifyResize(w, h);

        FrameContext frame;
        if (vk.beginFrame(frame)) {
            vk.endFrame();
        }
    }
    vk.cleanup();
    return 0;
}
```

- [ ] **Step 7: Build and manually verify**

```bash
cmake --build build_debug --target windows_ui_demo
./build_debug/windows_ui_demo.exe
```

Expected: window opens filled with solid mid-gray; resizing the window doesn't crash and the gray fill keeps covering the whole client area.

- [ ] **Step 8: Commit**

```bash
git_wrapper commit "Add Vulkan bootstrap: instance/device/swapchain/render pass, clears to gray"
```

---

### Task 4: gfx/primitives — grayscale quad/line/bezier batcher and pipeline

**Files:**
- Create: `src/gfx/primitives.h`, `src/gfx/primitives.cpp`
- Test: `src/gfx/primitives_test.cpp`
- Create: `shaders/shape.vert`, `shaders/shape.frag`
- Create: `src/gfx/shape_pipeline.h`, `src/gfx/shape_pipeline.cpp`
- Modify: `CMakeLists.txt`
- Modify: `src/app/main.cpp`

**Interfaces:**
- Produces:
  ```cpp
  struct Vertex { float x, y; float gray; float alpha; }; // pos in screen px
  class PrimitiveBatch {
   public:
      void reset();
      void pushRect(float x, float y, float w, float h, float gray, float alpha = 1.0f);
      void pushRoundedRect(float x, float y, float w, float h, float radius,
                           float gray, float alpha = 1.0f, int cornerSegments = 8);
      void pushLine(float x0, float y0, float x1, float y1, float thickness,
                   float gray, float alpha = 1.0f);
      void pushBezier(float x0, float y0, float cx0, float cy0, float cx1, float cy1,
                      float x1, float y1, float thickness, float gray,
                      float alpha = 1.0f, int segments = 24);
      const std::vector<Vertex>& vertices() const;
  };
  class ShapePipeline {
   public:
      void init(VkDevice device, VkRenderPass renderPass);
      void draw(VkCommandBuffer cmd, VkExtent2D extent, const PrimitiveBatch& batch);
      void cleanup(VkDevice device);
  };
  ```
- Consumes: `FrameContext` from Task 3 (`cmd`, `renderPass`, `extent`).

- [ ] **Step 1: Write the failing tests for the batcher**

```cpp
// src/gfx/primitives_test.cpp
#include "primitives.h"
#include <cassert>
#include <cmath>
#include <cstdio>

static bool nearlyEqual(float a, float b) { return std::fabs(a - b) < 0.001f; }

int main() {
    PrimitiveBatch batch;

    // pushRect emits exactly 6 verts (2 triangles), all with the given gray/alpha
    batch.pushRect(10, 20, 100, 50, 0.75f, 0.9f);
    assert(batch.vertices().size() == 6);
    for (auto& v : batch.vertices()) {
        assert(nearlyEqual(v.gray, 0.75f));
        assert(nearlyEqual(v.alpha, 0.9f));
    }
    // Corners present: (10,20) and (110,70) must both appear among the 6 verts
    bool sawTopLeft = false, sawBottomRight = false;
    for (auto& v : batch.vertices()) {
        if (nearlyEqual(v.x, 10) && nearlyEqual(v.y, 20)) sawTopLeft = true;
        if (nearlyEqual(v.x, 110) && nearlyEqual(v.y, 70)) sawBottomRight = true;
    }
    assert(sawTopLeft && sawBottomRight);

    // reset() clears everything
    batch.reset();
    assert(batch.vertices().empty());

    // pushLine emits a thin quad (6 verts) regardless of orientation
    batch.pushLine(0, 0, 100, 0, 4.0f, 0.5f);
    assert(batch.vertices().size() == 6);
    // thickness 4 → some vertex must be offset by 2px perpendicular (y = ±2)
    bool sawOffset = false;
    for (auto& v : batch.vertices()) {
        if (nearlyEqual(v.y, 2.0f) || nearlyEqual(v.y, -2.0f)) sawOffset = true;
    }
    assert(sawOffset);

    // pushBezier with N segments emits N * 6 verts (one quad per segment)
    batch.reset();
    batch.pushBezier(0, 0, 10, -20, 40, -20, 50, 0, 2.0f, 0.3f, 1.0f, /*segments=*/24);
    assert(batch.vertices().size() == 24 * 6);

    printf("primitives_test: OK\n");
    return 0;
}
```

- [ ] **Step 2: Add test target and run it to verify it fails**

```cmake
add_executable(primitives_test src/gfx/primitives.cpp src/gfx/primitives_test.cpp)
```

```bash
cmake -G Ninja -B build_debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build_debug --target primitives_test
```

Expected: FAIL — `primitives.h`/`primitives.cpp` don't exist yet.

- [ ] **Step 3: Write `src/gfx/primitives.h`**

```cpp
#pragma once
#include <vector>

struct Vertex { float x, y; float gray; float alpha; };

class PrimitiveBatch {
 public:
    void reset() { verts_.clear(); }

    void pushRect(float x, float y, float w, float h, float gray, float alpha = 1.0f);
    void pushRoundedRect(float x, float y, float w, float h, float radius,
                         float gray, float alpha = 1.0f, int cornerSegments = 8);
    void pushLine(float x0, float y0, float x1, float y1, float thickness,
                 float gray, float alpha = 1.0f);
    void pushBezier(float x0, float y0, float cx0, float cy0, float cx1, float cy1,
                    float x1, float y1, float thickness, float gray,
                    float alpha = 1.0f, int segments = 24);

    const std::vector<Vertex>& vertices() const { return verts_; }

 private:
    void pushQuad(float ax, float ay, float bx, float by,
                 float cx, float cy, float dx, float dy,
                 float gray, float alpha);
    std::vector<Vertex> verts_;
};
```

- [ ] **Step 4: Write `src/gfx/primitives.cpp`**

```cpp
#include "primitives.h"
#include <cmath>

void PrimitiveBatch::pushQuad(float ax, float ay, float bx, float by,
                              float cx, float cy, float dx, float dy,
                              float gray, float alpha) {
    // a-b-c-d wound as a quad: triangles (a,b,c) and (a,c,d)
    verts_.push_back({ax, ay, gray, alpha});
    verts_.push_back({bx, by, gray, alpha});
    verts_.push_back({cx, cy, gray, alpha});
    verts_.push_back({ax, ay, gray, alpha});
    verts_.push_back({cx, cy, gray, alpha});
    verts_.push_back({dx, dy, gray, alpha});
}

void PrimitiveBatch::pushRect(float x, float y, float w, float h, float gray, float alpha) {
    pushQuad(x, y, x + w, y, x + w, y + h, x, y + h, gray, alpha);
}

void PrimitiveBatch::pushLine(float x0, float y0, float x1, float y1, float thickness,
                              float gray, float alpha) {
    float dx = x1 - x0, dy = y1 - y0;
    float len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-6f) return;
    float nx = -dy / len * (thickness * 0.5f);
    float ny =  dx / len * (thickness * 0.5f);
    pushQuad(x0 + nx, y0 + ny, x1 + nx, y1 + ny, x1 - nx, y1 - ny, x0 - nx, y0 - ny, gray, alpha);
}

void PrimitiveBatch::pushBezier(float x0, float y0, float cx0, float cy0,
                                float cx1, float cy1, float x1, float y1,
                                float thickness, float gray, float alpha, int segments) {
    float prevX = x0, prevY = y0;
    for (int i = 1; i <= segments; i++) {
        float t = (float)i / (float)segments;
        float u = 1.0f - t;
        float x = u * u * u * x0 + 3 * u * u * t * cx0 + 3 * u * t * t * cx1 + t * t * t * x1;
        float y = u * u * u * y0 + 3 * u * u * t * cy0 + 3 * u * t * t * cy1 + t * t * t * y1;
        pushLine(prevX, prevY, x, y, thickness, gray, alpha);
        prevX = x; prevY = y;
    }
}

void PrimitiveBatch::pushRoundedRect(float x, float y, float w, float h, float radius,
                                     float gray, float alpha, int cornerSegments) {
    if (radius <= 0.0f) { pushRect(x, y, w, h, gray, alpha); return; }
    float r = radius;
    // Center cross: one rect spanning the full width (minus corner insets) and
    // one spanning the full height, covering everything except the four
    // corner squares — then each corner square is filled by a triangle fan
    // approximating a quarter-circle.
    pushRect(x + r, y, w - 2 * r, h, gray, alpha);
    pushRect(x, y + r, r, h - 2 * r, gray, alpha);
    pushRect(x + w - r, y + r, r, h - 2 * r, gray, alpha);

    struct Corner { float cx, cy, startDeg, endDeg; };
    Corner corners[4] = {
        {x + r,     y + r,     180.0f, 270.0f}, // top-left
        {x + w - r, y + r,     270.0f, 360.0f}, // top-right
        {x + w - r, y + h - r,   0.0f,  90.0f}, // bottom-right
        {x + r,     y + h - r,  90.0f, 180.0f}, // bottom-left
    };
    for (auto& c : corners) {
        float prevX = c.cx + r * std::cos(c.startDeg * 3.14159265f / 180.0f);
        float prevY = c.cy + r * std::sin(c.startDeg * 3.14159265f / 180.0f);
        for (int i = 1; i <= cornerSegments; i++) {
            float t = c.startDeg + (c.endDeg - c.startDeg) * (float)i / (float)cornerSegments;
            float rad = t * 3.14159265f / 180.0f;
            float px = c.cx + r * std::cos(rad);
            float py = c.cy + r * std::sin(rad);
            // Triangle fan slice from the corner's arc center out to the arc.
            verts_.push_back({c.cx, c.cy, gray, alpha});
            verts_.push_back({prevX, prevY, gray, alpha});
            verts_.push_back({px, py, gray, alpha});
            prevX = px; prevY = py;
        }
    }
}
```

- [ ] **Step 5: Run tests to verify they pass**

```bash
cmake --build build_debug --target primitives_test
./build_debug/primitives_test.exe
```

Expected: prints `primitives_test: OK`.

- [ ] **Step 6: Write the grayscale shaders**

`shaders/shape.vert`:
```glsl
#version 450
layout(location = 0) in vec2 inPos;   // screen-space pixels
layout(location = 1) in float inGray;
layout(location = 2) in float inAlpha;

layout(push_constant) uniform PushConstants { vec2 screenSize; } pc;

layout(location = 0) out float outGray;
layout(location = 1) out float outAlpha;

void main() {
    vec2 ndc = (inPos / pc.screenSize) * 2.0 - 1.0;
    gl_Position = vec4(ndc, 0.0, 1.0);
    outGray = inGray;
    outAlpha = inAlpha;
}
```

`shaders/shape.frag`:
```glsl
#version 450
layout(location = 0) in float inGray;
layout(location = 1) in float inAlpha;
layout(location = 0) out vec4 outColor;

void main() {
    // Grayscale-by-construction: R, G, B are always the same value.
    outColor = vec4(inGray, inGray, inGray, inAlpha);
}
```

- [ ] **Step 7: Write `src/gfx/shape_pipeline.h`**

```cpp
#pragma once
#include <vulkan/vulkan.h>
#include "primitives.h"

class ShapePipeline {
 public:
    void init(VkDevice device, VkRenderPass renderPass);
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
```

- [ ] **Step 8: Write `src/gfx/shape_pipeline.cpp`**

```cpp
#include "shape_pipeline.h"
#include <fstream>
#include <vector>
#include <cstring>
#include <stdexcept>

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

void ShapePipeline::init(VkDevice device, VkRenderPass renderPass) {
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
    // physicalDevice_ is set by the caller-visible init() contract below via
    // shape_pipeline.h's ShapePipeline::init(device, renderPass) — the memory
    // type search additionally needs the physical device, passed through the
    // same init() call in Step 9's usage (see draw-site wiring in Task 9).
    mai.memoryTypeIndex = findMemoryType(physicalDevice_, memReq.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vkAllocateMemory(device, &mai, nullptr, &vertexMemory_);
    vkBindBufferMemory(device, vertexBuffer_, vertexMemory_, 0);
    vkMapMemory(device, vertexMemory_, 0, bci.size, 0, &mapped_);

    VkShaderModule vert = loadShaderModule(device, "shaders/shape.vert.spv");
    VkShaderModule frag = loadShaderModule(device, "shaders/shape.frag.spv");

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
    VkVertexInputAttributeDescription attrs[3] = {
        {0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, x)},
        {1, 0, VK_FORMAT_R32_SFLOAT,    offsetof(Vertex, gray)},
        {2, 0, VK_FORMAT_R32_SFLOAT,    offsetof(Vertex, alpha)},
    };
    VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vi.vertexBindingDescriptionCount = 1;
    vi.pVertexBindingDescriptions = &binding;
    vi.vertexAttributeDescriptionCount = 3;
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
    size_t bytes = verts.size() * sizeof(Vertex);
    std::memcpy(mapped_, verts.data(), bytes);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

    VkViewport viewport{0, 0, (float)extent.width, (float)extent.height, 0.0f, 1.0f};
    VkRect2D scissor{{0, 0}, extent};
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    float screenSize[2] = { (float)extent.width, (float)extent.height };
    vkCmdPushConstants(cmd, layout_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(screenSize), screenSize);

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer_, &offset);
    vkCmdDraw(cmd, (uint32_t)verts.size(), 1, 0, 0);
}

void ShapePipeline::cleanup(VkDevice device) {
    vkDestroyPipeline(device, pipeline_, nullptr);
    vkDestroyPipelineLayout(device, layout_, nullptr);
    vkUnmapMemory(device, vertexMemory_);
    vkFreeMemory(device, vertexMemory_, nullptr);
    vkDestroyBuffer(device, vertexBuffer_, nullptr);
}
```

Note the comment flagged in Step 8's code: `physicalDevice_` must be set before `findMemoryType` is called. Add a `VkPhysicalDevice physicalDevice` parameter to `ShapePipeline::init` and store it in `physicalDevice_` as the very first line of `init()` — update both `shape_pipeline.h`'s signature (`void init(VkDevice device, VkPhysicalDevice physicalDevice, VkRenderPass renderPass);`) and the `.cpp` accordingly before building.

- [ ] **Step 9: Add sources/shader compilation to `CMakeLists.txt`**

```cmake
target_sources(windows_ui_demo PRIVATE
    src/gfx/primitives.cpp
    src/gfx/shape_pipeline.cpp
)
compile_glsl_shaders(compile_shape_shaders
    ${CMAKE_BINARY_DIR}/shaders
    ${CMAKE_SOURCE_DIR}/shaders
    shape.vert shape.frag
)
add_dependencies(windows_ui_demo compile_shape_shaders)
```

- [ ] **Step 10: Wire a test rect + line + bezier into `main.cpp`**

```cpp
#include "platform/window.h"
#include "gfx/vk_core.h"
#include "gfx/shape_pipeline.h"

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int) {
    Window window;
    if (!window.create(hInst, 1280, 800, L"windows_ui_demo")) return 1;

    VkCore vk;
    if (!vk.init(hInst, window.hwnd(), 1280, 800)) return 1;

    ShapePipeline shapes;
    shapes.init(vk.device(), vk.physicalDevice(), /*renderPass=*/VK_NULL_HANDLE); // see note below

    PrimitiveBatch batch;

    while (window.pumpMessages()) {
        int w, h;
        if (window.consumeResized(w, h)) vk.notifyResize(w, h);

        FrameContext frame;
        if (vk.beginFrame(frame)) {
            batch.reset();
            batch.pushRect(100, 100, 200, 120, 0.9f);
            batch.pushRoundedRect(400, 100, 200, 120, 24.0f, 0.9f);
            batch.pushBezier(700, 220, 750, 80, 850, 80, 900, 220, 3.0f, 0.9f);
            shapes.draw(frame.cmd, frame.extent, batch);
            vk.endFrame();
        }
    }
    shapes.cleanup(vk.device());
    vk.cleanup();
    return 0;
}
```

`ShapePipeline::init` needs the real `VkRenderPass` from `VkCore`, not `VK_NULL_HANDLE` — expose it by calling `shapes.init(vk.device(), vk.physicalDevice(), frame.renderPass)` right after the *first* successful `vk.beginFrame(frame)` call instead of before the loop (the render pass object itself is created once in `VkCore::init` and never changes across resizes, so initializing on the first frame is sufficient — restructure the loop so `shapes.init(...)` runs once, guarded by a `bool shapesReady = false;` flag, inside the `if (vk.beginFrame(frame))` block).

- [ ] **Step 11: Build and manually verify**

```bash
cmake --build build_debug --target windows_ui_demo
./build_debug/windows_ui_demo.exe
```

Expected: gray window showing a light-gray rectangle, a light-gray rounded rectangle, and a light-gray bezier curve stroke — all clearly visible against the mid-gray background, no color anywhere.

- [ ] **Step 12: Commit**

```bash
git_wrapper commit "Add grayscale primitive batcher (rect/rounded-rect/line/bezier) and shape pipeline"
```

---

### Task 5: anim — app-level animation helper

**Files:**
- Create: `src/anim/animated_float.h`, `src/anim/animated_float.cpp`
- Test: `src/anim/animated_float_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces:
  ```cpp
  using EaseFn = float(*)(float t); // t in [0,1], returns eased [0,1]
  float easeLinear(float t);
  float easeInOutCubic(float t);
  class AnimatedFloat {
   public:
      explicit AnimatedFloat(float initial = 0.0f);
      void set(float target, float durationSeconds, EaseFn ease = easeLinear);
      void update(float dtSeconds);
      float value() const;
      bool isAnimating() const;
  };
  ```

- [ ] **Step 1: Write the failing tests**

```cpp
// src/anim/animated_float_test.cpp
#include "animated_float.h"
#include <cassert>
#include <cmath>
#include <cstdio>

static bool nearlyEqual(float a, float b) { return std::fabs(a - b) < 0.001f; }

int main() {
    // Linear ease: exact midpoint at half duration
    AnimatedFloat f(0.0f);
    f.set(10.0f, 2.0f, easeLinear);
    assert(f.isAnimating());
    f.update(1.0f); // halfway through a 2s animation
    assert(nearlyEqual(f.value(), 5.0f));
    assert(f.isAnimating());

    f.update(1.0f); // now fully elapsed
    assert(nearlyEqual(f.value(), 10.0f));
    assert(!f.isAnimating());

    // Overshoot must clamp exactly at the target, not extrapolate past it
    AnimatedFloat g(0.0f);
    g.set(4.0f, 1.0f, easeLinear);
    g.update(5.0f);
    assert(nearlyEqual(g.value(), 4.0f));
    assert(!g.isAnimating());

    // A fresh set() restarts elapsed time from the new current value
    AnimatedFloat h(0.0f);
    h.set(10.0f, 1.0f, easeLinear);
    h.update(0.5f); // value == 5.0
    h.set(20.0f, 1.0f, easeLinear); // now animates 5.0 -> 20.0
    assert(nearlyEqual(h.value(), 5.0f));
    h.update(0.5f);
    assert(nearlyEqual(h.value(), 12.5f));

    printf("animated_float_test: OK\n");
    return 0;
}
```

- [ ] **Step 2: Add test target and run it to verify it fails**

```cmake
add_executable(animated_float_test src/anim/animated_float.cpp src/anim/animated_float_test.cpp)
```

```bash
cmake -G Ninja -B build_debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build_debug --target animated_float_test
```

Expected: FAIL — files don't exist yet.

- [ ] **Step 3: Write `src/anim/animated_float.h`**

```cpp
#pragma once

using EaseFn = float(*)(float);

float easeLinear(float t);
float easeInOutCubic(float t);

class AnimatedFloat {
 public:
    explicit AnimatedFloat(float initial = 0.0f)
        : current_(initial), from_(initial), to_(initial) {}

    void set(float target, float durationSeconds, EaseFn ease = easeLinear) {
        from_ = current_;
        to_ = target;
        duration_ = durationSeconds;
        elapsed_ = 0.0f;
        ease_ = ease;
    }

    void update(float dtSeconds) {
        if (elapsed_ >= duration_) { current_ = to_; return; }
        elapsed_ += dtSeconds;
        if (elapsed_ >= duration_) { current_ = to_; return; }
        float t = duration_ > 0.0f ? elapsed_ / duration_ : 1.0f;
        current_ = from_ + (to_ - from_) * ease_(t);
    }

    float value() const { return current_; }
    bool isAnimating() const { return elapsed_ < duration_; }

 private:
    float current_, from_, to_;
    float elapsed_ = 0.0f, duration_ = 0.0f;
    EaseFn ease_ = easeLinear;
};
```

- [ ] **Step 4: Write `src/anim/animated_float.cpp`**

```cpp
#include "animated_float.h"
#include <cmath>

float easeLinear(float t) { return t; }

float easeInOutCubic(float t) {
    return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) / 2.0f;
}
```

- [ ] **Step 5: Run tests to verify they pass**

```bash
cmake --build build_debug --target animated_float_test
./build_debug/animated_float_test.exe
```

Expected: prints `animated_float_test: OK`.

- [ ] **Step 6: Add sources to `CMakeLists.txt`**

```cmake
target_sources(windows_ui_demo PRIVATE src/anim/animated_float.cpp)
```

- [ ] **Step 7: Build the main app to confirm no regressions**

```bash
cmake --build build_debug --target windows_ui_demo
```

Expected: builds clean (this task adds no visible behavior to the app yet — Task 10 wires `AnimatedFloat` into the Animation page).

- [ ] **Step 8: Commit**

```bash
git_wrapper commit "Add AnimatedFloat with linear/ease-in-out-cubic easing"
```

---

### Task 6: text — vulkan_font_engine MTSDF adapter

**Files:**
- Create: `src/text/text_renderer.h`, `src/text/text_renderer.cpp`
- Modify: `CMakeLists.txt`
- Modify: `src/app/main.cpp`

**Interfaces:**
- Consumes: `vk_font_core`'s `MsdfFont`, `MsdfTextRenderer`, `FileByteReader` (from `libs/firstparty/vulkan_font_engine/core/msdf.hh`, `msdf_renderer.hh`, `asset_reader.hh`).
- Produces:
  ```cpp
  class TextRenderer {
   public:
      bool init(VkDevice device, VkPhysicalDevice physicalDevice, VkRenderPass renderPass,
               uint32_t screenWidth, uint32_t screenHeight);
      // Must be called once, inside a command buffer that will be submitted
      // before any draw() call — uploads the atlas texture to the GPU.
      void recordAtlasUpload(VkCommandBuffer cmd);
      // Lays out `text` at (x, baselineY) in screen px and queues it for the
      // next draw() call this frame. bold selects the Bold weight slot.
      void drawText(std::string_view text, float x, float baselineY, float sizePx,
                   float gray, bool bold = false);
      // Uploads all glyph quads queued by drawText() since the last draw()
      // and records the actual draw calls.
      void draw(VkCommandBuffer cmd, VkExtent2D extent);
      float textWidth(std::string_view text, float sizePx) const;
      void cleanup();
  };
  ```

- [ ] **Step 1: Copy the two bundled fonts' cache-friendly layout (already done in Task 1) — confirm paths**

```bash
ls "C:\Users\incxiuefb\Documents\Files\clone\windows_UI_demo\fonts\lm"
```

Expected: `lmroman10-regular.otf` and `lmroman10-bold.otf` present (from Task 1, Step 2).

- [ ] **Step 2: Write `src/text/text_renderer.h`**

```cpp
#pragma once
#include <vulkan/vulkan.h>
#include <string_view>
#include <vector>
#include "msdf.hh"
#include "msdf_renderer.hh"
#include "asset_reader.hh"

class TextRenderer {
 public:
    bool init(VkDevice device, VkPhysicalDevice physicalDevice, VkRenderPass renderPass,
             uint32_t screenWidth, uint32_t screenHeight);
    void recordAtlasUpload(VkCommandBuffer cmd);
    void drawText(std::string_view text, float x, float baselineY, float sizePx,
                 float gray, bool bold = false);
    void draw(VkCommandBuffer cmd, VkExtent2D extent);
    float textWidth(std::string_view text, float sizePx) const;
    void cleanup();

 private:
    FileByteReader assets_;
    MsdfFont font_;
    MsdfTextRenderer renderer_;
    std::vector<float> pendingRegularVerts_;
    std::vector<float> pendingBoldVerts_;
};
```

- [ ] **Step 3: Write `src/text/text_renderer.cpp`**

```cpp
#include "text_renderer.h"

bool TextRenderer::init(VkDevice device, VkPhysicalDevice physicalDevice, VkRenderPass renderPass,
                        uint32_t screenWidth, uint32_t screenHeight) {
    const char* regularPath = "fonts/lm/lmroman10-regular.otf";
    const char* boldPath = "fonts/lm/lmroman10-bold.otf";
    const char* cachePath = "fonts/lmroman10-regular.msdf.cache";

    if (!font_.generate(assets_, regularPath, cachePath)) return false;
    if (!font_.hasStyle(FontStyle::Bold)) {
        if (font_.addStyle(assets_, boldPath, FontStyle::Bold)) {
            font_.saveCache(cachePath);
        }
    }

    renderer_.init(device, physicalDevice, assets_, screenWidth, screenHeight);
    renderer_.createResources(renderPass, font_, /*weightIdx=*/0); // Regular
    renderer_.createResources(renderPass, font_, /*weightIdx=*/1); // Bold
    return true;
}

void TextRenderer::recordAtlasUpload(VkCommandBuffer cmd) {
    renderer_.recordAtlasUpload(cmd, 0);
    renderer_.recordAtlasUpload(cmd, 1);
}

void TextRenderer::drawText(std::string_view text, float x, float baselineY, float sizePx,
                            float gray, bool bold) {
    auto& out = bold ? pendingBoldVerts_ : pendingRegularVerts_;
    float penX = x;
    for (char c : text) {
        penX = font_.emitGlyph(out, (uint32_t)(unsigned char)c, penX, baselineY, sizePx,
                               gray, gray, gray, 1.0f);
    }
}

void TextRenderer::draw(VkCommandBuffer cmd, VkExtent2D extent) {
    if (!pendingRegularVerts_.empty()) {
        uint32_t count = (uint32_t)(pendingRegularVerts_.size() / MsdfFont::FLOATS_PER_VERT);
        renderer_.uploadGlyphQuads(pendingRegularVerts_.data(), count, 0);
        renderer_.draw(cmd, renderer_.vertOffset(0), count, 0, 0, 0, 0, extent.width, extent.height, 0);
    }
    if (!pendingBoldVerts_.empty()) {
        uint32_t count = (uint32_t)(pendingBoldVerts_.size() / MsdfFont::FLOATS_PER_VERT);
        renderer_.uploadGlyphQuads(pendingBoldVerts_.data(), count, 1);
        renderer_.draw(cmd, renderer_.vertOffset(1), count, 0, 0, 0, 0, extent.width, extent.height, 1);
    }
    pendingRegularVerts_.clear();
    pendingBoldVerts_.clear();
}

float TextRenderer::textWidth(std::string_view text, float sizePx) const {
    return font_.textWidth(text, sizePx);
}

void TextRenderer::cleanup() {
    renderer_.cleanup();
}
```

- [ ] **Step 4: Add sources and include path in `CMakeLists.txt`**

```cmake
target_sources(windows_ui_demo PRIVATE src/text/text_renderer.cpp)
target_include_directories(windows_ui_demo PRIVATE
    libs/firstparty/vulkan_font_engine/core
)
```

- [ ] **Step 5: Compile the pre-existing MSDF shaders**

```cmake
compile_slang_shaders(compile_msdf_shaders
    ${CMAKE_BINARY_DIR}/shaders
    ${CMAKE_SOURCE_DIR}/libs/firstparty/vulkan_font_engine/shaders_src
    msdf_vert msdf_frag
)
add_dependencies(windows_ui_demo compile_msdf_shaders)
```

- [ ] **Step 6: Wire text into `main.cpp`**

Add a `TextRenderer text;` alongside `ShapePipeline shapes;`, initialize it in the same first-frame-guarded block as `shapes.init(...)` (`text.init(vk.device(), vk.physicalDevice(), frame.renderPass, frame.extent.width, frame.extent.height);` then `text.recordAtlasUpload(frame.cmd);` — the atlas upload must be recorded into that same first command buffer before any `draw()` call), and each frame call:

```cpp
text.drawText("windows_ui_demo — MTSDF text", 100, 300, 32.0f, 0.95f);
text.drawText("Bold weight", 100, 350, 32.0f, 0.95f, /*bold=*/true);
// ... after shapes.draw(...) in the same frame:
text.draw(frame.cmd, frame.extent);
```

- [ ] **Step 7: Build and manually verify**

```bash
cmake --build build_debug --target windows_ui_demo
./build_debug/windows_ui_demo.exe
```

Expected: two crisp lines of white-gray text render over the gray background and shapes from Task 4, one regular-weight and one bold-weight, both perfectly grayscale.

- [ ] **Step 8: Commit**

```bash
git_wrapper commit "Add MTSDF text adapter over vulkan_font_engine (Regular + Bold weights)"
```

---

### Task 7: ui/widgets — Button and Toggle

**Files:**
- Create: `src/ui/widgets.h`, `src/ui/widgets.cpp`
- Test: `src/ui/widgets_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `InputState` (Task 2), `PrimitiveBatch` + `TextRenderer` (Tasks 4/6) for rendering.
- Produces:
  ```cpp
  bool pointInRect(float px, float py, float x, float y, float w, float h);

  struct Button {
      float x, y, w, h;
      // Returns true exactly on the frame the button transitions from
      // pressed+hovered to released+hovered (a completed click).
      bool update(const InputState& input);
      void draw(PrimitiveBatch& batch, TextRenderer& text, std::string_view label) const;
     private:
      bool wasDownLastFrame_ = false;
  };

  struct Toggle {
      float x, y, w, h;
      bool on = false;
      // Flips `on` and returns true on a completed click; returns false otherwise.
      bool update(const InputState& input);
      void draw(PrimitiveBatch& batch) const;
     private:
      bool wasDownLastFrame_ = false;
  };
  ```

- [ ] **Step 1: Write the failing tests**

```cpp
// src/ui/widgets_test.cpp
#include "widgets.h"
#include "../platform/input_state.h"
#include <cassert>
#include <cstdio>

int main() {
    assert(pointInRect(50, 50, 0, 0, 100, 100) == true);
    assert(pointInRect(150, 50, 0, 0, 100, 100) == false);
    assert(pointInRect(0, 0, 0, 0, 100, 100) == true);   // inclusive top-left
    assert(pointInRect(100, 100, 0, 0, 100, 100) == false); // exclusive bottom-right

    Button btn{10, 10, 100, 40};
    InputState in{};

    // Frame 1: mouse moves over the button, not pressed — no click yet
    in.mouseX = 50; in.mouseY = 30; in.mouseDown = false;
    assert(btn.update(in) == false);

    // Frame 2: press while hovered — still not a completed click
    in.mouseDown = true;
    assert(btn.update(in) == false);

    // Frame 3: release while still hovered — this is the completed click
    in.mouseDown = false;
    assert(btn.update(in) == true);

    // Frame 4: pressing then releasing OUTSIDE the button must not click it
    Button btn2{10, 10, 100, 40};
    in.mouseX = 500; in.mouseY = 500; in.mouseDown = true;
    assert(btn2.update(in) == false);
    in.mouseDown = false;
    assert(btn2.update(in) == false);

    Toggle tgl{10, 10, 40, 40};
    in.mouseX = 20; in.mouseY = 20; in.mouseDown = true;
    assert(tgl.update(in) == false); // press edge, not yet a completed click
    in.mouseDown = false;
    assert(tgl.update(in) == true);  // release completes the click
    assert(tgl.on == true);
    in.mouseDown = true;
    tgl.update(in);
    in.mouseDown = false;
    assert(tgl.update(in) == true);
    assert(tgl.on == false); // second click flips it back off

    printf("widgets_test: OK\n");
    return 0;
}
```

- [ ] **Step 2: Add test target and run it to verify it fails**

```cmake
add_executable(widgets_test src/ui/widgets.cpp src/platform/input_state.cpp src/ui/widgets_test.cpp)
```

```bash
cmake -G Ninja -B build_debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build_debug --target widgets_test
```

Expected: FAIL — `widgets.h`/`widgets.cpp` don't exist yet.

- [ ] **Step 3: Write `src/ui/widgets.h`**

```cpp
#pragma once
#include <string_view>
#include "../platform/input_state.h"
#include "../gfx/primitives.h"
#include "../text/text_renderer.h"

bool pointInRect(float px, float py, float x, float y, float w, float h);

struct Button {
    float x, y, w, h;

    bool update(const InputState& input) {
        bool hovered = pointInRect(input.mouseX, input.mouseY, x, y, w, h);
        bool clicked = wasDownLastFrame_ && !input.mouseDown && hovered;
        wasDownLastFrame_ = hovered && input.mouseDown;
        return clicked;
    }

    void draw(PrimitiveBatch& batch, TextRenderer& text, std::string_view label) const {
        batch.pushRoundedRect(x, y, w, h, 6.0f, wasDownLastFrame_ ? 0.35f : 0.55f);
        float textW = text.textWidth(label, 20.0f);
        text.drawText(label, x + (w - textW) * 0.5f, y + h * 0.5f + 7.0f, 20.0f, 0.95f);
    }

   private:
    bool wasDownLastFrame_ = false;
};

struct Toggle {
    float x, y, w, h;
    bool on = false;

    bool update(const InputState& input) {
        bool hovered = pointInRect(input.mouseX, input.mouseY, x, y, w, h);
        bool clicked = wasDownLastFrame_ && !input.mouseDown && hovered;
        wasDownLastFrame_ = hovered && input.mouseDown;
        if (clicked) on = !on;
        return clicked;
    }

    void draw(PrimitiveBatch& batch) const {
        batch.pushRoundedRect(x, y, w, h, h * 0.5f, on ? 0.7f : 0.3f);
        float knobD = h - 8.0f;
        float knobX = on ? (x + w - knobD - 4.0f) : (x + 4.0f);
        batch.pushRoundedRect(knobX, y + 4.0f, knobD, knobD, knobD * 0.5f, 0.95f);
    }

   private:
    bool wasDownLastFrame_ = false;
};
```

- [ ] **Step 4: Write `src/ui/widgets.cpp`**

```cpp
#include "widgets.h"

bool pointInRect(float px, float py, float x, float y, float w, float h) {
    return px >= x && py >= y && px < x + w && py < y + h;
}
```

- [ ] **Step 5: Run tests to verify they pass**

```bash
cmake --build build_debug --target widgets_test
./build_debug/widgets_test.exe
```

Expected: prints `widgets_test: OK`.

- [ ] **Step 6: Add sources to `CMakeLists.txt`**

```cmake
target_sources(windows_ui_demo PRIVATE src/ui/widgets.cpp)
```

- [ ] **Step 7: Build the main app to confirm no regressions**

```bash
cmake --build build_debug --target windows_ui_demo
```

Expected: builds clean (Task 9 wires Button/Toggle into the actual Widgets page).

- [ ] **Step 8: Commit**

```bash
git_wrapper commit "Add Button and Toggle widgets with hit-test unit tests"
```

---

### Task 8: ui/widgets — Slider and List

**Files:**
- Modify: `src/ui/widgets.h`, `src/ui/widgets.cpp`
- Modify: `src/ui/widgets_test.cpp`

**Interfaces:**
- Produces (appended to the same header from Task 7):
  ```cpp
  struct Slider {
      float x, y, w, h;
      float minValue, maxValue;
      float value;
      // Updates `value` from mouse X while dragging; returns true while a drag
      // is in progress (value changed this frame).
      bool update(const InputState& input);
      void draw(PrimitiveBatch& batch) const;
     private:
      bool dragging_ = false;
  };

  struct ListBox {
      float x, y, w, h;
      float rowHeight;
      int itemCount;
      int selectedIndex = -1;
      // Returns true (and updates selectedIndex) if a row was clicked this frame.
      bool update(const InputState& input);
      void draw(PrimitiveBatch& batch, TextRenderer& text,
               const std::vector<std::string>& labels) const;
  };

  float sliderValueFromMouseX(float mouseX, float trackX, float trackW,
                              float minValue, float maxValue);
  int listIndexFromMouseY(float mouseY, float trackY, float rowHeight, int itemCount);
  ```

- [ ] **Step 1: Add the failing tests to `src/ui/widgets_test.cpp`** (append before the final `printf`/`return`)

```cpp
    // sliderValueFromMouseX: clamps to [min, max], linear in between
    assert(nearlyEqualForTest(sliderValueFromMouseX(0.0f, 0.0f, 100.0f, 0.0f, 10.0f), 0.0f));
    assert(nearlyEqualForTest(sliderValueFromMouseX(50.0f, 0.0f, 100.0f, 0.0f, 10.0f), 5.0f));
    assert(nearlyEqualForTest(sliderValueFromMouseX(100.0f, 0.0f, 100.0f, 0.0f, 10.0f), 10.0f));
    assert(nearlyEqualForTest(sliderValueFromMouseX(-50.0f, 0.0f, 100.0f, 0.0f, 10.0f), 0.0f)); // clamp low
    assert(nearlyEqualForTest(sliderValueFromMouseX(500.0f, 0.0f, 100.0f, 0.0f, 10.0f), 10.0f)); // clamp high

    // listIndexFromMouseY: row index by integer division, out-of-range -> -1
    assert(listIndexFromMouseY(5.0f, 0.0f, 20.0f, 5) == 0);
    assert(listIndexFromMouseY(25.0f, 0.0f, 20.0f, 5) == 1);
    assert(listIndexFromMouseY(-5.0f, 0.0f, 20.0f, 5) == -1);   // above the list
    assert(listIndexFromMouseY(1000.0f, 0.0f, 20.0f, 5) == -1); // below the last row

    Slider slider{10, 10, 200, 20, 0.0f, 100.0f, 0.0f};
    InputState in2{};
    in2.mouseX = 10; in2.mouseY = 15; in2.mouseDown = true;
    slider.update(in2); // press starts a drag at the track's left edge
    assert(nearlyEqualForTest(slider.value, 0.0f));
    in2.mouseX = 110; // drag to the middle
    slider.update(in2);
    assert(nearlyEqualForTest(slider.value, 50.0f));
    in2.mouseDown = false;
    slider.update(in2); // release ends the drag; value holds at 50
    assert(nearlyEqualForTest(slider.value, 50.0f));
```

Add a `nearlyEqualForTest` helper near the top of the file (rename the existing `nearlyEqual` from Task 4/5's tests is not shared across files — this file needs its own copy):

```cpp
static bool nearlyEqualForTest(float a, float b) { return std::fabs(a - b) < 0.001f; }
```

(and add `#include <cmath>` and `#include <string>` `#include <vector>` at the top if not already present).

- [ ] **Step 2: Run test to verify it fails**

```bash
cmake --build build_debug --target widgets_test
```

Expected: FAIL to compile — `Slider`, `sliderValueFromMouseX`, `listIndexFromMouseY` not declared.

- [ ] **Step 3: Append to `src/ui/widgets.h`**

```cpp
float sliderValueFromMouseX(float mouseX, float trackX, float trackW,
                            float minValue, float maxValue) {
    float t = trackW > 0.0f ? (mouseX - trackX) / trackW : 0.0f;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return minValue + (maxValue - minValue) * t;
}

int listIndexFromMouseY(float mouseY, float trackY, float rowHeight, int itemCount) {
    if (mouseY < trackY) return -1;
    int idx = (int)((mouseY - trackY) / rowHeight);
    if (idx < 0 || idx >= itemCount) return -1;
    return idx;
}

struct Slider {
    float x, y, w, h;
    float minValue, maxValue;
    float value;

    bool update(const InputState& input) {
        bool hovered = pointInRect(input.mouseX, input.mouseY, x, y, w, h);
        if (input.mouseDown && (hovered || dragging_)) {
            dragging_ = true;
            value = sliderValueFromMouseX(input.mouseX, x, w, minValue, maxValue);
            return true;
        }
        dragging_ = false;
        return false;
    }

    void draw(PrimitiveBatch& batch) const {
        batch.pushRoundedRect(x, y, w, h, h * 0.5f, 0.3f);
        float t = (maxValue > minValue) ? (value - minValue) / (maxValue - minValue) : 0.0f;
        float knobD = h + 8.0f;
        float knobX = x + t * w - knobD * 0.5f;
        batch.pushRoundedRect(knobX, y - 4.0f, knobD, knobD, knobD * 0.5f, 0.95f);
    }

   private:
    bool dragging_ = false;
};

struct ListBox {
    float x, y, w, h;
    float rowHeight;
    int itemCount;
    int selectedIndex = -1;

    bool update(const InputState& input) {
        if (!input.mouseWentUp) return false;
        if (!pointInRect(input.mouseX, input.mouseY, x, y, w, h)) return false;
        int idx = listIndexFromMouseY(input.mouseY, y, rowHeight, itemCount);
        if (idx < 0) return false;
        selectedIndex = idx;
        return true;
    }

    void draw(PrimitiveBatch& batch, TextRenderer& text,
             const std::vector<std::string>& labels) const {
        for (int i = 0; i < itemCount; i++) {
            float rowY = y + i * rowHeight;
            float gray = (i == selectedIndex) ? 0.5f : 0.2f;
            batch.pushRect(x, rowY, w, rowHeight - 2.0f, gray);
            text.drawText(labels[(size_t)i], x + 12.0f, rowY + rowHeight * 0.5f + 6.0f, 18.0f, 0.9f);
        }
    }
};
```

(`ListBox` needs `#include <string>` and `#include <vector>` at the top of `widgets.h`.)

- [ ] **Step 4: Run tests to verify they pass**

```bash
cmake --build build_debug --target widgets_test
./build_debug/widgets_test.exe
```

Expected: prints `widgets_test: OK`.

- [ ] **Step 5: Build the main app to confirm no regressions**

```bash
cmake --build build_debug --target windows_ui_demo
```

Expected: builds clean.

- [ ] **Step 6: Commit**

```bash
git_wrapper commit "Add Slider and ListBox widgets with value-mapping unit tests"
```

---

### Task 9: app — top nav, Text page, Shapes & Curves page

**Files:**
- Create: `src/app/nav.h`, `src/app/nav.cpp`
- Create: `src/app/page_text.h`, `src/app/page_text.cpp`
- Create: `src/app/page_shapes.h`, `src/app/page_shapes.cpp`
- Modify: `src/app/main.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `Button` (Task 7), `PrimitiveBatch`/`ShapePipeline` (Task 4), `TextRenderer` (Task 6), `InputState` (Task 2).
- Produces:
  ```cpp
  enum class Page { Text, Shapes, Widgets, Animation };
  class TopNav {
   public:
      // Returns the page to switch to if a tab was clicked this frame, else current.
      Page update(const InputState& input, Page current);
      void draw(PrimitiveBatch& batch, TextRenderer& text, Page current) const;
  };
  class TextPage { public: void draw(PrimitiveBatch& batch, TextRenderer& text); };
  class ShapesPage { public: void draw(PrimitiveBatch& batch); };
  ```

- [ ] **Step 1: Write `src/app/nav.h`**

```cpp
#pragma once
#include "../platform/input_state.h"
#include "../gfx/primitives.h"
#include "../text/text_renderer.h"
#include "../ui/widgets.h"
#include <array>

enum class Page { Text, Shapes, Widgets, Animation };

class TopNav {
 public:
    TopNav() {
        const char* labels[4] = {"Text", "Shapes & Curves", "Widgets", "Animation"};
        float x = 20.0f;
        for (int i = 0; i < 4; i++) {
            tabs_[i] = Button{x, 20.0f, 180.0f, 44.0f};
            labels_[i] = labels[i];
            x += 190.0f;
        }
    }

    Page update(const InputState& input, Page current) {
        for (int i = 0; i < 4; i++) {
            if (tabs_[i].update(input)) return (Page)i;
        }
        return current;
    }

    void draw(PrimitiveBatch& batch, TextRenderer& text, Page current) const {
        for (int i = 0; i < 4; i++) {
            float gray = ((Page)i == current) ? 0.8f : 0.4f;
            batch.pushRoundedRect(tabs_[i].x, tabs_[i].y, tabs_[i].w, tabs_[i].h, 6.0f, gray);
            float textW = text.textWidth(labels_[i], 18.0f);
            text.drawText(labels_[i], tabs_[i].x + (tabs_[i].w - textW) * 0.5f,
                          tabs_[i].y + tabs_[i].h * 0.5f + 6.0f, 18.0f, 0.05f);
        }
    }

 private:
    std::array<Button, 4> tabs_;
    std::array<const char*, 4> labels_;
};
```

- [ ] **Step 2: Write `src/app/page_text.h` / `.cpp`**

```cpp
// src/app/page_text.h
#pragma once
#include "../gfx/primitives.h"
#include "../text/text_renderer.h"

class TextPage {
 public:
    void draw(PrimitiveBatch& batch, TextRenderer& text);
};
```

```cpp
// src/app/page_text.cpp
#include "page_text.h"

void TextPage::draw(PrimitiveBatch& batch, TextRenderer& text) {
    text.drawText("The quick brown fox — 12pt", 60, 140, 12.0f, 0.9f);
    text.drawText("The quick brown fox — 20pt", 60, 190, 20.0f, 0.9f);
    text.drawText("The quick brown fox — 32pt", 60, 250, 32.0f, 0.9f);
    text.drawText("The quick brown fox — 56pt", 60, 340, 56.0f, 0.9f);
    text.drawText("Bold at 32pt", 60, 420, 32.0f, 0.9f, /*bold=*/true);
    (void)batch; // page currently draws text only — batch kept for symmetry with other pages
}
```

- [ ] **Step 3: Write `src/app/page_shapes.h` / `.cpp`**

```cpp
// src/app/page_shapes.h
#pragma once
#include "../gfx/primitives.h"

class ShapesPage {
 public:
    void draw(PrimitiveBatch& batch);
};
```

```cpp
// src/app/page_shapes.cpp
#include "page_shapes.h"

void ShapesPage::draw(PrimitiveBatch& batch) {
    batch.pushRect(60, 140, 160, 100, 0.85f);
    batch.pushRoundedRect(260, 140, 160, 100, 20.0f, 0.85f);
    batch.pushLine(460, 140, 620, 240, 4.0f, 0.85f);
    batch.pushBezier(660, 240, 700, 100, 820, 100, 860, 240, 4.0f, 0.85f);

    // A small grid of varying gray levels to prove the pipeline's alpha/gray range
    for (int i = 0; i < 8; i++) {
        float gray = (float)i / 7.0f;
        batch.pushRect(60.0f + i * 100.0f, 320, 80, 80, gray);
    }
}
```

- [ ] **Step 4: Rewrite `src/app/main.cpp` to page-switch between Text and Shapes**

```cpp
#include "platform/window.h"
#include "gfx/vk_core.h"
#include "gfx/shape_pipeline.h"
#include "text/text_renderer.h"
#include "nav.h"
#include "page_text.h"
#include "page_shapes.h"

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int) {
    Window window;
    if (!window.create(hInst, 1280, 800, L"windows_ui_demo")) return 1;

    VkCore vk;
    if (!vk.init(hInst, window.hwnd(), 1280, 800)) return 1;

    ShapePipeline shapes;
    TextRenderer text;
    bool ready = false;

    PrimitiveBatch batch;
    TopNav nav;
    TextPage textPage;
    ShapesPage shapesPage;
    Page currentPage = Page::Text;

    while (window.pumpMessages()) {
        int w, h;
        if (window.consumeResized(w, h)) vk.notifyResize(w, h);

        currentPage = nav.update(window.input(), currentPage);

        FrameContext frame;
        if (vk.beginFrame(frame)) {
            if (!ready) {
                shapes.init(vk.device(), vk.physicalDevice(), frame.renderPass);
                text.init(vk.device(), vk.physicalDevice(), frame.renderPass,
                         frame.extent.width, frame.extent.height);
                text.recordAtlasUpload(frame.cmd);
                ready = true;
            }

            batch.reset();
            nav.draw(batch, text, currentPage);
            switch (currentPage) {
                case Page::Text:   textPage.draw(batch, text); break;
                case Page::Shapes: shapesPage.draw(batch); break;
                default: break; // Widgets/Animation wired in Task 10
            }

            shapes.draw(frame.cmd, frame.extent, batch);
            text.draw(frame.cmd, frame.extent);
            vk.endFrame();
        }
    }
    text.cleanup();
    shapes.cleanup(vk.device());
    vk.cleanup();
    return 0;
}
```

- [ ] **Step 5: Add sources to `CMakeLists.txt`**

```cmake
target_sources(windows_ui_demo PRIVATE
    src/app/nav.cpp
    src/app/page_text.cpp
    src/app/page_shapes.cpp
)
```

Create an empty `src/app/nav.cpp` with just `#include "nav.h"` (all of `TopNav` is header-defined) so the source-list entry has a real file to compile.

- [ ] **Step 6: Build and manually verify**

```bash
cmake --build build_debug --target windows_ui_demo
./build_debug/windows_ui_demo.exe
```

Expected: top nav with 4 tabs; clicking "Text" shows four sizes of text plus one bold line; clicking "Shapes & Curves" shows a rect, rounded rect, line, bezier, and an 8-step greeble gray gradient row. Tab highlighting reflects the active page.

- [ ] **Step 7: Commit**

```bash
git_wrapper commit "Add top nav, Text page, and Shapes & Curves page"
```

---

### Task 10: app — Widgets page, Animation page, final wiring, README

**Files:**
- Create: `src/app/page_widgets.h`, `src/app/page_widgets.cpp`
- Create: `src/app/page_animation.h`, `src/app/page_animation.cpp`
- Modify: `src/app/main.cpp`
- Modify: `CMakeLists.txt`
- Create: `README.md`

**Interfaces:**
- Consumes: `Button`/`Toggle`/`Slider`/`ListBox` (Tasks 7/8), `AnimatedFloat`/`easeInOutCubic` (Task 5).
- Produces: `class WidgetsPage { public: void update(const InputState&); void draw(PrimitiveBatch&, TextRenderer&); };` and `class AnimationPage { public: void update(float dtSeconds, const InputState&); void draw(PrimitiveBatch&, TextRenderer&); };`

- [ ] **Step 1: Write `src/app/page_widgets.h`**

```cpp
#pragma once
#include "../ui/widgets.h"
#include "../text/text_renderer.h"
#include <vector>
#include <string>

class WidgetsPage {
 public:
    WidgetsPage();
    void update(const InputState& input);
    void draw(PrimitiveBatch& batch, TextRenderer& text);

 private:
    Button demoButton_{60, 140, 200, 50};
    int clickCount_ = 0;
    Toggle demoToggle_{60, 220, 60, 32};
    Slider demoSlider_{60, 300, 300, 20, 0.0f, 100.0f, 40.0f};
    ListBox demoList_{60, 360, 260, 30};
    std::vector<std::string> listLabels_{"Alpha", "Bravo", "Charlie", "Delta", "Echo"};
};
```

- [ ] **Step 2: Write `src/app/page_widgets.cpp`**

```cpp
#include "page_widgets.h"
#include <cstdio>

WidgetsPage::WidgetsPage() {
    demoList_.itemCount = (int)listLabels_.size();
}

void WidgetsPage::update(const InputState& input) {
    if (demoButton_.update(input)) clickCount_++;
    demoToggle_.update(input);
    demoSlider_.update(input);
    demoList_.update(input);
}

void WidgetsPage::draw(PrimitiveBatch& batch, TextRenderer& text) {
    demoButton_.draw(batch, text, "Click me");
    char clickLabel[32];
    std::snprintf(clickLabel, sizeof(clickLabel), "Clicks: %d", clickCount_);
    text.drawText(clickLabel, 280, 172, 18.0f, 0.9f);

    demoToggle_.draw(batch);
    text.drawText(demoToggle_.on ? "On" : "Off", 140, 244, 18.0f, 0.9f);

    demoSlider_.draw(batch);
    char sliderLabel[32];
    std::snprintf(sliderLabel, sizeof(sliderLabel), "%.0f", demoSlider_.value);
    text.drawText(sliderLabel, 380, 316, 18.0f, 0.9f);

    demoList_.draw(batch, text, listLabels_);
}
```

- [ ] **Step 3: Write `src/app/page_animation.h`**

```cpp
#pragma once
#include "../anim/animated_float.h"
#include "../gfx/primitives.h"
#include "../text/text_renderer.h"
#include "../ui/widgets.h"
#include "../platform/input_state.h"

class AnimationPage {
 public:
    AnimationPage() { fade_.set(1.0f, 1.5f, easeInOutCubic); moveX_.set(700.0f, 1.5f, easeInOutCubic); }
    void update(float dtSeconds, const InputState& input);
    void draw(PrimitiveBatch& batch, TextRenderer& text);

 private:
    Button replayButton_{60, 140, 200, 50};
    AnimatedFloat fade_{0.0f};
    AnimatedFloat moveX_{60.0f};
};
```

- [ ] **Step 4: Write `src/app/page_animation.cpp`**

```cpp
#include "page_animation.h"

void AnimationPage::update(float dtSeconds, const InputState& input) {
    if (replayButton_.update(input)) {
        fade_.set(fade_.value() > 0.5f ? 0.1f : 1.0f, 1.5f, easeInOutCubic);
        moveX_.set(moveX_.value() > 400.0f ? 60.0f : 700.0f, 1.5f, easeInOutCubic);
    }
    fade_.update(dtSeconds);
    moveX_.update(dtSeconds);
}

void AnimationPage::draw(PrimitiveBatch& batch, TextRenderer& text) {
    replayButton_.draw(batch, text, "Replay");
    batch.pushRoundedRect(moveX_.value(), 240, 120, 120, 16.0f, 0.9f, fade_.value());
}
```

- [ ] **Step 5: Wire both pages into `main.cpp`**

Add `#include "page_widgets.h"` and `#include "page_animation.h"`, instantiate `WidgetsPage widgetsPage;` and `AnimationPage animationPage;` alongside the existing page objects, and extend the `switch (currentPage)` block:

```cpp
                case Page::Widgets:
                    widgetsPage.update(window.input());
                    widgetsPage.draw(batch, text);
                    break;
                case Page::Animation: {
                    static auto lastTick = std::chrono::steady_clock::now();
                    auto now = std::chrono::steady_clock::now();
                    float dt = std::chrono::duration<float>(now - lastTick).count();
                    lastTick = now;
                    animationPage.update(dt, window.input());
                    animationPage.draw(batch, text);
                    break;
                }
```

Add `#include <chrono>` to the top of `main.cpp`.

- [ ] **Step 6: Add sources to `CMakeLists.txt`**

```cmake
target_sources(windows_ui_demo PRIVATE
    src/app/page_widgets.cpp
    src/app/page_animation.cpp
)
```

- [ ] **Step 7: Build and manually verify all four pages**

```bash
cmake --build build_debug --target windows_ui_demo
./build_debug/windows_ui_demo.exe
```

Manually check:
- **Text** page: four text sizes plus bold line render crisply.
- **Shapes & Curves** page: rect/rounded-rect/line/bezier plus 8-step gray gradient row.
- **Widgets** page: clicking "Click me" increments the counter; clicking the toggle flips it and its knob slides between sides; dragging the slider updates its numeric readout live; clicking a list row highlights it.
- **Animation** page: on load, the "Replay" button and a rounded square fade/slide in over ~1.5s; clicking "Replay" repeatedly toggles it back and forth smoothly, never snapping or overshooting.
- Resize the window on every page — nothing crashes, no artifacts, gray-only pixels throughout (a screenshot pixel-picker or Task Manager GPU tab is not required — this is a visual eyeball check per the spec's stated no-automated-test-suite decision for rendering).

- [ ] **Step 8: Write `README.md`**

```markdown
# windows_UI_demo

A standalone Vulkan grayscale UI showcase — text (MTSDF via `vulkan_font_engine`),
shapes/curves, interactive widgets, and animation. No audio content; see
`docs/superpowers/specs/2026-07-11-vulkan-bw-ui-demo-design.md` for the design
and `docs/superpowers/plans/2026-07-11-vulkan-bw-ui-demo.md` for how it was built.

## Build

Requires Visual Studio Build Tools (MSVC), CMake, Ninja, and the Vulkan SDK
(for `glslc`/`slangc`).

    build.bat

Output: `build\windows_ui_demo.exe` (or `build_debug\` for a Debug build).

## Layout

- `src/platform` — Win32 window + input
- `src/gfx` — Vulkan bootstrap + the grayscale primitive batcher/pipeline
- `src/text` — MTSDF text adapter over `libs/firstparty/vulkan_font_engine`
- `src/ui` — Button/Toggle/Slider/ListBox widgets
- `src/anim` — app-level animation (the renderer has no animation primitive)
- `src/app` — the four showcase pages behind a top nav
```

- [ ] **Step 9: Commit and push**

```bash
git_wrapper commit "Add Widgets and Animation pages, wire all four showcase pages, add README"
git_wrapper push
```

---

### Task 11: ui/layout — proportional scale + edge-dock + gap-chain, retrofit into all four pages

**Why this task exists:** every page from Tasks 9–10 places widgets at hardcoded absolute pixel coordinates. `windows_matrix_player` hit exactly this problem — controls drifted/overlapped across monitor resolutions — and fixed it with two techniques working together: `ResponsiveTextScale` (`libs/firstparty/vk_canvas/core/responsive_text.hh` — a value scales as `max(pct * actualHeight, floorPx)`) and `PlayerWindow::recalcLayout()` (`src/player_window.cpp:1252`), which computes every rect from current window size using edge-docking (`rcTransport_ = {0, H-transportH, W, H}`) and gap-chaining (`btnX += btnSize + btnGap`). This repo doesn't depend on vk_canvas, so this task reimplements the same *technique* standalone rather than reusing that file directly.

**Files:**
- Create: `src/ui/layout.h`, `src/ui/layout.cpp`
- Test: `src/ui/layout_test.cpp`
- Modify: `src/app/nav.h` (dock + row-chain the tabs)
- Modify: `src/app/page_text.cpp` (column-chain the text lines)
- Modify: `src/app/page_shapes.cpp` (column/row-chain the shapes)
- Modify: `src/app/page_widgets.h`, `src/app/page_widgets.cpp` (column-chain the widgets)
- Modify: `src/app/page_animation.h`, `src/app/page_animation.cpp` (dock/center the animated square's start/end positions)
- Modify: `src/app/main.cpp` (compute one `Rect contentArea` per frame from `frame.extent`, pass it to `nav`/pages)
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces:
  ```cpp
  struct Rect { float x, y, w, h; };

  struct UiScale {
      float referenceHeight;   // the window height these values were designed at
      float floorScale = 0.5f; // never shrink below this fraction of the design size
      float factor(float actualHeight) const;      // max(actualHeight/referenceHeight, floorScale)
      float scale(float value, float actualHeight) const; // value * factor(actualHeight)
  };

  // Each mutates `container` in place (shrinks it by `thickness`) and returns
  // the docked strip — same split `recalcLayout()` does for rcTransport_/rcGrid_.
  Rect dockTop(Rect& container, float thickness);
  Rect dockBottom(Rect& container, float thickness);
  Rect dockLeft(Rect& container, float thickness);
  Rect dockRight(Rect& container, float thickness);

  Rect centerIn(const Rect& container, float w, float h);

  class RowCursor {   // left-to-right gap-chaining, e.g. nav tabs, transport buttons
   public:
      RowCursor(float startX, float y, float gap);
      Rect next(float w, float h);
  };
  class ColumnCursor { // top-to-bottom gap-chaining, e.g. stacked widgets/text lines
   public:
      ColumnCursor(float x, float startY, float gap);
      Rect next(float w, float h);
  };
  ```
- Consumes: `frame.extent` (Task 3) as the per-frame `actualHeight`/window size driving `UiScale`.

- [ ] **Step 1: Write the failing tests**

```cpp
// src/ui/layout_test.cpp
#include "layout.h"
#include <cassert>
#include <cmath>
#include <cstdio>

static bool nearlyEqual(float a, float b) { return std::fabs(a - b) < 0.001f; }

int main() {
    // UiScale: 1.0 at the reference height, floored (not negative/zero) well
    // below it, and NOT capped above it (matches ResponsiveTextScale's
    // std::max-only formula — windows_matrix_player's tall-monitor case).
    UiScale s{661.0f, 0.5f};
    assert(nearlyEqual(s.factor(661.0f), 1.0f));
    assert(nearlyEqual(s.factor(200.0f), 0.5f));   // floored
    assert(nearlyEqual(s.factor(1322.0f), 2.0f));  // uncapped, scales up
    assert(nearlyEqual(s.scale(44.0f, 661.0f), 44.0f));
    assert(nearlyEqual(s.scale(44.0f, 1322.0f), 88.0f));

    // dockTop shrinks the container from the top and returns the strip
    Rect container{0, 0, 800, 600};
    Rect top = dockTop(container, 80.0f);
    assert(nearlyEqual(top.x, 0) && nearlyEqual(top.y, 0));
    assert(nearlyEqual(top.w, 800) && nearlyEqual(top.h, 80));
    assert(nearlyEqual(container.y, 80) && nearlyEqual(container.h, 520));
    assert(nearlyEqual(container.x, 0) && nearlyEqual(container.w, 800));

    // dockBottom shrinks from the bottom
    Rect container2{0, 0, 800, 600};
    Rect bottom = dockBottom(container2, 100.0f);
    assert(nearlyEqual(bottom.y, 500) && nearlyEqual(bottom.h, 100));
    assert(nearlyEqual(container2.h, 500));

    // dockLeft / dockRight shrink horizontally
    Rect container3{0, 0, 800, 600};
    Rect left = dockLeft(container3, 200.0f);
    assert(nearlyEqual(left.w, 200) && nearlyEqual(container3.x, 200) && nearlyEqual(container3.w, 600));

    // centerIn centers a w x h box within a container
    Rect box = centerIn(Rect{0, 0, 800, 600}, 200, 100);
    assert(nearlyEqual(box.x, 300) && nearlyEqual(box.y, 250));
    assert(nearlyEqual(box.w, 200) && nearlyEqual(box.h, 100));

    // RowCursor chains x by w+gap each call
    RowCursor row(10, 20, 8);
    Rect r1 = row.next(100, 40);
    Rect r2 = row.next(50, 40);
    assert(nearlyEqual(r1.x, 10) && nearlyEqual(r1.y, 20));
    assert(nearlyEqual(r2.x, 118)); // 10 + 100 + 8

    // ColumnCursor chains y by h+gap each call
    ColumnCursor col(10, 20, 5);
    Rect c1 = col.next(200, 30);
    Rect c2 = col.next(200, 30);
    assert(nearlyEqual(c1.y, 20) && nearlyEqual(c2.y, 55)); // 20 + 30 + 5

    printf("layout_test: OK\n");
    return 0;
}
```

- [ ] **Step 2: Add test target and run it to verify it fails**

```cmake
add_executable(layout_test src/ui/layout.cpp src/ui/layout_test.cpp)
```

```bash
cmake -G Ninja -B build_debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build_debug --target layout_test
```

Expected: FAIL — `layout.h`/`layout.cpp` don't exist yet.

- [ ] **Step 3: Write `src/ui/layout.h`**

```cpp
#pragma once
#include <algorithm>

struct Rect { float x, y, w, h; };

// Same scaling formula as windows_matrix_player's ResponsiveTextScale
// (libs/firstparty/vk_canvas/core/responsive_text.hh): a value declared at
// `referenceHeight` scales linearly with the actual window height, floored
// (never capped) so controls shrink gracefully on a small window but keep
// growing on a large one instead of staying pinned to their reference size.
struct UiScale {
    float referenceHeight;
    float floorScale = 0.5f;

    float factor(float actualHeight) const {
        return std::max(actualHeight / referenceHeight, floorScale);
    }
    float scale(float value, float actualHeight) const {
        return value * factor(actualHeight);
    }
};

Rect dockTop(Rect& container, float thickness);
Rect dockBottom(Rect& container, float thickness);
Rect dockLeft(Rect& container, float thickness);
Rect dockRight(Rect& container, float thickness);

Rect centerIn(const Rect& container, float w, float h);

class RowCursor {
 public:
    RowCursor(float startX, float y, float gap) : x_(startX), y_(y), gap_(gap) {}
    Rect next(float w, float h) {
        Rect r{x_, y_, w, h};
        x_ += w + gap_;
        return r;
    }
 private:
    float x_, y_, gap_;
};

class ColumnCursor {
 public:
    ColumnCursor(float x, float startY, float gap) : x_(x), y_(startY), gap_(gap) {}
    Rect next(float w, float h) {
        Rect r{x_, y_, w, h};
        y_ += h + gap_;
        return r;
    }
 private:
    float x_, y_, gap_;
};
```

- [ ] **Step 4: Write `src/ui/layout.cpp`**

```cpp
#include "layout.h"

Rect dockTop(Rect& container, float thickness) {
    Rect strip{container.x, container.y, container.w, thickness};
    container.y += thickness;
    container.h -= thickness;
    return strip;
}

Rect dockBottom(Rect& container, float thickness) {
    Rect strip{container.x, container.y + container.h - thickness, container.w, thickness};
    container.h -= thickness;
    return strip;
}

Rect dockLeft(Rect& container, float thickness) {
    Rect strip{container.x, container.y, thickness, container.h};
    container.x += thickness;
    container.w -= thickness;
    return strip;
}

Rect dockRight(Rect& container, float thickness) {
    Rect strip{container.x + container.w - thickness, container.y, thickness, container.h};
    container.w -= thickness;
    return strip;
}

Rect centerIn(const Rect& container, float w, float h) {
    return Rect{
        container.x + (container.w - w) * 0.5f,
        container.y + (container.h - h) * 0.5f,
        w, h
    };
}
```

- [ ] **Step 5: Run tests to verify they pass**

```bash
cmake --build build_debug --target layout_test
./build_debug/layout_test.exe
```

Expected: prints `layout_test: OK`.

- [ ] **Step 6: Add sources to `CMakeLists.txt`**

```cmake
target_sources(windows_ui_demo PRIVATE src/ui/layout.cpp)
```

- [ ] **Step 7: Retrofit `src/app/nav.h` to dock the strip and row-chain the tabs**

Replace the hardcoded `x = 20.0f; x += 190.0f;` loop in `TopNav`'s constructor and give it an `updateLayout(const Rect& windowRect, float uiScaleFactor)` method called once per frame from `main.cpp` (window size can change every frame via resize):

```cpp
#pragma once
#include "../platform/input_state.h"
#include "../gfx/primitives.h"
#include "../text/text_renderer.h"
#include "../ui/widgets.h"
#include "../ui/layout.h"
#include <array>

enum class Page { Text, Shapes, Widgets, Animation };

class TopNav {
 public:
    // Recomputes tab rects from the current window size — call once per
    // frame before update()/draw() so resizing never leaves stale rects.
    void updateLayout(Rect windowRect, float uiScaleFactor) {
        Rect strip = dockTop(windowRect, 64.0f * uiScaleFactor);
        RowCursor row(strip.x + 20.0f * uiScaleFactor, strip.y + 10.0f * uiScaleFactor,
                     10.0f * uiScaleFactor);
        for (int i = 0; i < 4; i++) {
            Rect r = row.next(180.0f * uiScaleFactor, 44.0f * uiScaleFactor);
            tabs_[i] = Button{r.x, r.y, r.w, r.h};
        }
        contentArea_ = windowRect; // what dockTop left behind, for pages to use
    }

    const Rect& contentArea() const { return contentArea_; }

    Page update(const InputState& input, Page current) {
        for (int i = 0; i < 4; i++) {
            if (tabs_[i].update(input)) return (Page)i;
        }
        return current;
    }

    void draw(PrimitiveBatch& batch, TextRenderer& text, Page current) const {
        const char* labels[4] = {"Text", "Shapes & Curves", "Widgets", "Animation"};
        for (int i = 0; i < 4; i++) {
            float gray = ((Page)i == current) ? 0.8f : 0.4f;
            batch.pushRoundedRect(tabs_[i].x, tabs_[i].y, tabs_[i].w, tabs_[i].h, 6.0f, gray);
            float textW = text.textWidth(labels[i], 18.0f);
            text.drawText(labels[i], tabs_[i].x + (tabs_[i].w - textW) * 0.5f,
                          tabs_[i].y + tabs_[i].h * 0.5f + 6.0f, 18.0f, 0.05f);
        }
    }

 private:
    std::array<Button, 4> tabs_;
    Rect contentArea_{0, 0, 0, 0};
};
```

- [ ] **Step 8: Retrofit `src/app/page_text.cpp` to column-chain the text lines**

```cpp
#include "page_text.h"
#include "../ui/layout.h"

void TextPage::draw(PrimitiveBatch& batch, TextRenderer& text, Rect area, float uiScaleFactor) {
    ColumnCursor col(area.x + 40.0f * uiScaleFactor, area.y + 40.0f * uiScaleFactor,
                     20.0f * uiScaleFactor);
    struct Line { const char* text; float sizePx; bool bold; };
    Line lines[] = {
        {"The quick brown fox — 12pt", 12.0f, false},
        {"The quick brown fox — 20pt", 20.0f, false},
        {"The quick brown fox — 32pt", 32.0f, false},
        {"The quick brown fox — 56pt", 56.0f, false},
        {"Bold at 32pt", 32.0f, true},
    };
    for (auto& line : lines) {
        float scaledSize = line.sizePx * uiScaleFactor;
        Rect r = col.next(400.0f * uiScaleFactor, scaledSize * 1.4f);
        text.drawText(line.text, r.x, r.y + scaledSize, scaledSize, 0.9f, line.bold);
    }
    (void)batch;
}
```

Update `src/app/page_text.h`'s `draw` signature to `void draw(PrimitiveBatch& batch, TextRenderer& text, Rect area, float uiScaleFactor);` (add `#include "../ui/layout.h"`).

- [ ] **Step 9: Retrofit `src/app/page_shapes.cpp` to column/row-chain the shapes**

```cpp
#include "page_shapes.h"
#include "../ui/layout.h"

void ShapesPage::draw(PrimitiveBatch& batch, Rect area, float uiScaleFactor) {
    float pad = 40.0f * uiScaleFactor;
    ColumnCursor col(area.x + pad, area.y + pad, 20.0f * uiScaleFactor);

    Rect row1 = col.next(area.w - pad * 2, 100.0f * uiScaleFactor);
    RowCursor shapesRow(row1.x, row1.y, 20.0f * uiScaleFactor);
    Rect rectR = shapesRow.next(160.0f * uiScaleFactor, row1.h);
    batch.pushRect(rectR.x, rectR.y, rectR.w, rectR.h, 0.85f);
    Rect roundedR = shapesRow.next(160.0f * uiScaleFactor, row1.h);
    batch.pushRoundedRect(roundedR.x, roundedR.y, roundedR.w, roundedR.h, 20.0f * uiScaleFactor, 0.85f);
    Rect lineR = shapesRow.next(160.0f * uiScaleFactor, row1.h);
    batch.pushLine(lineR.x, lineR.y, lineR.x + lineR.w, lineR.y + lineR.h, 4.0f * uiScaleFactor, 0.85f);
    Rect bezierR = shapesRow.next(200.0f * uiScaleFactor, row1.h);
    batch.pushBezier(bezierR.x, bezierR.y + bezierR.h,
                     bezierR.x + bezierR.w * 0.2f, bezierR.y,
                     bezierR.x + bezierR.w * 0.8f, bezierR.y,
                     bezierR.x + bezierR.w, bezierR.y + bezierR.h,
                     4.0f * uiScaleFactor, 0.85f);

    Rect row2 = col.next(area.w - pad * 2, 80.0f * uiScaleFactor);
    RowCursor gradientRow(row2.x, row2.y, 0.0f);
    for (int i = 0; i < 8; i++) {
        float gray = (float)i / 7.0f;
        Rect g = gradientRow.next((row2.w) / 8.0f, row2.h);
        batch.pushRect(g.x, g.y, g.w * 0.9f, g.h, gray);
    }
}
```

Update `src/app/page_shapes.h`'s `draw` signature to `void draw(PrimitiveBatch& batch, Rect area, float uiScaleFactor);` (add `#include "../ui/layout.h"`).

- [ ] **Step 10: Retrofit `src/app/page_widgets.h`/`.cpp` to column-chain the widgets**

In `page_widgets.h`, remove the hardcoded `Button demoButton_{60,140,200,50};` -style member initializers (keep the members, drop their inline rects) and add:

```cpp
void updateLayout(Rect area, float uiScaleFactor);
```

In `page_widgets.cpp`, add:

```cpp
void WidgetsPage::updateLayout(Rect area, float uiScaleFactor) {
    ColumnCursor col(area.x + 60.0f * uiScaleFactor, area.y + 60.0f * uiScaleFactor,
                     20.0f * uiScaleFactor);
    Rect r = col.next(200.0f * uiScaleFactor, 50.0f * uiScaleFactor);
    demoButton_ = Button{r.x, r.y, r.w, r.h};
    r = col.next(60.0f * uiScaleFactor, 32.0f * uiScaleFactor);
    demoToggle_ = Toggle{r.x, r.y, r.w, r.h, demoToggle_.on};
    r = col.next(300.0f * uiScaleFactor, 20.0f * uiScaleFactor);
    demoSlider_.x = r.x; demoSlider_.y = r.y; demoSlider_.w = r.w; demoSlider_.h = r.h;
    r = col.next(260.0f * uiScaleFactor, 30.0f * uiScaleFactor * demoList_.itemCount);
    demoList_.x = r.x; demoList_.y = r.y; demoList_.w = r.w;
    demoList_.rowHeight = 30.0f * uiScaleFactor;
}
```

Call `widgetsPage.updateLayout(area, uiScaleFactor);` once per frame from `main.cpp`, right before `widgetsPage.update(window.input())`.

- [ ] **Step 11: Retrofit `src/app/page_animation.h`/`.cpp` to dock/center the animated square's travel range**

```cpp
void AnimationPage::updateLayout(Rect area, float uiScaleFactor) {
    Rect r = centerIn(area, 200.0f * uiScaleFactor, 50.0f * uiScaleFactor);
    replayButton_ = Button{r.x, area.y + 60.0f * uiScaleFactor, r.w, r.h};
    leftX_ = area.x + 60.0f * uiScaleFactor;
    rightX_ = area.x + area.w - 60.0f * uiScaleFactor - 120.0f * uiScaleFactor;
    squareY_ = area.y + 240.0f * uiScaleFactor;
    squareSize_ = 120.0f * uiScaleFactor;
    // Re-target in-flight animations to the rescaled endpoints so a resize
    // mid-animation doesn't leave the square heading for a stale coordinate.
    if (fade_.isAnimating() || moveX_.isAnimating()) {
        moveX_.set(moveX_.value() > (leftX_ + rightX_) * 0.5f ? rightX_ : leftX_, 0.01f);
    }
}
```

Add `float leftX_, rightX_, squareY_, squareSize_;` members to `AnimationPage` (declared in `page_animation.h`), initialize `moveX_` to `leftX_` the first time `updateLayout` runs (guard with a `bool laidOut_ = false;` member), and change `draw()`'s final line to `batch.pushRoundedRect(moveX_.value(), squareY_, squareSize_, squareSize_, 16.0f, 0.9f, fade_.value());`. Update `update()`'s replay logic to target `leftX_`/`rightX_` instead of the old literals `60.0f`/`700.0f`.

- [ ] **Step 12: Wire the per-frame layout pass into `main.cpp`**

Replace the body of the render loop's page-switch section so layout runs before update/draw every frame:

```cpp
            Rect windowRect{0, 0, (float)frame.extent.width, (float)frame.extent.height};
            UiScale uiScale{661.0f, 0.5f};
            float uiScaleFactor = uiScale.factor(windowRect.h);

            nav.updateLayout(windowRect, uiScaleFactor);
            currentPage = nav.update(window.input(), currentPage);

            batch.reset();
            nav.draw(batch, text, currentPage);
            switch (currentPage) {
                case Page::Text:
                    textPage.draw(batch, text, nav.contentArea(), uiScaleFactor);
                    break;
                case Page::Shapes:
                    shapesPage.draw(batch, nav.contentArea(), uiScaleFactor);
                    break;
                case Page::Widgets:
                    widgetsPage.updateLayout(nav.contentArea(), uiScaleFactor);
                    widgetsPage.update(window.input());
                    widgetsPage.draw(batch, text);
                    break;
                case Page::Animation: {
                    static auto lastTick = std::chrono::steady_clock::now();
                    auto now = std::chrono::steady_clock::now();
                    float dt = std::chrono::duration<float>(now - lastTick).count();
                    lastTick = now;
                    animationPage.updateLayout(nav.contentArea(), uiScaleFactor);
                    animationPage.update(dt, window.input());
                    animationPage.draw(batch, text);
                    break;
                }
            }
```

(Note `currentPage = nav.update(...)` moved above the `switch` and out of its old standalone line earlier in the loop — remove the now-duplicate old call if it's still there from Task 9's version.)

- [ ] **Step 13: Build and manually verify across resolutions**

```bash
cmake --build build_debug --target windows_ui_demo
./build_debug/windows_ui_demo.exe
```

Resize the window through several sizes (small ~640x480, default ~1280x800, large ~2560x1440) on every page. Expected: nav tabs stay evenly spaced and never overlap; text lines on the Text page stay stacked with consistent gaps; the Shapes & Curves row stays inside the content area at any width; Widgets page controls stay stacked with proportional gaps and never overflow the window; the Animation page's square travels between positions that stay inside the visible area at any window size, matching the exact resize-robustness `windows_matrix_player`'s `recalcLayout()` was built to guarantee.

- [ ] **Step 14: Commit and push**

```bash
git_wrapper commit "Add proportional-scale + edge-dock + gap-chain layout; retrofit all four pages"
git_wrapper push
```

---

## Self-Review

**Spec coverage:**
- Submodule scoped to `vulkan_font_engine` only, no audio_engine anywhere — Task 1 + Global Constraints. ✓
- Grayscale-by-construction (R==G==B in every fragment shader, R8 offscreen textures) — Task 3's clear color, Task 4's `shape.frag`; no offscreen/procedural textures beyond the swapchain and the MTSDF atlas exist in this plan, so the R8 rule has nothing else to apply to (noted, not a gap: the spec's "offscreen/procedural textures" caveat anticipated masks/gradients that this scope doesn't end up needing). ✓
- Four showcase pages (Text, Shapes & Curves, Widgets, Animation) behind a top nav — Tasks 9 + 10. ✓
- Text via MTSDF `vulkan_font_engine` — Task 6. ✓
- Animation implemented at app level, not renderer — Task 5 (`anim/`), consumed by Task 10. ✓
- CMake + Ninja + MSVC build matching `windows_matrix_player`'s pattern — Task 1. ✓
- Fail-fast error handling, Debug-only validation layers — Task 3 (`fatal()`, `#ifdef _DEBUG`). ✓
- No automated test suite for rendering; real unit tests for pure logic (input edges, batcher math, easing, widget hit-testing/value-mapping) — Tasks 2, 4, 5, 7, 8. ✓
- `git_wrapper` for every commit/push — every task's final step. ✓
- Resolution-robust layout (proportional scale + edge-dock + gap-chain, matching `windows_matrix_player`'s validated `recalcLayout()`/`ResponsiveTextScale` technique, no hardcoded absolute positions left in any page) — Task 11. ✓

**Placeholder scan:** no TBD/TODO markers; every code step has complete, concrete code.

**Type consistency:** `FrameContext{cmd, renderPass, extent}` (Task 3) is consumed identically in Tasks 4, 6, 9, 10. `PrimitiveBatch`/`Vertex` (Task 4) match across `ShapePipeline`, all four pages, and both widget files. `TextRenderer::drawText`/`textWidth` signatures (Task 6) match every call site in Tasks 7–10, then gain `Rect area, float uiScaleFactor` parameters in Task 11's retrofit (`TextPage::draw`, `ShapesPage::draw`) — every call site of those two functions is updated in the same task's Step 12, so no stale signature is left uncalled. `Rect`/`UiScale`/`RowCursor`/`ColumnCursor`/`dockTop`/`dockBottom`/`dockLeft`/`dockRight`/`centerIn` (Task 11) are used with matching names and parameter order across `nav.h` and all four page files.

## Execution Handoff

Plan complete and saved to `docs/superpowers/plans/2026-07-11-vulkan-bw-ui-demo.md`. Two execution options:

1. **Subagent-Driven (recommended)** — I dispatch a fresh subagent per task, review between tasks, fast iteration.
2. **Inline Execution** — Execute tasks in this session using executing-plans, batch execution with checkpoints.

Which approach?
