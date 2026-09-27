#pragma once

#include "Settings.h"

#include <windows.h>

#include <optional>

// Totals the traffic of hotspot sessions for the current calendar month, using the Wi-Fi adapter's
// byte counters while the hotspot is connected.
class DataUsage {
public:
    DataUsage();

    void Start(const GUID& interfaceGuid);
    void Stop();
    unsigned long long ThisMonth();

private:
    void Record();
    void RollOverMonth();
    std::optional<unsigned long long> ReadCounter() const;

    MonthlyUsage usage_;
    unsigned long long interfaceLuid_ = 0;
    unsigned long long lastCounter_ = 0;
    bool active_ = false;
};
