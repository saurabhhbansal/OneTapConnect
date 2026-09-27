#pragma once

#include "Settings.h"

#include <optional>
#include <string>
#include <string_view>

// Builds the Windows WLAN profile that describes the hidden iPhone hotspot. The profile is named after
// the SSID so Windows lists the hotspot once, under the iPhone's name, and replaces any profile it made itself.
namespace HotspotProfile {

bool IsValidSsid(std::wstring_view ssid);
bool IsValidPassphrase(std::wstring_view passphrase);

std::wstring BuildXml(std::wstring_view ssid, std::wstring_view passphrase, ConnectMode mode);
std::optional<std::wstring> WithConnectMode(std::wstring profileXml, ConnectMode mode);

}
