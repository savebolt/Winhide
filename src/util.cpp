#include "util.h"

#include <windows.h>

#include <cstdio>
#include <cwctype>

bool g_color = false;

std::string toUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string s((size_t)n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), s.data(), n, nullptr, nullptr);
    return s;
}

std::wstring fromUtf8(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    if (n <= 0) return {};
    std::wstring w((size_t)n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
    return w;
}

static bool isSpace(wchar_t c) {
    return c == L' ' || c == L'\t' || c == L'\r' || c == L'\n';
}

std::wstring trim(const std::wstring& s) {
    size_t a = 0, b = s.size();
    while (a < b && isSpace(s[a])) a++;
    while (b > a && isSpace(s[b - 1])) b--;
    return s.substr(a, b - a);
}

std::wstring trimRight(const std::wstring& s) {
    size_t b = s.size();
    while (b > 0 && isSpace(s[b - 1])) b--;
    return s.substr(0, b);
}

std::wstring toLower(std::wstring s) {
    for (auto& c : s) c = (wchar_t)towlower(c);
    return s;
}

bool iequals(const std::wstring& a, const std::wstring& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); i++)
        if (towlower(a[i]) != towlower(b[i])) return false;
    return true;
}

bool icontains(const std::wstring& haystack, const std::wstring& needle) {
    if (needle.empty()) return true;
    if (haystack.size() < needle.size()) return false;
    std::wstring h = toLower(haystack);
    std::wstring n = toLower(needle);
    return h.find(n) != std::wstring::npos;
}

std::wstring basenameOf(const std::wstring& path) {
    size_t pos = path.find_last_of(L"\\/");
    return pos == std::wstring::npos ? path : path.substr(pos + 1);
}

std::wstring truncate(const std::wstring& s, size_t max) {
    if (s.size() <= max || max < 2) return s;
    return s.substr(0, max - 1) + L"\u2026";
}

std::wstring padRight(const std::wstring& s, size_t width) {
    if (s.size() >= width) return s;
    return s + std::wstring(width - s.size(), L' ');
}

void initConsole() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (out != nullptr && out != INVALID_HANDLE_VALUE && GetConsoleMode(out, &mode)) {
        if (SetConsoleMode(out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) g_color = true;
    }
}

void print(const std::wstring& s) {
    std::string u = toUtf8(s);
    if (!u.empty()) fwrite(u.data(), 1, u.size(), stdout);
    fflush(stdout);
}

void println(const std::wstring& s) {
    print(s + L"\n");
}

void eprintln(const std::wstring& s) {
    std::string u = toUtf8(s);
    if (!u.empty()) fwrite(u.data(), 1, u.size(), stderr);
    fwrite("\n", 1, 1, stderr);
    fflush(stderr);
}

void printlnErr(const std::wstring& s) {
    eprintln(s);
}

std::wstring dim(const std::wstring& s) {
    return g_color ? L"\x1b[2m" + s + L"\x1b[0m" : s;
}

std::wstring bold(const std::wstring& s) {
    return g_color ? L"\x1b[1m" + s + L"\x1b[0m" : s;
}
