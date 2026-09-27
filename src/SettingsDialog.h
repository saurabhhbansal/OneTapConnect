#pragma once

#include "Settings.h"

#include <windows.h>

#include <functional>
#include <optional>
#include <string>

// Applies validated input. Returning false keeps the dialog open; the handler reports its own errors.
// An empty passphrase means "keep the saved password" and is only passed when the SSID is unchanged.
using ApplySettingsHandler =
    std::function<bool(HWND dialog, const HotspotSettings& settings, const std::wstring& passphrase)>;

bool ShowSettingsDialog(HINSTANCE instance, HWND owner, const std::optional<HotspotSettings>& current,
                        const ApplySettingsHandler& apply);
