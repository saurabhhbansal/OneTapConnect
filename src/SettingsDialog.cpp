#include "SettingsDialog.h"

#include "HotspotProfile.h"
#include "resource.h"

#include <commctrl.h>

#include <algorithm>
#include <format>

namespace {

constexpr wchar_t Title[] = L"OneTapConnect";
constexpr wchar_t TypographicApostrophe = L'’';

struct DialogContext {
    const std::optional<HotspotSettings>& current;
    const ApplySettingsHandler& apply;
};

std::wstring ControlText(HWND dialog, int controlId)
{
    HWND control = GetDlgItem(dialog, controlId);
    std::wstring text(GetWindowTextLengthW(control), L'\0');
    GetWindowTextW(control, text.data(), static_cast<int>(text.size()) + 1);
    return text;
}

void Warn(HWND dialog, int controlId, const wchar_t* message)
{
    MessageBoxW(dialog, message, Title, MB_ICONWARNING);
    SendMessageW(dialog, WM_NEXTDLGCTL, reinterpret_cast<WPARAM>(GetDlgItem(dialog, controlId)), TRUE);
}

// iPhones name themselves "Name’s iPhone" with a typographic apostrophe that PC keyboards don't type.
// Returns false if the user cancels.
bool ConfirmApostrophe(HWND dialog, std::wstring& ssid)
{
    std::wstring suggestion = ssid;
    std::ranges::replace(suggestion, L'\'', TypographicApostrophe);
    if (suggestion == ssid || !HotspotProfile::IsValidSsid(suggestion))
        return true;

    const std::wstring question = std::format(
        L"iPhone names use a curly apostrophe (’), not a straight one (').\n\nUse “{}” instead?",
        suggestion);

    switch (MessageBoxW(dialog, question.c_str(), Title, MB_YESNOCANCEL | MB_ICONQUESTION)) {
    case IDYES:
        ssid = std::move(suggestion);
        SetDlgItemTextW(dialog, IDC_SSID, ssid.c_str());
        return true;
    case IDNO:
        return true;
    default:
        return false;
    }
}

void Initialize(HWND dialog, const DialogContext& context)
{
    const HotspotSettings settings = context.current.value_or(HotspotSettings{});

    SetDlgItemTextW(dialog, IDC_SSID, settings.ssid.c_str());
    SendDlgItemMessageW(dialog, IDC_SSID, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"e.g. Alex’s iPhone"));
    SendDlgItemMessageW(dialog, IDC_PASSWORD, EM_LIMITTEXT, 63, 0);
    if (context.current) {
        SendDlgItemMessageW(dialog, IDC_PASSWORD, EM_SETCUEBANNER, TRUE,
                            reinterpret_cast<LPARAM>(L"Leave blank to keep the saved password"));
    }

    const int mode = settings.connectMode == ConnectMode::Automatic ? IDC_MODE_AUTO : IDC_MODE_MANUAL;
    CheckRadioButton(dialog, IDC_MODE_MANUAL, IDC_MODE_AUTO, mode);
    CheckDlgButton(dialog, IDC_SCAN, settings.scanBeforeConnect ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(dialog, IDC_METERED, settings.metered ? BST_CHECKED : BST_UNCHECKED);

    const auto instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(dialog, GWLP_HINSTANCE));
    const auto loadIcon = [instance](int size) {
        return reinterpret_cast<LPARAM>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON, size, size, LR_SHARED));
    };
    SendMessageW(dialog, WM_SETICON, ICON_SMALL, loadIcon(GetSystemMetrics(SM_CXSMICON)));
    SendMessageW(dialog, WM_SETICON, ICON_BIG, loadIcon(GetSystemMetrics(SM_CXICON)));

    SetForegroundWindow(dialog);
}

bool Save(HWND dialog, const DialogContext& context)
{
    HotspotSettings settings;
    settings.ssid = ControlText(dialog, IDC_SSID);
    settings.connectMode = IsDlgButtonChecked(dialog, IDC_MODE_AUTO) ? ConnectMode::Automatic : ConnectMode::Manual;
    settings.scanBeforeConnect = IsDlgButtonChecked(dialog, IDC_SCAN) == BST_CHECKED;
    settings.metered = IsDlgButtonChecked(dialog, IDC_METERED) == BST_CHECKED;
    const std::wstring passphrase = ControlText(dialog, IDC_PASSWORD);

    if (!HotspotProfile::IsValidSsid(settings.ssid)) {
        Warn(dialog, IDC_SSID, L"Enter your hotspot’s name (at most 32 characters).");
        return false;
    }

    const bool ssidChanged = !context.current || context.current->ssid != settings.ssid;
    if (ssidChanged && !ConfirmApostrophe(dialog, settings.ssid))
        return false;

    const bool passphraseRequired = ssidChanged || !passphrase.empty();
    if (passphraseRequired && !HotspotProfile::IsValidPassphrase(passphrase)) {
        Warn(dialog, IDC_PASSWORD, L"The hotspot password must be 8 to 63 characters long.");
        return false;
    }

    return context.apply(dialog, settings, passphrase);
}

INT_PTR CALLBACK DialogProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_INITDIALOG:
        SetWindowLongPtrW(dialog, DWLP_USER, lParam);
        Initialize(dialog, *reinterpret_cast<const DialogContext*>(lParam));
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK: {
            const auto* context = reinterpret_cast<const DialogContext*>(GetWindowLongPtrW(dialog, DWLP_USER));
            if (Save(dialog, *context))
                EndDialog(dialog, IDOK);
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(dialog, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

}

bool ShowSettingsDialog(HINSTANCE instance, HWND owner, const std::optional<HotspotSettings>& current,
                        const ApplySettingsHandler& apply)
{
    const DialogContext context{ current, apply };
    return DialogBoxParamW(instance, MAKEINTRESOURCEW(IDD_SETTINGS), owner, DialogProc,
                           reinterpret_cast<LPARAM>(&context)) == IDOK;
}
