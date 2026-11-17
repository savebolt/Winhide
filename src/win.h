#pragma once

#include <windows.h>

#include <string>
#include <vector>

// A single top-level window that can be picked or matched.
struct WindowInfo {
    HWND hwnd = nullptr;
    DWORD pid = 0;
    std::wstring process;  // base exe name, e.g. "Discord.exe"
    std::wstring title;
    std::wstring cls;  // window class name
    bool minimized = false;
};

// A saved identity of a window. Matching is always re-resolved each run:
// handles and PIDs are meaningful only for one process lifetime.
struct Target {
    std::wstring process;  // required when set
    std::wstring title;    // empty = any title of the process
    bool contains = false; // title comparison mode
    std::wstring cls;      // optional tiebreaker
};

std::vector<WindowInfo> enumerateWindows();

std::vector<WindowInfo> resolve(const Target& target);

std::vector<WindowInfo> withProcess(const std::vector<WindowInfo>& windows, const std::wstring& process);

std::wstring describeTarget(const Target& target);
std::wstring describeWindow(const WindowInfo& window);

void minimizeWindow(HWND hwnd);
void restoreWindow(HWND hwnd);
