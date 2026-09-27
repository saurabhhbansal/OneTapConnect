#include "TrayApp.h"

#include "HotspotProfile.h"
#include "SettingsDialog.h"
#include "Util.h"
#include "resource.h"

#include <commctrl.h>
#include <windowsx.h>

#include <format>
#include <string_view>

namespace {

constexpr wchar_t AppName[] = L"OneTapConnect";
constexpr UINT TrayIconId = 1;
constexpr UINT TrayIconMessage = WM_APP + 1;
constexpr UINT WlanEventMessage = WM_APP + 2;
constexpr UINT BluetoothResultMessage = WM_APP + 3;

constexpr UINT_PTR ScanTimer = 1;
constexpr UINT_PTR RetryTimer = 2;
constexpr UINT_PTR DeadlineTimer = 3;
constexpr UINT ScanTimeoutMs = 5'000;
constexpr UINT HotspotStartupDelayMs = 3'000;
constexpr UINT RetryIntervalMs = 3'000;
constexpr UINT AttemptTimeoutMs = 60'000;

enum MenuCommand : UINT {
    CommandConnect = 1,
    CommandDisconnect,
    CommandSettings,
    CommandStartup,
    CommandUninstall,
    CommandQuit,
};

bool TaskbarUsesLightTheme()
{
    DWORD value = 0;
    DWORD size = sizeof(value);
    RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                 L"SystemUsesLightTheme", RRF_RT_REG_DWORD, nullptr, &value, &size);
    return value != 0;
}

// Menus treat '&' as a mnemonic prefix.
std::wstring MenuLabel(std::wstring_view text)
{
    std::wstring label;
    for (wchar_t c : text) {
        label += c;
        if (c == L'&')
            label += L'&';
    }
    return label;
}

bool ReportError(HWND owner, const std::wstring& message, const std::wstring& detail)
{
    const std::wstring text = detail.empty() ? message : message + L"\n\n" + detail;
    MessageBoxW(owner, text.c_str(), AppName, MB_ICONERROR);
    return false;
}

void ScheduleSelfDelete()
{
    // The ping gives this process time to exit so its executable can be deleted.
    RunHidden(SystemProgram(L"cmd.exe"),
              std::format(L"/d /c ping -n 3 127.0.0.1 >nul & del /f /q \"{}\"", ExecutablePath()));
}

}

UINT TrayApp::ConnectRequestMessage()
{
    static const UINT message = RegisterWindowMessageW(L"OneTapConnect.ConnectRequest");
    return message;
}

TrayApp::TrayApp(HINSTANCE instance)
    : instance_(instance)
    , taskbarCreatedMessage_(RegisterWindowMessageW(L"TaskbarCreated"))
    , settings_(Settings::LoadHotspot())
{
}

int TrayApp::Run(bool connectNow)
{
    if (!CreateMainWindow())
        return 1;

    const DWORD result = wlan_.Open([window = window_](WlanEvent event) {
        auto message = std::make_unique<WlanEvent>(std::move(event));
        if (PostMessageW(window, WlanEventMessage, 0, reinterpret_cast<LPARAM>(message.get())))
            message.release();
    });
    if (result != ERROR_SUCCESS) {
        ReportError(nullptr, L"The Windows Wi-Fi service isn’t available.", SystemErrorText(result));
        return 1;
    }

    LoadTrayIcon();
    ShowTrayIcon(NIM_ADD);

    if (Settings::IsStartupEnabled())
        Settings::SetStartupEnabled(true); // Keeps the entry pointing here if the exe was moved.
    if (settings_ && settings_->scanBeforeConnect)
        SyncConnectionState();

    if (!settings_)
        OpenSettings();
    else if (connectNow)
        Connect();

    MSG message;
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}

