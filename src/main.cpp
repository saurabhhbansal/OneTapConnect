#include "Settings.h"
#include "TrayApp.h"

#include <windows.h>

#include <memory>
#include <string_view>

namespace {

struct HandleCloser {
    void operator()(HANDLE handle) const { CloseHandle(handle); }
};

}

int WINAPI wWinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE, _In_ PWSTR commandLine, _In_ int)
{
    const bool launchedAtStartup = std::wstring_view(commandLine).find(Settings::StartupArgument) != std::wstring_view::npos;

    const std::unique_ptr<void, HandleCloser> singleInstance(CreateMutexW(nullptr, FALSE, L"Local\\OneTapConnect"));
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        // Launching again (for example from a pinned taskbar button) asks the running instance to connect.
        HWND running = FindWindowW(TrayApp::WindowClassName, nullptr);
        if (running && !launchedAtStartup) {
            AllowSetForegroundWindow(ASFW_ANY);
            PostMessageW(running, TrayApp::ConnectRequestMessage(), 0, 0);
        }
        return 0;
    }

    TrayApp app(instance);
    return app.Run(!launchedAtStartup);
}
