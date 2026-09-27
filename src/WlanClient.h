#pragma once

#include <windows.h>
#include <wlanapi.h>

#include <functional>
#include <optional>
#include <string>
#include <vector>

struct WlanEvent {
    enum class Kind { Connected, ConnectionFailed, Disconnected, ScanFinished };

    Kind kind{};
    GUID interfaceGuid{};
    std::wstring profileName;
    DWORD reasonCode = 0;
};

// RAII wrapper over the Native Wifi API. Profile operations apply to every Wi-Fi adapter;
// connection operations target a single one.
class WlanClient {
public:
    // Invoked on a system worker thread.
    using EventHandler = std::function<void(WlanEvent)>;

    WlanClient() = default;
    ~WlanClient();
    WlanClient(const WlanClient&) = delete;
    WlanClient& operator=(const WlanClient&) = delete;

    DWORD Open(EventHandler onEvent);

    std::vector<GUID> Interfaces() const;

    DWORD SaveProfile(const std::wstring& profileXml, std::wstring& errorText) const;
    std::optional<std::wstring> GetProfileXml(const std::wstring& profileName) const;
    void DeleteProfile(const std::wstring& profileName) const;
    void SetMetered(const std::wstring& profileName, bool metered) const;

    // Both require the user's Location permission on current Windows versions.
    DWORD Scan(const GUID& interfaceGuid, const std::wstring& ssid) const;
    std::optional<std::wstring> ConnectedProfile(const GUID& interfaceGuid) const;

    DWORD Connect(const GUID& interfaceGuid, const std::wstring& profileName) const;
    DWORD Disconnect(const GUID& interfaceGuid) const;

    static std::wstring ReasonText(DWORD reasonCode);

private:
    static void WINAPI OnNotification(PWLAN_NOTIFICATION_DATA data, PVOID context);

    HANDLE handle_ = nullptr;
    EventHandler onEvent_;
};
