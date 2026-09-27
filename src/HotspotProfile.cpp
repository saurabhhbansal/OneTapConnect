#include "HotspotProfile.h"

#include "Util.h"

#include <algorithm>
#include <format>

namespace {

constexpr size_t MaxSsidBytes = 32;
constexpr size_t MinPassphraseLength = 8;
constexpr size_t MaxPassphraseLength = 63;

const wchar_t* ModeName(ConnectMode mode)
{
    return mode == ConnectMode::Automatic ? L"auto" : L"manual";
}

std::wstring EscapeXml(std::wstring_view text)
{
    std::wstring escaped;
    escaped.reserve(text.size());
    for (wchar_t c : text) {
        switch (c) {
        case L'&': escaped += L"&amp;"; break;
        case L'<': escaped += L"&lt;"; break;
        case L'>': escaped += L"&gt;"; break;
        case L'"': escaped += L"&quot;"; break;
        case L'\'': escaped += L"&apos;"; break;
        default: escaped += c; break;
        }
    }
    return escaped;
}

std::wstring ToHex(std::string_view bytes)
{
    constexpr wchar_t digits[] = L"0123456789ABCDEF";
    std::wstring hex;
    hex.reserve(bytes.size() * 2);
    for (unsigned char byte : bytes) {
        hex += digits[byte >> 4];
        hex += digits[byte & 0x0F];
    }
    return hex;
}

}

std::wstring HotspotProfile::NameFor(std::wstring_view ssid)
{
    return std::wstring(ssid) + L" (OneTapConnect)";
}

bool HotspotProfile::IsValidSsid(std::wstring_view ssid)
{
    const size_t bytes = ToUtf8(ssid).size();
    return bytes > 0 && bytes <= MaxSsidBytes;
}

bool HotspotProfile::IsValidPassphrase(std::wstring_view passphrase)
{
    return passphrase.size() >= MinPassphraseLength && passphrase.size() <= MaxPassphraseLength
        && std::ranges::all_of(passphrase, [](wchar_t c) { return c >= L' ' && c <= L'~'; });
}

// The SSID is written as hex because iPhone names usually contain a typographic apostrophe (U+2019),
// and Windows matches the hex bytes exactly. A manual, non-broadcast profile means Windows only
// probes for the hotspot when asked to, instead of announcing its name everywhere.
std::wstring HotspotProfile::BuildXml(std::wstring_view ssid, std::wstring_view passphrase, ConnectMode mode)
{
    return std::format(
        LR"(<?xml version="1.0"?>
<WLANProfile xmlns="http://www.microsoft.com/networking/WLAN/profile/v1">
    <name>{}</name>
    <SSIDConfig>
        <SSID>
            <hex>{}</hex>
            <name>{}</name>
        </SSID>
        <nonBroadcast>true</nonBroadcast>
    </SSIDConfig>
    <connectionType>ESS</connectionType>
    <connectionMode>{}</connectionMode>
    <autoSwitch>false</autoSwitch>
    <MSM>
        <security>
            <authEncryption>
                <authentication>WPA2PSK</authentication>
                <encryption>AES</encryption>
                <useOneX>false</useOneX>
            </authEncryption>
            <sharedKey>
                <keyType>passPhrase</keyType>
                <protected>false</protected>
                <keyMaterial>{}</keyMaterial>
            </sharedKey>
        </security>
    </MSM>
</WLANProfile>)",
        EscapeXml(NameFor(ssid)), ToHex(ToUtf8(ssid)), EscapeXml(ssid), ModeName(mode), EscapeXml(passphrase));
}

std::optional<std::wstring> HotspotProfile::WithConnectMode(std::wstring profileXml, ConnectMode mode)
{
    constexpr std::wstring_view openTag = L"<connectionMode>";
    constexpr std::wstring_view closeTag = L"</connectionMode>";

    const size_t open = profileXml.find(openTag);
    if (open == std::wstring::npos)
        return std::nullopt;

    const size_t valueStart = open + openTag.size();
    const size_t close = profileXml.find(closeTag, valueStart);
    if (close == std::wstring::npos)
        return std::nullopt;

    profileXml.replace(valueStart, close - valueStart, ModeName(mode));
    return profileXml;
}
