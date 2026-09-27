#include "DataUsage.h"

#include <winsock2.h>
#include <ws2ipdef.h>
#include <iphlpapi.h>

namespace {

unsigned long CurrentMonth()
{
    SYSTEMTIME now;
    GetLocalTime(&now);
    return now.wYear * 100UL + now.wMonth;
}

}

DataUsage::DataUsage()
    : usage_(Settings::LoadUsage())
{
}

void DataUsage::Start(const GUID& interfaceGuid)
{
    NET_LUID luid{};
    if (ConvertInterfaceGuidToLuid(&interfaceGuid, &luid) != NO_ERROR)
        return;

    interfaceLuid_ = luid.Value;
    if (const auto counter = ReadCounter()) {
        lastCounter_ = *counter;
        active_ = true;
    }
}

void DataUsage::Stop()
{
    if (!active_)
        return;

    Record();
    active_ = false;
}

unsigned long long DataUsage::ThisMonth()
{
    if (active_)
        Record();
    else
        RollOverMonth();
    return usage_.bytes;
}

void DataUsage::Record()
{
    const auto counter = ReadCounter();
    if (!counter)
        return;

    // The counters restart from zero when the adapter is reset.
    const unsigned long long delta = *counter >= lastCounter_ ? *counter - lastCounter_ : *counter;
    lastCounter_ = *counter;

    RollOverMonth();
    usage_.bytes += delta;
    Settings::SaveUsage(usage_);
}

void DataUsage::RollOverMonth()
{
    const unsigned long month = CurrentMonth();
    if (usage_.month != month)
        usage_ = { .month = month, .bytes = 0 };
}

std::optional<unsigned long long> DataUsage::ReadCounter() const
{
    MIB_IF_ROW2 row{};
    row.InterfaceLuid.Value = interfaceLuid_;
    if (GetIfEntry2(&row) != NO_ERROR)
        return std::nullopt;
    return row.InOctets + row.OutOctets;
}
