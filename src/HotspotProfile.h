#pragma once

#include "Settings.h"

#include <optional>
#include <string>
#include <string_view>

// Builds the Windows WLAN profile that describes the hidden iPhone hotspot.
namespace HotspotProfile {

std::wstring NameFor(std::wstring_view ssid);

bool IsValidSsid(std::wstring_view ssid);
bool IsValidPassphrase(std::wstring_view passphrase);

std::wstring BuildXml(std::wstring_view ssid, std::wstring_view passphrase, ConnectMode mode);
std::optional<std::wstring> WithConnectMode(std::wstring profileXml, ConnectMode mode);

}