LRESULT CALLBACK TrayApp::WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_NCCREATE) {
        auto* app = static_cast<TrayApp*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        app->window_ = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }

    auto* app = reinterpret_cast<TrayApp*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    return app ? app->HandleMessage(message, wParam, lParam) : DefWindowProcW(window, message, wParam, lParam);
}

LRESULT TrayApp::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case TrayIconMessage:
        OnTrayIcon(LOWORD(lParam), POINT{ GET_X_LPARAM(wParam), GET_Y_LPARAM(wParam) });
        return 0;

    case WlanEventMessage: {
        const std::unique_ptr<WlanEvent> event(reinterpret_cast<WlanEvent*>(lParam));
        OnWlanEvent(*event);
        return 0;
    }

    case BluetoothResultMessage:
        OnBluetoothResult(static_cast<int>(wParam));
        return 0;

    case WM_TIMER:
        OnTimer(wParam);
        return 0;

    case WM_SETTINGCHANGE:
        if (lParam && std::wstring_view(reinterpret_cast<const wchar_t*>(lParam)) == L"ImmersiveColorSet") {
            LoadTrayIcon();
            ShowTrayIcon();
        }
        return 0;

    case WM_ENDSESSION:
        if (wParam)
            usage_.Stop();
        return 0;

    case WM_DESTROY: {
        if (!uninstalled_)
            usage_.Stop();
        NOTIFYICONDATAW data = IconData();
        Shell_NotifyIconW(NIM_DELETE, &data);
        PostQuitMessage(0);
        return 0;
    }
    }

    if (message == taskbarCreatedMessage_) {
        ShowTrayIcon(NIM_ADD);
        return 0;
    }
    if (message == ConnectRequestMessage()) {
        if (!FocusOpenDialog())
            Connect();
        return 0;
    }
    return DefWindowProcW(window_, message, wParam, lParam);
}

bool TrayApp::CreateMainWindow()
{
    WNDCLASSEXW windowClass{ .cbSize = sizeof(windowClass) };
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance_;
    windowClass.lpszClassName = WindowClassName;
    if (!RegisterClassExW(&windowClass))
        return false;

    // A hidden top-level window (not message-only) so it receives TaskbarCreated and theme broadcasts.
    CreateWindowExW(WS_EX_TOOLWINDOW, WindowClassName, AppName, WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, instance_,
                    this);
    return window_ != nullptr;
}

void TrayApp::LoadTrayIcon()
{
    const int iconId = TaskbarUsesLightTheme() ? IDI_TRAY_BLACK : IDI_TRAY_WHITE;
    HICON icon = nullptr;
    if (SUCCEEDED(LoadIconMetric(instance_, MAKEINTRESOURCEW(iconId), LIM_SMALL, &icon)))
        trayIcon_.reset(icon);
}

void TrayApp::ShowTrayIcon(DWORD action) const
{
    NOTIFYICONDATAW data = IconData();
    data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    data.uCallbackMessage = TrayIconMessage;
    data.hIcon = trayIcon_.get();
    wcsncpy_s(data.szTip, Tooltip().c_str(), _TRUNCATE);
    Shell_NotifyIconW(action, &data);

    if (action == NIM_ADD) {
        data.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &data);
    }
}

NOTIFYICONDATAW TrayApp::IconData() const
{
    return NOTIFYICONDATAW{ .cbSize = sizeof(NOTIFYICONDATAW), .hWnd = window_, .uID = TrayIconId };
}

std::wstring TrayApp::Tooltip() const
{
    if (!settings_)
        return std::format(L"{}\nClick to set up your hotspot", AppName);

    switch (state_) {
    case State::Connected:
        return std::format(L"{}\nConnected to {}", AppName, settings_->ssid);
    case State::Idle:
        return std::format(L"{}\nClick to connect to {}", AppName, settings_->ssid);
    case State::Waking:
        return std::format(L"{}\nWaking {} over Bluetooth…", AppName, settings_->ssid);
    default:
        return std::format(L"{}\nConnecting to {}…", AppName, settings_->ssid);
    }
}

