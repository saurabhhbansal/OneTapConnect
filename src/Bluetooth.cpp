#include "Bluetooth.h"

#include <initguid.h> // Defines the service class GUIDs declared by bthdef.h in this translation unit.
#include <ws2bth.h>
#include <bluetoothapis.h>
#include <winioctl.h>
#include <bthioctl.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <string>
#include <string_view>

namespace {

constexpr DWORD HandshakeTimeoutMs = 3'000;
constexpr std::chrono::seconds ReconnectDelay{ 1 };

// Drops every Bluetooth connection to the device without unpairing it.
void Disconnect(unsigned long long address)
{
    BLUETOOTH_FIND_RADIO_PARAMS params{ .dwSize = sizeof(params) };
    HANDLE radio = nullptr;
    const HBLUETOOTH_RADIO_FIND find = BluetoothFindFirstRadio(&params, &radio);
    if (!find)
        return;

    do {
        BTH_ADDR target = address;
        DWORD returned = 0;
        DeviceIoControl(radio, IOCTL_BTH_DISCONNECT_DEVICE, &target, sizeof(target), nullptr, 0, &returned, nullptr);
        CloseHandle(radio);
    } while (BluetoothFindNextRadio(find, &radio));
    BluetoothFindRadioClose(find);
}

// Sleeps for the duration unless a stop is requested first. Returns false if stopped.
bool Pause(const std::stop_token& stop, std::chrono::milliseconds duration)
{
    std::mutex mutex;
    std::condition_variable_any wake;
    std::unique_lock lock(mutex);
    wake.wait_for(lock, stop, duration, [] { return false; });
    return !stop.stop_requested();
}

bool AwaitOk(SOCKET socket)
{
    std::string reply;
    char buffer[128];
    while (reply.find("OK") == std::string::npos) {
        if (reply.find("ERROR") != std::string::npos)
            return false;
        const int received = recv(socket, buffer, sizeof(buffer), 0);
        if (received <= 0)
            return false;
        reply.append(buffer, received);
    }
    return true;
}

// The service-level handshake a car kit performs, so the phone treats the PC as a connected hands-free device.
void StartHandsFreeSession(SOCKET socket)
{
    DWORD timeoutMs = HandshakeTimeoutMs;
    setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeoutMs), sizeof(timeoutMs));

    for (std::string_view command : { "AT+BRSF=0\r", "AT+CIND=?\r", "AT+CIND?\r", "AT+CMER=3,0,0,1\r" }) {
        if (send(socket, command.data(), static_cast<int>(command.size()), 0) == SOCKET_ERROR || !AwaitOk(socket))
            break;
    }

    timeoutMs = 0;
    setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeoutMs), sizeof(timeoutMs));
}

}

std::vector<PairedDevice> PairedBluetoothDevices()
{
    BLUETOOTH_DEVICE_SEARCH_PARAMS search{ .dwSize = sizeof(search) };
    search.fReturnAuthenticated = TRUE;
    search.fReturnRemembered = TRUE;
    search.fReturnConnected = TRUE;

    std::vector<PairedDevice> devices;
    BLUETOOTH_DEVICE_INFO info{ .dwSize = sizeof(info) };
    const HBLUETOOTH_DEVICE_FIND find = BluetoothFindFirstDevice(&search, &info);
    if (!find)
        return devices;

    do {
        if (info.fAuthenticated || info.fRemembered) {
            devices.push_back({ .address = info.Address.ullLong,
                                .name = info.szName,
                                .isPhone = GET_COD_MAJOR(info.ulClassofDevice) == COD_MAJOR_PHONE });
        }
        info = { .dwSize = sizeof(info) };
    } while (BluetoothFindNextDevice(find, &info));
    BluetoothFindDeviceClose(find);

    std::ranges::stable_partition(devices, &PairedDevice::isPhone);
    return devices;
}

BluetoothWake::BluetoothWake()
{
    WSADATA data;
    WSAStartup(MAKEWORD(2, 2), &data);
}

BluetoothWake::~BluetoothWake()
{
    Stop();
    WSACleanup();
}

void BluetoothWake::Start(unsigned long long address, ResultHandler onResult)
{
    Stop();
    address_ = address;
    worker_ = std::jthread([this, address, onResult = std::move(onResult)](std::stop_token stop) {
        Run(stop, address, onResult);
    });
}

void BluetoothWake::Stop()
{
    if (!worker_.joinable())
        return;

    worker_.request_stop();
    CloseSocket(); // Unblocks connect() or recv() on the worker.
    worker_.join();

    // Windows keeps its own hands-free and media links open after ours closes, which drains the phone's battery.
    Disconnect(address_);
}

void BluetoothWake::Run(const std::stop_token& stop, unsigned long long address, const ResultHandler& onResult)
{
    // The phone's automation reacts to a new connection, so first drop any link Windows is holding.
    Disconnect(address);
    if (!Pause(stop, ReconnectDelay))
        return;

    const SOCKET socket = ::socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
    if (socket == INVALID_SOCKET) {
        onResult(WSAGetLastError());
        return;
    }
    if (!Adopt(socket, stop))
        return;

    SOCKADDR_BTH target{};
    target.addressFamily = AF_BTH;
    target.btAddr = address;
    target.serviceClassId = HandsfreeAudioGatewayServiceClass_UUID;

    if (connect(socket, reinterpret_cast<const sockaddr*>(&target), sizeof(target)) == SOCKET_ERROR) {
        const int error = WSAGetLastError();
        CloseSocket();
        if (!stop.stop_requested())
            onResult(error);
        return;
    }

    StartHandsFreeSession(socket);
    if (stop.stop_requested())
        return;
    onResult(0);

    // Hold the connection until Stop() closes the socket or the phone drops it.
    char buffer[256];
    while (recv(socket, buffer, sizeof(buffer), 0) > 0) {
    }
    CloseSocket();
}

bool BluetoothWake::Adopt(SOCKET socket, const std::stop_token& stop)
{
    const std::lock_guard lock(mutex_);
    if (stop.stop_requested()) {
        closesocket(socket);
        return false;
    }
    socket_ = socket;
    return true;
}

void BluetoothWake::CloseSocket()
{
    const std::lock_guard lock(mutex_);
    if (socket_ == INVALID_SOCKET)
        return;

    shutdown(socket_, SD_BOTH);
    closesocket(socket_);
    socket_ = INVALID_SOCKET;
}
