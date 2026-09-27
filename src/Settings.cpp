#include "Settings.h"

#include "Util.h"

#include <windows.h>

#include <cwchar>

namespace {

constexpr wchar_t AppKey[] = L"Software\\OneTapConnect";
constexpr wchar_t RunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t RunValue[] = L"OneTapConnect";

std::optional<std::wstring> ReadString(const wchar_t* key, const wchar_t* name)
{
    DWORD size = 0;
    if (RegGetValueW(HKEY_CURRENT_USER, key, name, RRF_RT_REG_SZ, nullptr, nullptr, &size) != ERROR_SUCCESS)
        return std::nullopt;

    std::wstring value(size / sizeof(wchar_t), L'\0');
    if (RegGetValueW(HKEY_CURRENT_USER, key, name, RRF_RT_REG_SZ, nullptr, value.data(), &size) != ERROR_SUCCESS)
        return std::nullopt;

    value.resize(std::wcslen(value.c_str()));
    return value;
}

template <typename T>
std::optional<T> ReadNumber(const wchar_t* name)
{
    static_assert(sizeof(T) == sizeof(DWORD) || sizeof(T) == sizeof(ULONGLONG));
    constexpr DWORD type = sizeof(T) == sizeof(DWORD) ? RRF_RT_REG_DWORD : RRF_RT_REG_QWORD;

    T value{};
    DWORD size = sizeof(value);
    if (RegGetValueW(HKEY_CURRENT_USER, AppKey, name, type, nullptr, &value, &size) != ERROR_SUCCESS)
        return std::nullopt;
    return value;
}

void WriteString(const wchar_t* key, const wchar_t* name, const std::wstring& value)
{
    const auto size = static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t));
    RegSetKeyValueW(HKEY_CURRENT_USER, key, name, REG_SZ, value.c_str(), size);
}

void WriteDword(const wchar_t* name, DWORD value)
{
    RegSetKeyValueW(HKEY_CURRENT_USER, AppKey, name, REG_DWORD, &value, sizeof(value));
}

void WriteQword(const wchar_t* name, ULONGLONG value)
{
    RegSetKeyValueW(HKEY_CURRENT_USER, AppKey, name, REG_QWORD, &value, sizeof(value));
}

}

std::optional<HotspotSettings> Settings::LoadHotspot()
{
    auto ssid = ReadString(AppKey, L"Ssid");
    if (!ssid || ssid->empty())
        return std::nullopt;

    HotspotSettings settings;
    settings.ssid = std::move(*ssid);
    settings.connectMode = ReadNumber<DWORD>(L"AutoConnect").value_or(0) ? ConnectMode::Automatic : ConnectMode::Manual;
    settings.scanBeforeConnect = ReadNumber<DWORD>(L"ScanBeforeConnect").value_or(0) != 0;
    settings.metered = ReadNumber<DWORD>(L"Metered").value_or(1) != 0;
    settings.wakeDevice = ReadNumber<ULONGLONG>(L"WakeDevice").value_or(0);
    settings.wakeDeviceName = ReadString(AppKey, L"WakeDeviceName").value_or(L"");
    settings.startWithWindows = IsStartupEnabled();
    settings.animateIcon = ReadNumber<DWORD>(L"AnimateIcon").value_or(1) != 0;
    return settings;
}

void Settings::SaveHotspot(const HotspotSettings& settings)
{
    WriteString(AppKey, L"Ssid", settings.ssid);
    WriteDword(L"AutoConnect", settings.connectMode == ConnectMode::Automatic);
    WriteDword(L"ScanBeforeConnect", settings.scanBeforeConnect);
    WriteDword(L"Metered", settings.metered);
    WriteQword(L"WakeDevice", settings.wakeDevice);
    WriteString(AppKey, L"WakeDeviceName", settings.wakeDeviceName);
    WriteDword(L"AnimateIcon", settings.animateIcon);
    SetStartupEnabled(settings.startWithWindows);
}

MonthlyUsage Settings::LoadUsage()
{
    return {
        .month = ReadNumber<DWORD>(L"UsageMonth").value_or(0),
        .bytes = ReadNumber<ULONGLONG>(L"UsageBytes").value_or(0),
    };
}

void Settings::SaveUsage(const MonthlyUsage& usage)
{
    WriteDword(L"UsageMonth", usage.month);
    WriteQword(L"UsageBytes", usage.bytes);
}

bool Settings::IsStartupEnabled()
{
    return ReadString(RunKey, RunValue).has_value();
}

void Settings::SetStartupEnabled(bool enabled)
{
    if (enabled)
        WriteString(RunKey, RunValue, L"\"" + ExecutablePath() + L"\" " + StartupArgument);
    else
        RegDeleteKeyValueW(HKEY_CURRENT_USER, RunKey, RunValue);
}

void Settings::RemoveAll()
{
    SetStartupEnabled(false);
    RegDeleteTreeW(HKEY_CURRENT_USER, AppKey);
    RegDeleteKeyW(HKEY_CURRENT_USER, AppKey);
}
