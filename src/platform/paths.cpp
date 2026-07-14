#include "paths.h"
#include <windows.h>
#include <vector>

std::string exeDirectory() {
    std::vector<wchar_t> buf(MAX_PATH);
    for (;;) {
        DWORD len = GetModuleFileNameW(nullptr, buf.data(), (DWORD)buf.size());
        if (len == 0) return std::string();
        if (len < buf.size() - 1) break;  // fits, not truncated
        buf.resize(buf.size() * 2);
    }

    std::wstring full(buf.data());
    size_t slash = full.find_last_of(L"\\/");
    std::wstring dirW = (slash == std::wstring::npos) ? L"" : full.substr(0, slash);

    if (dirW.empty()) return std::string();
    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, dirW.c_str(), (int)dirW.size(),
                                       nullptr, 0, nullptr, nullptr);
    std::string dirUtf8(utf8Len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, dirW.c_str(), (int)dirW.size(),
                         dirUtf8.data(), utf8Len, nullptr, nullptr);
    return dirUtf8;
}
