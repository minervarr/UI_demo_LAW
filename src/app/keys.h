#pragma once
#include "keys.hh"

// The keys the pages read via FrameInput::keysWentDown.
//
// These used to be raw numeric literals with a "Win32 VK_* today" comment,
// written when this app owned a wnd_proc and the codes arriving from it were
// the platform's own. They are app_shell's portable key:: space now — the
// hosts translate into it before the app sees anything, so a page never learns
// which platform it is running on. The VALUES are unchanged (key:: keeps the
// VK_* numbering), so this is a renaming, not a remapping.
namespace keys {
constexpr int Back  = key::Backspace;
constexpr int Left  = key::Left;
constexpr int Up    = key::Up;
constexpr int Right = key::Right;
constexpr int Down  = key::Down;

// Z and Y are not in vk_canvas's key:: set — it names only the keys the engine
// itself has needed so far, and the undo/redo shortcuts are this app's. Same
// numbering (the platform-independent space follows the VK_* values), so
// adding them here is a naming and not a new mapping.
constexpr int Z = 0x5A;
constexpr int Y = 0x59;
}  // namespace keys
