#include <windows.h>

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "config.h"
#include "util.h"
#include "win.h"

namespace {

constexpr int kExitOk = 0;
constexpr int kExitNotFound = 1;
constexpr int kExitUsage = 2;

constexpr size_t kProcessWidth = 22;
constexpr size_t kTitleWidth = 62;

struct Args {
    bool help = false;
    bool list = false;
    bool set = false;
    bool show = false;
    bool hide = false;
    bool status = false;
    bool clear = false;
    bool contains = false;
    bool hasProcess = false;
    bool hasTitle = false;
    std::wstring process;
    std::wstring title;
    bool hasIndex = false;
    long index = 0;
};

void printHelp() {
    println(bold(L"winhide") + L" - toggle a window between minimized and shown");
    println();
    println(L"Usage:");
    println(L"  winhide                          toggle the saved default target");
    println(L"                                   (first run: pick a window instead)");
    println(L"  winhide --list                   list selectable windows");
    println(L"  winhide --set                    pick a window interactively, save it");
    println(L"  winhide --set <n>                save entry <n> from --list");
    println(L"  winhide --set --process <exe> [--title <text>] [--contains]");
    println(L"                                   save a target without the picker");
    println(L"  winhide --show                   restore + focus the default target");
    println(L"  winhide --hide                   minimize the default target");
    println(L"  winhide --status                 show resolved windows and their state");
    println(L"  winhide --clear                  forget the saved target");
    println(L"  winhide --process <exe> [--title <text>] [--show|--hide]");
    println(L"                                   act on a target without saving it");
    println();
    println(L"Matching:");
    println(L"  A target is a window, not an exe: process name + title. That keeps");
    println(L"  popped-out windows of the same app apart (Discord stream vs main).");
    println(L"  --title is an exact, case-insensitive match unless --contains is used.");
    println(L"  Targets are re-resolved on every run, so handles never go stale.");
    println();
    println(L"Examples:");
    println(dim(L"  winhide --set --process Discord.exe --title \"Stream - someuser\""));
    println(dim(L"  winhide --set --process Discord.exe --title \"Discord\""));
    println(dim(L"  winhide --show"));
    println();
    println(L"Config:");
    println(L"  winhide.cfg is written next to the exe (override with WINHIDE_CONFIG).");
    println();
    println(L"Exit codes: 0 ok, 1 target/config missing, 2 bad usage.");
}

bool isAllDigits(const std::wstring& s) {
    if (s.empty()) return false;
    for (wchar_t c : s)
        if (c < L'0' || c > L'9') return false;
    return true;
}

bool parseIndex(const std::wstring& text, long& value) {
    std::wstring s = trim(text);
    if (!isAllDigits(s) || s.size() > 9) return false;
    value = wcstol(s.c_str(), nullptr, 10);
    return true;
}

bool parseArgs(int argc, wchar_t** argv, Args& args, std::wstring& error) {
    auto needValue = [&](int i, const std::wstring& flag, std::wstring& out) {
        if (i + 1 >= argc) {
            error = L"missing value for " + flag;
            return false;
        }
        out = argv[i + 1];
        return true;
    };

    for (int i = 1; i < argc; i++) {
        std::wstring s = argv[i];
        if (s == L"--help" || s == L"-h" || s == L"/?" || s == L"-?") {
            args.help = true;
        } else if (s == L"--list" || s == L"-l") {
            args.list = true;
        } else if (s == L"--set" || s == L"-s") {
            args.set = true;
            if (i + 1 < argc) {
                std::wstring next = argv[i + 1];
                long value = 0;
                if (!next.empty() && next[0] != L'-' && parseIndex(next, value)) {
                    args.hasIndex = true;
                    args.index = value;
                    i++;
                }
            }
        } else if (s == L"--show") {
            args.show = true;
        } else if (s == L"--hide") {
            args.hide = true;
        } else if (s == L"--status" || s == L"--state") {
            args.status = true;
        } else if (s == L"--clear" || s == L"--reset") {
            args.clear = true;
        } else if (s == L"--contains" || s == L"--fuzzy") {
            args.contains = true;
        } else if (s == L"--exact") {
            args.contains = false;
        } else if (s == L"--process" || s == L"-p") {
            std::wstring value;
            if (!needValue(i, s, value)) return false;
            args.hasProcess = true;
            args.process = value;
            i++;
        } else if (s == L"--title" || s == L"-t") {
            std::wstring value;
            if (!needValue(i, s, value)) return false;
            args.hasTitle = true;
            args.title = value;
            i++;
        } else if (s.rfind(L"--process=", 0) == 0) {
            args.hasProcess = true;
            args.process = s.substr(10);
        } else if (s.rfind(L"--title=", 0) == 0) {
            args.hasTitle = true;
            args.title = s.substr(8);
        } else if (!s.empty() && s[0] == L'-') {
            error = L"unknown option: " + s;
            return false;
        } else if (args.set && isAllDigits(s)) {
            args.hasIndex = true;
            args.index = wcstol(s.c_str(), nullptr, 10);
        } else {
            error = L"unexpected argument: " + s;
            return false;
        }
    }
    return true;
}

bool readLine(std::wstring& out) {
    std::string raw;
    if (!std::getline(std::cin, raw)) return false;
    if (!raw.empty() && raw.back() == '\r') raw.pop_back();
    out = fromUtf8(raw);
    return true;
}

bool promptYesNo(const std::wstring& question, bool defaultYes) {
    print(L"  " + question + (defaultYes ? L" [Y/n] " : L" [y/N] "));
    std::wstring line;
    if (!readLine(line)) {
        println();
        return false;
    }
    line = trim(line);
    if (line.empty()) return defaultYes;
    wchar_t c = line[0];
    if (c >= L'A' && c <= L'Z') c = (wchar_t)(c - L'A' + L'a');
    return c == L'y';
}

void printWindowTable(const std::vector<WindowInfo>& rows, bool numbered, bool toErr = false) {
    auto emit = [toErr](const std::wstring& line) {
        if (toErr)
            printlnErr(line);
        else
            println(line);
    };

    if (rows.empty()) {
        emit(L"  No selectable windows found.");
        return;
    }
    std::wstring header = L"  " + padRight(L"#", 4) + padRight(L"PID", 8) +
                          padRight(L"PROCESS", kProcessWidth + 2) + padRight(L"STATE", 7) + L"TITLE";
    emit(dim(header));
    for (size_t i = 0; i < rows.size(); i++) {
        const WindowInfo& w = rows[i];
        std::wstring num = numbered ? std::to_wstring(i + 1) : L"-";
        std::wstring state = w.minimized ? L"min" : L"shown";
        emit(L"  " + padRight(num, 4) + padRight(std::to_wstring(w.pid), 8) +
             padRight(truncate(w.process, kProcessWidth), kProcessWidth + 2) + padRight(state, 7) +
             truncate(w.title, kTitleWidth));
    }
}

bool pickWindow(WindowInfo& chosen) {
    std::vector<WindowInfo> rows = enumerateWindows();
    if (rows.empty()) {
        eprintln(L"winhide: no selectable windows found.");
        return false;
    }

    println();
    printWindowTable(rows, true);
    println(L"  " + dim(L"0") + L"    cancel");
    println();

    while (true) {
        print(L"Select window [0-" + std::to_wstring(rows.size()) + L"]: ");
        std::wstring line;
        if (!readLine(line)) {
            println();
            println(L"Cancelled.");
            return false;
        }
        long value = 0;
        if (!parseIndex(line, value) || value < 0 || value > (long)rows.size()) {
            eprintln(L"  Invalid choice, try again.");
            continue;
        }
        if (value == 0) {
            println(L"Cancelled.");
            return false;
        }
        chosen = rows[(size_t)value - 1];
        return true;
    }
}

bool saveAsDefault(const Target& target) {
    println();
    println(L"  Target: " + bold(describeTarget(target)));
    if (!promptYesNo(L"Make this your default target?", true)) {
        println(L"  Not saved as default.");
        return false;
    }
    std::wstring error;
    if (!saveConfig(target, error)) {
        eprintln(L"  Failed to save: " + error);
        return false;
    }
    println(L"  Saved to " + dim(configPath()));
    return true;
}

void noDefault() {
    eprintln(L"winhide: no default target saved.");
    eprintln(L"Run 'winhide' to pick one, or 'winhide --process <name>' for a one-shot toggle.");
}

int notFound(const Target& target) {
    eprintln(L"Target window not found: " + describeTarget(target));

    if (!target.process.empty()) {
        std::vector<WindowInfo> same = withProcess(enumerateWindows(), target.process);
        if (!same.empty()) {
            eprintln();
            eprintln(L"Windows from " + target.process + L" right now:");
            printWindowTable(same, false, true);
            eprintln(L"Pick one with: winhide --set");
            return kExitNotFound;
        }
    }

    eprintln(L"Run 'winhide --list' to see what is open, or 'winhide --set' to pick a new target.");
    return kExitNotFound;
}

int doToggle(const Target& target) {
    std::vector<WindowInfo> windows = resolve(target);
    if (windows.empty()) return notFound(target);

    bool anyShown = false;
    for (const auto& w : windows)
        if (!w.minimized) anyShown = true;

    for (const auto& w : windows) {
        if (anyShown) {
            minimizeWindow(w.hwnd);
            println(L"  minimized  " + describeWindow(w));
        } else {
            restoreAndFocus(w.hwnd);
            println(L"  shown      " + describeWindow(w));
        }
    }
    return kExitOk;
}

int doAction(const Target& target, bool show) {
    std::vector<WindowInfo> windows = resolve(target);
    if (windows.empty()) return notFound(target);

    for (const auto& w : windows) {
        if (show) {
            restoreAndFocus(w.hwnd);
            println(L"  shown      " + describeWindow(w));
        } else {
            minimizeWindow(w.hwnd);
            println(L"  minimized  " + describeWindow(w));
        }
    }
    return kExitOk;
}

int doStatus(const Target& target) {
    println(L"Default target: " + bold(describeTarget(target)));
    println(L"Config:         " + dim(configPath()));

    std::vector<WindowInfo> windows = resolve(target);
    if (windows.empty()) {
        println(L"Resolved:       0 windows (target is not open right now)");
        println();
        println(dim(L"Run 'winhide --list' to see what is open, or 'winhide --set' to re-target."));
        return kExitNotFound;
    }

    println(L"Resolved:       " + std::to_wstring(windows.size()) + L" window(s)");
    println();
    printWindowTable(windows, false);

    bool anyShown = false;
    for (const auto& w : windows)
        if (!w.minimized) anyShown = true;

    println();
    println(anyShown ? L"State: shown, so a bare run would minimize it."
                     : L"State: minimized, so a bare run would restore it.");
    return kExitOk;
}

int pickerFlow(bool toggleAfter, bool contains) {
    println(bold(L"winhide") + L" " + dim(L"- pick the window to toggle"));
    WindowInfo chosen;
    if (!pickWindow(chosen)) return kExitNotFound;

    Target target;
    target.process = chosen.process;
    target.title = chosen.title;
    target.contains = contains;
    target.cls = chosen.cls;

    bool saved = saveAsDefault(target);

    if (toggleAfter) {
        if (chosen.minimized) {
            restoreAndFocus(chosen.hwnd);
            println(L"  shown      " + describeWindow(chosen));
        } else {
            minimizeWindow(chosen.hwnd);
            println(L"  minimized  " + describeWindow(chosen));
        }
        println();
        if (saved) {
            println(L"Next run: " + bold(L"winhide") + L" toggles it.");
        } else {
            println(dim(L"Nothing saved; next bare run will ask again."));
        }
    }
    return kExitOk;
}

int doSet(const Args& args) {
    if (args.hasIndex) {
        std::vector<WindowInfo> rows = enumerateWindows();
        if (args.index < 1 || args.index > (long)rows.size()) {
            eprintln(L"winhide: index out of range (1-" + std::to_wstring(rows.size()) + L")");
            eprintln(L"Run 'winhide --list' to see the current numbering.");
            return kExitUsage;
        }
        const WindowInfo& w = rows[(size_t)args.index - 1];
        Target target{w.process, w.title, args.contains, w.cls};
        return saveAsDefault(target) ? kExitOk : kExitNotFound;
    }

    if (args.hasProcess || args.hasTitle) {
        Target target{args.process, args.title, args.contains, L""};
        std::vector<WindowInfo> matches = resolve(target);
        if (matches.size() == 1) {
            target.cls = matches[0].cls;
        } else if (matches.empty()) {
            println(dim(L"  Note: no window matches this target right now."));
        } else {
            println(dim(L"  Note: " + std::to_wstring(matches.size()) +
                        L" windows match; all of them will be toggled together."));
        }
        return saveAsDefault(target) ? kExitOk : kExitNotFound;
    }

    return pickerFlow(false, args.contains);
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    initConsole();

    Args args;
    std::wstring error;
    if (!parseArgs(argc, argv, args, error)) {
        eprintln(L"winhide: " + error);
        eprintln(L"Try 'winhide --help' for usage.");
        return kExitUsage;
    }

    if (args.help) {
        printHelp();
        return kExitOk;
    }

    if (args.list) {
        std::vector<WindowInfo> rows = enumerateWindows();
        println(bold(L"Selectable windows") + dim(L" (" + std::to_wstring(rows.size()) + L")"));
        println();
        printWindowTable(rows, true);
        println();
        println(dim(L"Pick one with: winhide --set <number>"));
        return kExitOk;
    }

    if (args.clear) {
        if (!clearConfig(error)) {
            eprintln(L"winhide: " + error);
            return kExitNotFound;
        }
        println(L"Default target cleared.");
        return kExitOk;
    }

    if (args.show && args.hide) {
        eprintln(L"winhide: --show and --hide are mutually exclusive.");
        return kExitUsage;
    }

    bool oneShot = (args.hasProcess || args.hasTitle) && !args.set;

    Target target;
    bool haveDefault = false;
    if (oneShot) {
        target.process = args.process;
        target.title = args.title;
        target.contains = args.contains;
    } else {
        std::wstring configError;
        haveDefault = loadConfig(target, configError);
    }
    bool haveTarget = oneShot || haveDefault;

    if (args.set) return doSet(args);

    if (args.status) {
        if (!haveTarget) {
            noDefault();
            return kExitNotFound;
        }
        return doStatus(target);
    }

    if (args.show || args.hide) {
        if (!haveTarget) {
            noDefault();
            return kExitNotFound;
        }
        return doAction(target, args.show);
    }

    if (oneShot) return doToggle(target);
    if (haveDefault) return doToggle(target);

    return pickerFlow(true, args.contains);
}