void TrayApp::Notify(const std::wstring& title, const std::wstring& text, DWORD icon) const
{
    NOTIFYICONDATAW data = IconData();
    data.uFlags = NIF_INFO;
    data.dwInfoFlags = icon | NIIF_RESPECT_QUIET_TIME;
    wcsncpy_s(data.szInfoTitle, title.c_str(), _TRUNCATE);
    wcsncpy_s(data.szInfo, text.c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &data);
}

void TrayApp::OnTrayIcon(UINT event, POINT anchor)
{
    switch (event) {
    case NIN_SELECT:
    case NIN_KEYSELECT:
        if (!FocusOpenDialog())
            Connect();
        break;
    case WM_CONTEXTMENU:
        if (!FocusOpenDialog())
            ShowMenu(anchor);
        break;
    }
}

void TrayApp::OnWlanEvent(const WlanEvent& event)
{
    if (!settings_)
        return;

    const bool isHotspot = event.profileName == settings_->ssid;

    switch (event.kind) {
    case WlanEvent::Kind::ScanFinished:
        if (state_ == State::Scanning && IsEqualGUID(event.interfaceGuid, interface_))
            BeginConnect();
        break;

    case WlanEvent::Kind::Connected:
        if (isHotspot) {
            const bool requested = AttemptInProgress();
            EndAttempt();
            interface_ = event.interfaceGuid;
            usage_.Start(interface_);
            SetState(State::Connected);
            if (requested)
                Notify(std::format(L"Connected to {}", settings_->ssid), L"You’re online through your iPhone.");
        }
        break;

    case WlanEvent::Kind::ConnectionFailed:
        // The hotspot takes a few seconds to appear, so keep trying until the attempt's deadline.
        if (isHotspot && state_ == State::Connecting)
            WaitAndRetry(RetryIntervalMs);
        break;

    case WlanEvent::Kind::Disconnected:
        if (isHotspot && state_ == State::Connected) {
            usage_.Stop();
            SetState(State::Idle);
        }
        break;
    }
}

void TrayApp::OnBluetoothResult(int error)
{
    if (state_ != State::Waking)
        return;

    bluetoothError_ = error;
    if (error == 0)
        WaitAndRetry(HotspotStartupDelayMs); // Gives the iPhone's automation time to switch the hotspot on.
    else
        TryWifi(); // The hotspot may already be on.
}

void TrayApp::OnTimer(UINT_PTR timer)
{
    KillTimer(window_, timer);

    if (timer == ScanTimer && state_ == State::Scanning)
        BeginConnect();
    else if (timer == RetryTimer && state_ == State::Waiting)
        TryWifi();
    else if (timer == DeadlineTimer && AttemptInProgress())
        GiveUp();
}

void TrayApp::OnCommand(UINT command)
{
    switch (command) {
    case CommandConnect:
        Connect();
        break;
    case CommandDisconnect:
        wlan_.Disconnect(interface_);
        break;
    case CommandSettings:
        OpenSettings();
        break;
    case CommandStartup:
        Settings::SetStartupEnabled(!Settings::IsStartupEnabled());
        break;
    case CommandUninstall:
        Uninstall();
        break;
    case CommandQuit:
        DestroyWindow(window_);
        break;
    }
}

// While a modal dialog or message box is open its owner is disabled; bring that window forward instead.
bool TrayApp::FocusOpenDialog() const
{
    if (IsWindowEnabled(window_))
        return false;

    SetForegroundWindow(GetLastActivePopup(window_));
    return true;
}

