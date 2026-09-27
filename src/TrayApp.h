#pragma once

#include "DataUsage.h"
#include "Settings.h"
#include "WlanClient.h"

#include <windows.h>
#include <shellapi.h>

#include <memory>
#include <optional>
#include <string>
#include <type_traits>

// Owns the notification-area icon and drives the connect flow. Runs entirely on the UI thread;
// Wi-Fi events are marshalled onto it through window messages.
class TrayApp {
public:
    static constexpr wchar_t WindowClassName[] = L"OneTapConnect.Tray";
    static UINT ConnectRequestMessage();

    explicit TrayApp(HINSTANCE instance);
    TrayApp(const TrayApp&) = delete;
    TrayApp& operator=(const TrayApp&) = delete;

    int Run(bool connectNow);

private:
    enum class State { Idle, Scanning, Connecting, Connected };

    struct IconDeleter {
        void operator()(HICON icon) const { DestroyIcon(icon); }
    };
    using UniqueIcon = std::unique_ptr<std::remove_pointer_t<HICON>, IconDeleter>;

    static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    bool CreateMainWindow();

    void LoadTrayIcon();
    void ShowTrayIcon(DWORD action = NIM_MODIFY) const;
    NOTIFYICONDATAW IconData() const;
    std::wstring Tooltip() const;
    void Notify(const std::wstring& title, const std::wstring& text, DWORD icon = NIIF_NONE) const;

    void OnTrayIcon(UINT event, POINT anchor);
    void OnWlanEvent(const WlanEvent& event);
    void OnTimer(UINT_PTR timer);
    void OnCommand(UINT command);
    bool FocusOpenDialog() const;
    void ShowMenu(POINT anchor);

    void Connect();
    void BeginConnect();
    void SyncConnectionState();
    void SetState(State state);
    std::wstring ProfileName() const;

    void OpenSettings();
    bool ApplySettings(HWND dialog, const HotspotSettings& updated, const std::wstring& passphrase);
    void Uninstall();

    HINSTANCE instance_;
    HWND window_ = nullptr;
    UINT taskbarCreatedMessage_;
    UniqueIcon trayIcon_;

    WlanClient wlan_;
    DataUsage usage_;
    std::optional<HotspotSettings> settings_;
    State state_ = State::Idle;
    GUID interface_{};
    bool uninstalled_ = false;
};
