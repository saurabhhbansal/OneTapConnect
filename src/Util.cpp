#include "Util.h"

#include <windows.h>

#include <array>
#include <cwctype>
#include <format>

std::string ToUtf8(std::wstring_view text)
{
    if (text.empty())
        return {};

    const int wideLength = static_cast<int>(text.size());
    const int length = WideCharToMultiByte(CP_UTF8, 0, text.data(), wideLength, nullptr, 0, nullptr, nullptr);
    std::string utf8(length, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), wideLength, utf8.data(), length, nullptr, nullptr);
    return utf8;
}

std::wstring SystemErrorText(unsigned long errorCode)
{
    wchar_t* buffer = nullptr;
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, errorCode, 0, reinterpret_cast<wchar_t*>(&buffer), 0, nullptr);

    if (length == 0)
        return std::format(L"Error {}.", errorCode);

    std::wstring message(buffer, length);
    LocalFree(buffer);
    while (!message.empty() && std::iswspace(message.back()))
        message.pop_back();
    return message;
}

std::wstring FormatBytes(unsigned long long bytes)
{
    constexpr std::array units{ L"KB", L"MB", L"GB", L"TB" };

    if (bytes < 1024)
        return std::format(L"{} B", bytes);

    double value = static_cast<double>(bytes) / 1024.0;
    size_t unit = 0;
    while (value >= 1024.0 && unit + 1 < units.size()) {
        value /= 1024.0;
        ++unit;
    }
    return std::format(L"{:.1f} {}", value, units[unit]);
}

std::wstring ExecutablePath()
{
    std::wstring path(MAX_PATH, L'\0');
    for (;;) {
        const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (length < path.size()) {
            path.resize(length);
            return path;
        }
        path.resize(path.size() * 2);
    }
}

std::wstring SystemProgram(std::wstring_view fileName)
{
    wchar_t directory[MAX_PATH];
    const UINT length = GetSystemDirectoryW(directory, MAX_PATH);
    if (length == 0 || length >= MAX_PATH)
        return {};
    return std::format(L"{}\\{}", std::wstring_view(directory, length), fileName);
}

bool RunHidden(const std::wstring& program, const std::wstring& arguments, unsigned long timeoutMs)
{
    std::wstring commandLine = std::format(L"\"{}\" {}", program, arguments);
    STARTUPINFOW startup{ .cb = sizeof(startup) };
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(program.c_str(), commandLine.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
                        nullptr, &startup, &process))
        return false;

    DWORD exitCode = 0;
    const bool succeeded = timeoutMs == 0
        || (WaitForSingleObject(process.hProcess, timeoutMs) == WAIT_OBJECT_0
            && GetExitCodeProcess(process.hProcess, &exitCode) && exitCode == 0);

    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return succeeded;
}
