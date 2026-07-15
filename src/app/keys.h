#pragma once
// Virtual-key codes for the keys the app reads via FrameInput::keysWentDown.
// vk_canvas's input.hh forwards raw platform key codes (Win32 VK_* today);
// numeric literals here keep page code free of <windows.h>.
namespace keys {
constexpr int Back  = 0x08;  // VK_BACK
constexpr int Left  = 0x25;  // VK_LEFT
constexpr int Up    = 0x26;  // VK_UP
constexpr int Right = 0x27;  // VK_RIGHT
constexpr int Down  = 0x28;  // VK_DOWN
}  // namespace keys
