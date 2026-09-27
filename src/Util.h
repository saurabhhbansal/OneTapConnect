#pragma once

#include <string>
#include <string_view>

std::string ToUtf8(std::wstring_view text);
std::wstring SystemErrorText(unsigned long errorCode);
std::wstring FormatBytes(unsigned long long bytes);
std::wstring ExecutablePath();

// Full path of a program in the Windows system directory, so nothing else on PATH can stand in for it.
std::wstring SystemProgram(std::wstring_view fileName);

// Starts a console program without a window. With a timeout it waits and reports whether the program
// exited with code 0; without one it only reports whether the program started.
bool RunHidden(const std::wstring& program, const std::wstring& arguments, unsigned long timeoutMs = 0);
