#pragma once

#include <string>

// Console / encoding helpers.
extern bool g_color;

std::string toUtf8(const std::wstring& w);
std::wstring fromUtf8(const std::string& s);

std::wstring trim(const std::wstring& s);
std::wstring trimRight(const std::wstring& s);
bool iequals(const std::wstring& a, const std::wstring& b);
bool icontains(const std::wstring& haystack, const std::wstring& needle);
std::wstring basenameOf(const std::wstring& path);
std::wstring toLower(std::wstring s);
std::wstring truncate(const std::wstring& s, size_t max);
std::wstring padRight(const std::wstring& s, size_t width);

void initConsole();

void print(const std::wstring& s);
void println(const std::wstring& s = L"");
void eprintln(const std::wstring& s = L"");
void printlnErr(const std::wstring& s = L"");

std::wstring dim(const std::wstring& s);
std::wstring bold(const std::wstring& s);
