#include "../fatal.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

void fatal(const char* msg) {
    MessageBoxA(nullptr, msg, "windows_ui_demo — fatal error", MB_OK | MB_ICONERROR);
    ExitProcess(1);
}
