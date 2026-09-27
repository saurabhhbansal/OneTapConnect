#pragma once

#include <optional>
#include <string>

enum class ConnectMode { Manual, Automatic };

struct HotspotSettings {
    std::wstring ssid;
    ConnectMode connectMode = ConnectMode::Manual;
    bool scanBeforeConnect = false;
    bool metered = true;
};

struct MonthlyUsage {
    unsigned long month = 0; // yyyymm
    unsigned long long bytes = 0;
};

namespace Settings {

inline constexpr wchar_t StartupArgument[] = L"--startup";

std::optional<HotspotSettings> LoadHotspot();
void SaveHotspot(const HotspotSettings& settings);

MonthlyUsage LoadUsage();
void SaveUsage(const MonthlyUsage& usage);

bool IsStartupEnabled();
void SetStartupEnabled(bool enabled);

void RemoveAll();

}
