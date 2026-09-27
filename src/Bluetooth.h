#pragma once

#include <winsock2.h>

#include <functional>
#include <mutex>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

struct PairedDevice {
    unsigned long long address = 0;
    std::wstring name;
    bool isPhone = false;
};

// Classic Bluetooth devices paired with this PC, phones first.
std::vector<PairedDevice> PairedBluetoothDevices();

// Opens a fresh hands-free Bluetooth connection to a paired phone so that its "Bluetooth › Is Connected"
// Shortcuts automation runs, and keeps it open until Stop(), which disconnects the phone again without
// unpairing it. All blocking work happens on a worker thread.
class BluetoothWake {
public:
    // Called once on the worker thread: 0 when the phone accepted the connection, otherwise a Winsock error.
    using ResultHandler = std::function<void(int error)>;

    BluetoothWake();
    ~BluetoothWake();
    BluetoothWake(const BluetoothWake&) = delete;
    BluetoothWake& operator=(const BluetoothWake&) = delete;

    void Start(unsigned long long address, ResultHandler onResult);
    void Stop();

private:
    void Run(const std::stop_token& stop, unsigned long long address, const ResultHandler& onResult);
    bool Adopt(SOCKET socket, const std::stop_token& stop);
    void CloseSocket();

    std::mutex mutex_;
    SOCKET socket_ = INVALID_SOCKET;
    unsigned long long address_ = 0;
    std::jthread worker_;
};
