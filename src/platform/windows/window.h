#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "../input_state.h"

// Client-area cursor shape, applied via WM_SETCURSOR. The frame loop sets
// the desired shape once per frame (hand over interactive widgets, arrow
// otherwise); non-client cursors (resize arrows on the frame) stay with
// DefWindowProc.
enum class CursorShape { Arrow, Hand };

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
    // Applies immediately if the mouse is over the client area, so hover
    // feedback doesn't wait for the next WM_SETCURSOR (which only fires on
    // mouse movement).
    void setDesiredCursor(CursorShape shape);

 private:
    static LRESULT CALLBACK wndProc(HWND, UINT, WPARAM, LPARAM);
    HWND hwnd_ = nullptr;
    InputState input_;
    bool resizedPending_ = false;
    int pendingW_ = 0, pendingH_ = 0;
    CursorShape desiredCursor_ = CursorShape::Arrow;
};
