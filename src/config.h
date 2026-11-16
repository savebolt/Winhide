#pragma once

#include <string>

#include "win.h"

// Config lives next to the exe so the folder stays portable.
// WINHIDE_CONFIG overrides the path for read-only install locations.
std::wstring configPath();

bool loadConfig(Target& out, std::wstring& error);
bool saveConfig(const Target& target, std::wstring& error);
bool clearConfig(std::wstring& error);