void TrayApp::ShowMenu(POINT anchor)
{
    HMENU menu = CreatePopupMenu();

    if (settings_) {
        const std::wstring ssid = MenuLabel(settings_->ssid);
        if (state_ == State::Connected) {
            AppendMenuW(menu, MF_STRING, CommandDisconnect, (L"Disconnect from " + ssid).c_str());
        } else {
            AppendMenuW(menu, MF_STRING, CommandConnect, (L"Connect to " + ssid).c_str());
            SetMenuDefaultItem(menu, CommandConnect, FALSE);
        }
        const std::wstring usage = L"Data used this month: " + FormatBytes(usage_.ThisMonth());
        AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, usage.c_str());
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    }

    AppendMenuW(menu, MF_STRING, CommandSettings, settings_ ? L"Settings…" : L"Set up hotspot…");
    AppendMenuW(menu, MF_STRING | (Settings::IsStartupEnabled() ? MF_CHECKED : MF_UNCHECKED), CommandStartup,
                L"Start with Windows");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, CommandUninstall, L"Uninstall…");
    AppendMenuW(menu, MF_STRING, CommandQuit, L"Quit");

    // Without this the menu doesn't close when the user clicks elsewhere.
    SetForegroundWindow(window_);
    const UINT alignment = GetSystemMetrics(SM_MENUDROPALIGNMENT) ? TPM_RIGHTALIGN : TPM_LEFTALIGN;
    const auto command = static_cast<UINT>(TrackPopupMenuEx(
        menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON | alignment, anchor.x, anchor.y, window_, nullptr));
    DestroyMenu(menu);

    OnCommand(command);
}

void TrayApp::Connect()
{
    if (!settings_ || !settings_->wakeDevice) {
        OpenSettings();
        return;
    }
    if (state_ == State::Connected) {
        Notify(L"Already connected", std::format(L"You’re connected to {}.", settings_->ssid));
        return;
    }
    if (state_ != State::Idle)
        return;

    const auto interfaces = wlan_.Interfaces();
    if (interfaces.empty()) {
        Notify(L"No Wi-Fi adapter", L"Turn on Wi-Fi and try again.", NIIF_WARNING);
        return;
    }
    interface_ = interfaces.front();
    bluetoothError_ = 0;
    SetTimer(window_, DeadlineTimer, AttemptTimeoutMs, nullptr);

    SetState(State::Waking);
    bluetooth_.Start(settings_->wakeDevice, [window = window_](int error) {
        PostMessageW(window, BluetoothResultMessage, static_cast<WPARAM>(error), 0);
    });
}

void TrayApp::TryWifi()
{
    // Without Location permission the scan is refused, so fall straight through to connecting.
    if (settings_->scanBeforeConnect && wlan_.Scan(interface_, settings_->ssid) == ERROR_SUCCESS) {
        SetState(State::Scanning);
        SetTimer(window_, ScanTimer, ScanTimeoutMs, nullptr);
        return;
    }
    BeginConnect();
}

void TrayApp::BeginConnect()
{
    KillTimer(window_, ScanTimer);

    const DWORD result = wlan_.Connect(interface_, settings_->ssid);
    if (result != ERROR_SUCCESS) {
        EndAttempt();
        SetState(State::Idle);
        const std::wstring detail = result == ERROR_NOT_FOUND
            ? L"The saved hotspot network is missing. Open Settings and save it again."
            : SystemErrorText(result);
        Notify(L"Couldn’t connect", detail, NIIF_ERROR);
        return;
    }
    SetState(State::Connecting);
}

void TrayApp::WaitAndRetry(UINT delayMs)
{
    SetState(State::Waiting);
    SetTimer(window_, RetryTimer, delayMs, nullptr);
}

void TrayApp::GiveUp()
{
    EndAttempt();
    SetState(State::Idle);
    Notify(std::format(L"Couldn’t reach {}", settings_->ssid), FailureHint(), NIIF_WARNING);
}

// Ends the current attempt, including the Bluetooth connection, which is only needed to wake the hotspot.
void TrayApp::EndAttempt()
{
    KillTimer(window_, ScanTimer);
    KillTimer(window_, RetryTimer);
    KillTimer(window_, DeadlineTimer);
    bluetooth_.Stop();
}

