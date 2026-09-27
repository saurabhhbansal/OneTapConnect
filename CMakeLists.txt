cmake_minimum_required(VERSION 3.21)
project(OneTapConnect VERSION 1.1.0 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_executable(OneTapConnect WIN32
    src/main.cpp
    src/Bluetooth.cpp
    src/DataUsage.cpp
    src/HotspotProfile.cpp
    src/Settings.cpp
    src/SettingsDialog.cpp
    src/TrayApp.cpp
    src/Util.cpp
    src/WlanClient.cpp
    src/OneTapConnect.rc
    src/OneTapConnect.manifest
)

target_compile_definitions(OneTapConnect PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(OneTapConnect PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(OneTapConnect PRIVATE bthprops comctl32 iphlpapi wlanapi ws2_32)
