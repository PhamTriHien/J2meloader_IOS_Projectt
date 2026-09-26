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
#elif defined(__APPLE__)
    FILE* fp = popen("osascript -e 'POSIX path of (choose file of type {\"jar\", \"jad\"} with prompt \"Select J2ME Game\")' 2>/dev/null", "r");
    if (fp) {
        if (fgets(out_path, static_cast<int>(max_len), fp)) {
            size_t len = strlen(out_path);
            while (len > 0 && (out_path[len - 1] == '\n' || out_path[len - 1] == '\r')) {
                out_path[--len] = '\0';
            }
            pclose(fp);
            return len > 0;
        }
        pclose(fp);
    }
    return false;
#elif defined(__linux__) && !defined(__ANDROID__)
    FILE* fp = popen("zenity --file-selection --title=\"Select J2ME Game\" --file-filter=\"J2ME (*.jar *.jad) | *.jar *.jad\" 2>/dev/null", "r");
    if (!fp) {
        fp = popen("kdialog --getopenfilename . \"*.jar *.jad\" 2>/dev/null", "r");
    }
    if (fp) {
        if (fgets(out_path, static_cast<int>(max_len), fp)) {
            size_t len = strlen(out_path);
            while (len > 0 && (out_path[len - 1] == '\n' || out_path[len - 1] == '\r')) {
                out_path[--len] = '\0';
            }
            pclose(fp);
            return len > 0;
        }
        pclose(fp);
    }
    return false;
#else
    return false;
#endif
}

} // extern "C"
