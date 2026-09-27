#pragma once

#include <string>
#include <string_view>

std::string ToUtf8(std::wstring_view text);
std::wstring SystemErrorText(unsigned long errorCode);
std::wstring FormatBytes(unsigned long long bytes);
std::wstring ExecutablePath();
