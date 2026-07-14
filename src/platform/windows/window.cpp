#include "window.h"
#include <windowsx.h>

static const wchar_t* kClassName = L"WindowsUIDemoWindow";

bool Window::create(HINSTANCE hInst, int width, int height, const wchar_t* title) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = wndProc;
    wc.hInstance = hInst;
    // A NULL class cursor tells Windows to leave the cursor alone over the
    // client area, so whatever shape it had on entry (app-start hourglass,
    // the frame's resize arrows) would stick forever.
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = kClassName;
    RegisterClassExW(&wc);

    // width/height are the desired CLIENT size (the swapchain is created at
    // exactly this size before the first WM_SIZE) — grow the rect by the
    // frame/title so the client area comes out exact, then center the outer
    // rect on the monitor work area.
    RECT wr{0, 0, width, height};
    AdjustWindowRectEx(&wr, WS_OVERLAPPEDWINDOW, FALSE, 0);
    int winW = wr.right - wr.left;
    int winH = wr.bottom - wr.top;
    RECT wa{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    int px = wa.left + ((wa.right - wa.left) - winW) / 2;
    int py = wa.top + ((wa.bottom - wa.top) - winH) / 2;
    if (px < wa.left) px = wa.left;
    if (py < wa.top) py = wa.top;

    hwnd_ = CreateWindowExW(0, kClassName, title, WS_OVERLAPPEDWINDOW,
        px, py, winW, winH,
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

void Window::setDesiredCursor(CursorShape shape) {
    if (shape == desiredCursor_) return;
    desiredCursor_ = shape;
    // WM_SETCURSOR only fires on mouse movement; apply the change now if the
    // cursor is currently over our client area so hover feedback is instant.
    POINT pt;
    if (GetCursorPos(&pt)) {
        RECT rc;
        GetClientRect(hwnd_, &rc);
        POINT client = pt;
        ScreenToClient(hwnd_, &client);
        if (WindowFromPoint(pt) == hwnd_ && PtInRect(&rc, client)) {
            SetCursor(LoadCursor(nullptr, shape == CursorShape::Hand ? IDC_HAND : IDC_ARROW));
        }
    }
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
        case WM_SETCURSOR:
            // Own the client-area cursor; non-client hits (frame edges) fall
            // through to DefWindowProc so the resize arrows still appear.
            if (self && LOWORD(lParam) == HTCLIENT) {
                SetCursor(LoadCursor(nullptr,
                    self->desiredCursor_ == CursorShape::Hand ? IDC_HAND : IDC_ARROW));
                return TRUE;
            }
            break;
        case WM_MOUSEMOVE:
            if (self) {
                self->input_.mouseX = (float)GET_X_LPARAM(lParam);
                self->input_.mouseY = (float)GET_Y_LPARAM(lParam);
            }
            return 0;
        case WM_MOUSEWHEEL:
            if (self) {
                self->input_.wheelDelta +=
                    (float)GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA;
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
        case WM_CHAR:
            if (self) {
                // wParam is a UTF-16 code unit (window class is the wide
                // variant). Only append printable ASCII -- digits/operators/
                // letters, which is everything the math editor understands;
                // control chars (backspace=0x08, enter=0x0D, escape=0x1B,
                // tab=0x09, and DEL=0x7F) are handled via WM_KEYDOWN's
                // virtual-key codes below instead, so they must NOT also
                // land in typedChars (that would double-handle backspace).
                if (wParam >= 0x20 && wParam < 0x7F) {
                    self->input_.typedChars.push_back((char)wParam);
                }
            }
            return 0;
        case WM_KEYDOWN:
            if (self) {
                switch (wParam) {
                    case VK_LEFT:  self->input_.keyLeftPressed = true;  break;
                    case VK_RIGHT: self->input_.keyRightPressed = true; break;
                    case VK_UP:    self->input_.keyUpPressed = true;    break;
                    case VK_DOWN:  self->input_.keyDownPressed = true;  break;
                    case VK_BACK:  self->input_.keyBackspacePressed = true; break;
                }
            }
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