bool TrayApp::AttemptInProgress() const
{
    return state_ != State::Idle && state_ != State::Connected;
}

std::wstring TrayApp::FailureHint() const
{
    if (bluetoothError_)
        return std::format(L"Your iPhone didn’t answer over Bluetooth (error {}). Check that Bluetooth is on "
                           L"and the phone is nearby.", bluetoothError_);
    return L"The hotspot didn’t appear. Check that your iPhone’s Bluetooth automation is set to run immediately.";
}

// Reading the current connection needs Location permission, so this only runs for users who opted into scanning.
void TrayApp::SyncConnectionState()
{
    for (const GUID& guid : wlan_.Interfaces()) {
        if (wlan_.ConnectedProfile(guid) == settings_->ssid) {
            interface_ = guid;
            usage_.Start(guid);
            SetState(State::Connected);
            return;
        }
    }
}

void TrayApp::SetState(State state)
{
    state_ = state;
    ShowTrayIcon();
}

void TrayApp::OpenSettings()
{
    ShowSettingsDialog(instance_, window_, settings_,
                       [this](HWND dialog, const HotspotSettings& updated, const std::wstring& passphrase) {
                           return ApplySettings(dialog, updated, passphrase);
                       });
}

bool TrayApp::ApplySettings(HWND dialog, const HotspotSettings& updated, const std::wstring& passphrase)
{
    std::wstring error;

    if (!passphrase.empty()) {
        const std::wstring xml = HotspotProfile::BuildXml(updated.ssid, passphrase, updated.connectMode);
        if (wlan_.SaveProfile(xml, error) != ERROR_SUCCESS)
            return ReportError(dialog, L"Couldn’t save the hotspot network.", error);
    } else if (updated.connectMode != settings_->connectMode) {
        // The saved profile keeps the password Windows encrypted, so only the connection mode is rewritten.
        const auto saved = wlan_.GetProfileXml(updated.ssid);
        const auto changed = saved ? HotspotProfile::WithConnectMode(*saved, updated.connectMode) : std::nullopt;
        if (!changed || wlan_.SaveProfile(*changed, error) != ERROR_SUCCESS)
            return ReportError(dialog, L"Couldn’t update the hotspot network. Enter the password again and save.",
                               error);
    }

    if (settings_ && settings_->ssid != updated.ssid) {
        wlan_.DeleteProfile(settings_->ssid);
        if (state_ == State::Connected) {
            usage_.Stop();
            SetState(State::Idle);
        }
    }

    // Asking for a scan now shows Windows' one-time Location prompt while the user is looking at this option.
    const bool scanNewlyEnabled = updated.scanBeforeConnect && !(settings_ && settings_->scanBeforeConnect);
    if (const auto interfaces = wlan_.Interfaces(); scanNewlyEnabled && !interfaces.empty())
        wlan_.Scan(interfaces.front(), updated.ssid);

    WlanClient::SetMetered(updated.ssid, updated.metered);

    const bool firstSetup = !settings_;
    Settings::SaveHotspot(updated);
    settings_ = updated;
    ShowTrayIcon();

    if (firstSetup) {
        Settings::SetStartupEnabled(true);
        Notify(L"You’re all set", L"Click the OneTapConnect icon whenever you want to connect.");
    }
    return true;
}

void TrayApp::Uninstall()
{
    const int answer = MessageBoxW(
        window_,
        L"This removes the saved hotspot network, the startup entry and all OneTapConnect settings, "
        L"then deletes OneTapConnect.exe.\n\nUninstall now?",
        L"Uninstall OneTapConnect", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
    if (answer != IDYES)
        return;

    if (settings_)
        wlan_.DeleteProfile(settings_->ssid);
    Settings::RemoveAll();
    ScheduleSelfDelete();

    uninstalled_ = true;
    DestroyWindow(window_);
}
