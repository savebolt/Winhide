#include "win.h"

#include <algorithm>
#include <map>

#include "util.h"

namespace {

std::map<DWORD, std::wstring>& processCache() {
    static std::map<DWORD, std::wstring> cache;
    return cache;
}

std::wstring queryProcessName(DWORD pid) {
    auto& cache = processCache();
    auto it = cache.find(pid);
    if (it != cache.end()) return it->second;

    std::wstring name;
    HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (proc != nullptr) {
        wchar_t buf[32768];
        DWORD size = (DWORD)(sizeof(buf) / sizeof(buf[0]));
        if (QueryFullProcessImageNameW(proc, 0, buf, &size)) name = basenameOf(buf);
        CloseHandle(proc);
    }
    cache[pid] = name;
    return name;
}

std::wstring windowTitle(HWND hwnd) {
    int len = GetWindowTextLengthW(hwnd);
    if (len <= 0) return L"";
    std::wstring buf((size_t)len + 1, L'\0');
    int n = GetWindowTextW(hwnd, buf.data(), len + 1);
    buf.resize(n > 0 ? (size_t)n : 0);
    return trim(buf);
}

std::wstring windowClass(HWND hwnd) {
    wchar_t buf[256];
    int n = GetClassNameW(hwnd, buf, 256);
    return std::wstring(buf, n > 0 ? (size_t)n : 0);
}

// Windows living on another virtual desktop (or suspended UWP apps) are
// cloaked by DWM. Resolve the API dynamically so we keep no extra link dep.
bool isCloaked(HWND hwnd) {
    using Pfn = HRESULT(WINAPI*)(HWND, DWORD, void*, DWORD);
    static Pfn fn = [] {
        HMODULE dwm = LoadLibraryW(L"dwmapi.dll");
        if (dwm == nullptr) return (Pfn)nullptr;
        return (Pfn)(void*)GetProcAddress(dwm, "DwmGetWindowAttribute");
    }();
    if (fn == nullptr) return false;
    DWORD cloaked = 0;
    const DWORD DWMWA_CLOAKED = 14;
    if (FAILED(fn(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked)))) return false;
    return cloaked != 0;
}

bool isShellWindow(HWND hwnd, const std::wstring& cls) {
    static const wchar_t* skip[] = {L"Progman", L"Shell_TrayWnd", L"Shell_SecondaryTrayWnd", L"WorkerW"};
    for (auto name : skip)
        if (iequals(cls, name)) return true;
    return hwnd == GetConsoleWindow();
}

BOOL CALLBACK enumProc(HWND hwnd, LPARAM param) {
    auto* rows = reinterpret_cast<std::vector<WindowInfo>*>(param);

    if (!IsWindowVisible(hwnd)) return TRUE;
    if (GetWindowTextLengthW(hwnd) <= 0) return TRUE;
    if (GetWindowLongW(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) return TRUE;
    if (isCloaked(hwnd)) return TRUE;

    std::wstring cls = windowClass(hwnd);
    if (isShellWindow(hwnd, cls)) return TRUE;

    WindowInfo info;
    info.hwnd = hwnd;
    GetWindowThreadProcessId(hwnd, &info.pid);
    info.process = queryProcessName(info.pid);
    if (info.process.empty()) return TRUE;
    info.title = windowTitle(hwnd);
    if (info.title.empty()) return TRUE;
    info.cls = cls;
    info.minimized = IsIconic(hwnd) != FALSE;
    rows->push_back(info);
    return TRUE;
}

struct CompareWindows {
    bool operator()(const WindowInfo& a, const WindowInfo& b) const {
        std::wstring ap = toLower(a.process), bp = toLower(b.process);
        if (ap != bp) return ap < bp;
        std::wstring at = toLower(a.title), bt = toLower(b.title);
        if (at != bt) return at < bt;
        return a.pid < b.pid;
    }
};

}  // namespace

std::vector<WindowInfo> enumerateWindows() {
    std::vector<WindowInfo> rows;
    EnumWindows(enumProc, reinterpret_cast<LPARAM>(&rows));
    std::sort(rows.begin(), rows.end(), CompareWindows{});
    return rows;
}

std::vector<WindowInfo> resolve(const Target& target) {
    std::vector<WindowInfo> matches;
    for (const auto& w : enumerateWindows()) {
        if (!target.process.empty() && !iequals(w.process, target.process)) continue;
        if (!target.title.empty()) {
            bool titleOk = target.contains ? icontains(w.title, target.title) : iequals(w.title, target.title);
            if (!titleOk) continue;
        }
        matches.push_back(w);
    }

    // Class name is only a tiebreaker, never a hard requirement: some apps
    // reuse the same class for every window.
    if (!target.cls.empty() && matches.size() > 1) {
        std::vector<WindowInfo> sameClass;
        for (const auto& w : matches)
            if (iequals(w.cls, target.cls)) sameClass.push_back(w);
        if (!sameClass.empty()) matches = sameClass;
    }
    return matches;
}

std::vector<WindowInfo> withProcess(const std::vector<WindowInfo>& windows, const std::wstring& process) {
    std::vector<WindowInfo> out;
    for (const auto& w : windows)
        if (iequals(w.process, process)) out.push_back(w);
    return out;
}

std::wstring describeTarget(const Target& target) {
    std::wstring s = target.process.empty() ? L"<any process>" : target.process;
    s += target.title.empty() ? L" | <any title>" : L" | \"" + target.title + L"\"";
    s += target.contains ? L" (contains)" : L" (exact)";
    return s;
}

std::wstring describeWindow(const WindowInfo& window) {
    return window.process + L" | \"" + window.title + L"\"";
}

void minimizeWindow(HWND hwnd) {
    ShowWindow(hwnd, SW_MINIMIZE);

    // Alt+Tab is ordered by z-order (most recent first). Minimizing alone can
    // leave the window near the top, so push it to the bottom: it then shows up
    // last in Alt+Tab instead of getting in the way.
    SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
}

void restoreAndFocus(HWND hwnd) {
    if (IsIconic(hwnd)) {
        ShowWindow(hwnd, SW_RESTORE);
    } else {
        ShowWindow(hwnd, SW_SHOW);
    }

    // Raise in z-order first. Alt+Tab follows z-order, and SetForegroundWindow
    // can be refused by the foreground lock, which would otherwise leave the
    // window at the bottom of the list even though it is visible again.
    SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

    HWND foreground = GetForegroundWindow();
    DWORD foregroundThread = foreground != nullptr ? GetWindowThreadProcessId(foreground, nullptr) : 0;
    DWORD thisThread = GetCurrentThreadId();
    bool attached = false;
    if (foregroundThread != 0 && foregroundThread != thisThread)
        attached = AttachThreadInput(thisThread, foregroundThread, TRUE) != FALSE;

    BringWindowToTop(hwnd);
    SetForegroundWindow(hwnd);

    if (attached) AttachThreadInput(thisThread, foregroundThread, FALSE);

    // Last resort when the foreground lock still refuses the switch.
    if (GetForegroundWindow() != hwnd) SwitchToThisWindow(hwnd, TRUE);
}
