#include <algorithm>
#include <thread>
#include "../../include/j2me_core.h"
#include <cstring>
#include <cstdio>

#if defined(_WIN32) || defined(_WIN64)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#endif

extern "C" {

J2ME_API bool j2me_core_platform_pick_file(char* out_path, size_t max_len) {
    if (!out_path || max_len == 0) return false;
    out_path[0] = '\0';

#if defined(_WIN32) || defined(_WIN64)
    wchar_t szFile[32768] = {0};
    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(OPENFILENAMEW);
    ofn.hwndOwner = GetForegroundWindow();
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
    ofn.lpstrFilter = L"J2ME Game Files (*.jar;*.jad)\0*.jar;*.jad\0JAR Files (*.jar)\0*.jar\0JAD Files (*.jad)\0*.jad\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.lpstrTitle = L"Select J2ME Game (.jar / .jad)";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;

    if (GetOpenFileNameW(&ofn) == TRUE) {
        int ret = WideCharToMultiByte(CP_UTF8, 0, szFile, -1, out_path, static_cast<int>(max_len), NULL, NULL);
        return ret > 0;
    }
    return false;
#else
    // Android / iOS pick files through the Flutter platform channel
    return false;
#endif
}

#if defined(_WIN32) || defined(_WIN64)
static HWND getTopLevelFlutterWindow() {
    HWND hwnd = GetForegroundWindow();
    DWORD currentProcessId = GetCurrentProcessId();
    DWORD windowProcessId = 0;
    if (hwnd) {
        GetWindowThreadProcessId(hwnd, &windowProcessId);
        if (windowProcessId == currentProcessId) {
            return hwnd;
        }
    }
    HWND found = NULL;
    EnumWindows([](HWND h, LPARAM lp) -> BOOL {
        DWORD pid = 0;
        GetWindowThreadProcessId(h, &pid);
        if (pid == GetCurrentProcessId() && IsWindowVisible(h)) {
            if (GetParent(h) == NULL) {
                *reinterpret_cast<HWND*>(lp) = h;
                return FALSE;
            }
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&found));
    return found;
}
#endif

J2ME_API bool j2me_core_platform_set_window_size(int client_width, int client_height) {
#if defined(_WIN32) || defined(_WIN64)
    if (client_width <= 0 || client_height <= 0) return false;
    HWND hwnd = getTopLevelFlutterWindow();
    if (!hwnd) return false;

    // Callers pass logical (DPI-independent) pixels, as Flutter does; convert to physical pixels
    UINT dpi = 96;
    using GetDpiForWindowFn = UINT(WINAPI*)(HWND);
    static auto pGetDpiForWindow = reinterpret_cast<GetDpiForWindowFn>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"));
    if (pGetDpiForWindow) {
        UINT d = pGetDpiForWindow(hwnd);
        if (d > 0) dpi = d;
    }
    client_width = MulDiv(client_width, static_cast<int>(dpi), 96);
    client_height = MulDiv(client_height, static_cast<int>(dpi), 96);

    RECT clientRect = {0, 0, client_width, client_height};
    DWORD style = static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_STYLE));
    DWORD exStyle = static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_EXSTYLE));
    AdjustWindowRectEx(&clientRect, style, FALSE, exStyle);

    int totalW = clientRect.right - clientRect.left;
    int totalH = clientRect.bottom - clientRect.top;

    RECT curRect;
    GetWindowRect(hwnd, &curRect);

    HMONITOR hMon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof(mi) };
    if (GetMonitorInfoW(hMon, &mi)) {
        int maxW = mi.rcWork.right - mi.rcWork.left - 24;
        int maxH = mi.rcWork.bottom - mi.rcWork.top - 48;
        if (totalW > maxW) totalW = maxW;
        if (totalH > maxH) totalH = maxH;
    }

    // Keep the whole window inside the work area without touching bottom taskbar
    int left = curRect.left, top = curRect.top;
    if (GetMonitorInfoW(hMon, &mi)) {
        if (top + totalH > mi.rcWork.bottom) {
            top = std::max<int>(mi.rcWork.top + 10, mi.rcWork.bottom - totalH - 10);
        }
        if (left + totalW > mi.rcWork.right) {
            left = std::max<int>(mi.rcWork.left + 10, mi.rcWork.right - totalW - 10);
        }
        if (top < mi.rcWork.top + 10) top = mi.rcWork.top + 10;
        if (left < mi.rcWork.left + 10) left = mi.rcWork.left + 10;
    }
    std::thread([hwnd, left, top, totalW, totalH]() {
        SetWindowPos(hwnd, NULL, left, top, totalW, totalH, SWP_NOZORDER | SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS);
    }).detach();
    return true;
#else
    (void)client_width;
    (void)client_height;
    return false;
#endif
}

J2ME_API bool j2me_core_platform_get_window_size(int* out_width, int* out_height) {
#if defined(_WIN32) || defined(_WIN64)
    HWND hwnd = getTopLevelFlutterWindow();
    if (!hwnd) return false;
    RECT r;
    if (GetClientRect(hwnd, &r)) {
        if (out_width) *out_width = r.right - r.left;
        if (out_height) *out_height = r.bottom - r.top;
        return true;
    }
    return false;
#else
    (void)out_width;
    (void)out_height;
    return false;
#endif
}

} // extern "C"
