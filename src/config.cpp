#include "config.h"

#include <windows.h>

#include <sstream>

#include "util.h"

namespace {

std::wstring exeDir() {
    wchar_t buf[32768];
    DWORD n = GetModuleFileNameW(nullptr, buf, (DWORD)(sizeof(buf) / sizeof(buf[0])));
    if (n == 0) return L".";
    std::wstring path(buf, n);
    size_t pos = path.find_last_of(L"\\/");
    return pos == std::wstring::npos ? L"." : path.substr(0, pos);
}

std::wstring windowsError(DWORD code) {
    wchar_t* msg = nullptr;
    DWORD n = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                             nullptr, code, 0, (wchar_t*)&msg, 0, nullptr);
    std::wstring out = n > 0 ? trimRight(std::wstring(msg, n)) : L"error " + std::to_wstring(code);
    if (msg != nullptr) LocalFree(msg);
    return out;
}

std::string readFile(const std::wstring& path, bool& ok) {
    ok = false;
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return {};

    std::string data;
    char buf[4096];
    DWORD got = 0;
    while (ReadFile(file, buf, (DWORD)sizeof(buf), &got, nullptr) && got > 0) data.append(buf, got);
    CloseHandle(file);
    ok = true;
    return data;
}

}  // namespace

std::wstring configPath() {
    wchar_t buf[32768];
    DWORD n = GetEnvironmentVariableW(L"WINHIDE_CONFIG", buf, (DWORD)(sizeof(buf) / sizeof(buf[0])));
    if (n > 0 && n < sizeof(buf) / sizeof(buf[0])) return buf;
    return exeDir() + L"\\winhide.cfg";
}

bool loadConfig(Target& out, std::wstring& error) {
    bool ok = false;
    std::string raw = readFile(configPath(), ok);
    if (!ok) {
        error = L"no saved default target yet";
        return false;
    }

    // Strip a UTF-8 BOM if the file was hand-edited by a Windows editor.
    if (raw.size() >= 3 && (unsigned char)raw[0] == 0xEF && (unsigned char)raw[1] == 0xBB &&
        (unsigned char)raw[2] == 0xBF)
        raw.erase(0, 3);

    Target t;
    std::istringstream in(raw);
    std::string line;
    while (std::getline(in, line)) {
        std::wstring wline = trim(fromUtf8(line));
        if (wline.empty() || wline[0] == L'#') continue;
        size_t eq = wline.find(L'=');
        if (eq == std::wstring::npos) continue;
        std::wstring key = toLower(trim(wline.substr(0, eq)));
        std::wstring value = trimRight(wline.substr(eq + 1));
        if (key == L"process")
            t.process = value;
        else if (key == L"title")
            t.title = value;
        else if (key == L"class")
            t.cls = value;
        else if (key == L"match") {
            std::wstring mode = toLower(trim(value));
            t.contains = (mode == L"contains" || mode == L"fuzzy" || mode == L"true" || mode == L"1");
        }
    }

    if (t.process.empty() && t.title.empty()) {
        error = L"config is empty: " + configPath();
        return false;
    }
    out = std::move(t);
    return true;
}

bool saveConfig(const Target& target, std::wstring& error) {
    std::wstring path = configPath();
    std::wstring tmp = path + L".tmp";

    // Newlines would break the line-oriented format.
    auto clean = [](std::wstring v) {
        for (auto& c : v)
            if (c == L'\r' || c == L'\n') c = L' ';
        return trimRight(v);
    };

    std::string body;
    body += "# winhide target\n";
    body += "process=" + toUtf8(clean(target.process)) + "\n";
    body += "title=" + toUtf8(clean(target.title)) + "\n";
    body += "match=" + std::string(target.contains ? "contains" : "exact") + "\n";
    body += "class=" + toUtf8(clean(target.cls)) + "\n";

    {
        HANDLE file = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                                  nullptr);
        if (file == INVALID_HANDLE_VALUE) {
            error = L"cannot write " + tmp + L": " + windowsError(GetLastError());
            return false;
        }
        DWORD written = 0;
        BOOL okWrite = WriteFile(file, body.data(), (DWORD)body.size(), &written, nullptr);
        CloseHandle(file);
        if (!okWrite || written != (DWORD)body.size()) {
            error = L"cannot write " + tmp;
            DeleteFileW(tmp.c_str());
            return false;
        }
    }

    if (!MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        error = L"cannot replace " + path + L": " + windowsError(GetLastError());
        DeleteFileW(tmp.c_str());
        return false;
    }
    return true;
}

bool clearConfig(std::wstring& error) {
    std::wstring path = configPath();
    if (DeleteFileW(path.c_str())) return true;
    DWORD code = GetLastError();
    if (code == ERROR_FILE_NOT_FOUND || code == ERROR_PATH_NOT_FOUND) return true;
    error = L"cannot delete " + path + L": " + windowsError(code);
    return false;
}
