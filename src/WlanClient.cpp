#include "WlanClient.h"

#include "Util.h"

#include <wcmapi.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <format>
#include <memory>

namespace {

constexpr DWORD ClientVersion = 2;

struct WlanFree {
    void operator()(void* memory) const { WlanFreeMemory(memory); }
};

template <typename T>
using WlanPtr = std::unique_ptr<T, WlanFree>;

}

WlanClient::~WlanClient()
{
    if (handle_)
        WlanCloseHandle(handle_, nullptr);
}

DWORD WlanClient::Open(EventHandler onEvent)
{
    onEvent_ = std::move(onEvent);

    DWORD negotiatedVersion = 0;
    const DWORD result = WlanOpenHandle(ClientVersion, nullptr, &negotiatedVersion, &handle_);
    if (result != ERROR_SUCCESS)
        return result;

    return WlanRegisterNotification(handle_, WLAN_NOTIFICATION_SOURCE_ACM, TRUE, OnNotification, this, nullptr, nullptr);
}

std::vector<GUID> WlanClient::Interfaces() const
{
    std::vector<GUID> guids;
    PWLAN_INTERFACE_INFO_LIST rawList = nullptr;
    if (WlanEnumInterfaces(handle_, nullptr, &rawList) != ERROR_SUCCESS)
        return guids;

    const WlanPtr<WLAN_INTERFACE_INFO_LIST> list(rawList);
    for (DWORD i = 0; i < list->dwNumberOfItems; ++i) {
        const WLAN_INTERFACE_INFO& info = list->InterfaceInfo[i];
        if (info.isState != wlan_interface_state_not_ready)
            guids.push_back(info.InterfaceGuid);
    }
    return guids;
}

DWORD WlanClient::SaveProfile(const std::wstring& profileXml, std::wstring& errorText) const
{
    const auto interfaces = Interfaces();
    if (interfaces.empty()) {
        errorText = L"No Wi-Fi adapter was found.";
        return ERROR_NOT_FOUND;
    }

    for (const GUID& guid : interfaces) {
        DWORD reasonCode = WLAN_REASON_CODE_SUCCESS;
        const DWORD result = WlanSetProfile(handle_, &guid, 0, profileXml.c_str(), nullptr, TRUE, nullptr, &reasonCode);
        if (result != ERROR_SUCCESS) {
            errorText = result == ERROR_BAD_PROFILE ? ReasonText(reasonCode) : SystemErrorText(result);
            return result;
        }
    }
    return ERROR_SUCCESS;
}

std::optional<std::wstring> WlanClient::GetProfileXml(const std::wstring& profileName) const
{
    for (const GUID& guid : Interfaces()) {
        LPWSTR rawXml = nullptr;
        DWORD flags = 0;
        if (WlanGetProfile(handle_, &guid, profileName.c_str(), nullptr, &rawXml, &flags, nullptr) == ERROR_SUCCESS) {
            const WlanPtr<wchar_t> xml(rawXml);
            return std::wstring(xml.get());
        }
    }
    return std::nullopt;
}

void WlanClient::DeleteProfile(const std::wstring& profileName) const
{
    for (const GUID& guid : Interfaces())
        WlanDeleteProfile(handle_, &guid, profileName.c_str(), nullptr);
}

void WlanClient::SetMetered(const std::wstring& profileName, bool metered) const
{
    WCM_CONNECTION_COST_DATA cost{};
    cost.ConnectionCost = metered ? WCM_CONNECTION_COST_FIXED : WCM_CONNECTION_COST_UNRESTRICTED;

    for (const GUID& guid : Interfaces()) {
        WcmSetProperty(&guid, profileName.c_str(), wcm_intf_property_connection_cost, nullptr, sizeof(cost),
                       reinterpret_cast<const BYTE*>(&cost));
    }
}

DWORD WlanClient::Scan(const GUID& interfaceGuid, const std::wstring& ssid) const
{
    const std::string bytes = ToUtf8(ssid);
    DOT11_SSID dot11Ssid{};
    dot11Ssid.uSSIDLength = static_cast<ULONG>(std::min(bytes.size(), sizeof(dot11Ssid.ucSSID)));
    std::memcpy(dot11Ssid.ucSSID, bytes.data(), dot11Ssid.uSSIDLength);

    return WlanScan(handle_, &interfaceGuid, &dot11Ssid, nullptr, nullptr);
}

std::optional<std::wstring> WlanClient::ConnectedProfile(const GUID& interfaceGuid) const
{
    DWORD size = 0;
    PWLAN_CONNECTION_ATTRIBUTES rawAttributes = nullptr;
    const DWORD result = WlanQueryInterface(handle_, &interfaceGuid, wlan_intf_opcode_current_connection, nullptr,
                                            &size, reinterpret_cast<PVOID*>(&rawAttributes), nullptr);
    if (result != ERROR_SUCCESS)
        return std::nullopt;

    const WlanPtr<WLAN_CONNECTION_ATTRIBUTES> attributes(rawAttributes);
    if (attributes->isState != wlan_interface_state_connected)
        return std::nullopt;
    return std::wstring(attributes->strProfileName);
}

DWORD WlanClient::Connect(const GUID& interfaceGuid, const std::wstring& profileName) const
{
    WLAN_CONNECTION_PARAMETERS parameters{};
    parameters.wlanConnectionMode = wlan_connection_mode_profile;
    parameters.strProfile = profileName.c_str();
    parameters.dot11BssType = dot11_BSS_type_infrastructure;

    return WlanConnect(handle_, &interfaceGuid, &parameters, nullptr);
}

DWORD WlanClient::Disconnect(const GUID& interfaceGuid) const
{
    return WlanDisconnect(handle_, &interfaceGuid, nullptr);
}

std::wstring WlanClient::ReasonText(DWORD reasonCode)
{
    std::array<wchar_t, 256> buffer{};
    if (WlanReasonCodeToString(reasonCode, static_cast<DWORD>(buffer.size()), buffer.data(), nullptr) != ERROR_SUCCESS)
        return std::format(L"Reason code {}.", reasonCode);
    return buffer.data();
}

void WINAPI WlanClient::OnNotification(PWLAN_NOTIFICATION_DATA data, PVOID context)
{
    if (data->NotificationSource != WLAN_NOTIFICATION_SOURCE_ACM)
        return;

    WlanEvent event{ .interfaceGuid = data->InterfaceGuid };

    switch (data->NotificationCode) {
    case wlan_notification_acm_scan_complete:
    case wlan_notification_acm_scan_fail:
        event.kind = WlanEvent::Kind::ScanFinished;
        break;

    case wlan_notification_acm_connection_complete:
    case wlan_notification_acm_disconnected: {
        if (!data->pData || data->dwDataSize < offsetof(WLAN_CONNECTION_NOTIFICATION_DATA, strProfileXml))
            return;

        const auto* connection = static_cast<const WLAN_CONNECTION_NOTIFICATION_DATA*>(data->pData);
        event.profileName = connection->strProfileName;
        event.reasonCode = connection->wlanReasonCode;

        if (data->NotificationCode == wlan_notification_acm_disconnected)
            event.kind = WlanEvent::Kind::Disconnected;
        else if (connection->wlanReasonCode == WLAN_REASON_CODE_SUCCESS)
            event.kind = WlanEvent::Kind::Connected;
        else
            event.kind = WlanEvent::Kind::ConnectionFailed;
        break;
    }

    default:
        return;
    }

    static_cast<WlanClient*>(context)->onEvent_(std::move(event));
}
