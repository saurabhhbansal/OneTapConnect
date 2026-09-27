<p align="center">
  <img src="assets/icon.png" width="96" alt="OneTapConnect icon">
</p>

# OneTapConnect

Connect your Windows PC to your iPhone's Personal Hotspot with one click.

On a Mac, your iPhone's hotspot is always in the Wi-Fi menu. Windows usually lists it only as **Hidden Network**, so joining means typing the network name and password every time. OneTapConnect saves those details once and puts a button in the system tray. Click it and you're online.

## Requirements

- Windows 10 or 11 with Wi-Fi.
- An iPhone with Personal Hotspot set up.
- **Your iPhone has to be broadcasting its hotspot.** Windows shows it in the Wi-Fi list as **Hidden Network**. Many iPhones keep broadcasting it in the background on their own. If yours doesn't, keep another Apple device (Mac, iPad) connected to the hotspot, or open **Settings › Personal Hotspot** on the iPhone before you connect.

## Install

1. Download `OneTapConnect.exe` from the [latest release](../../releases/latest).
2. Move it somewhere permanent, for example `%LOCALAPPDATA%\Programs\OneTapConnect\`.
3. Run it. The app isn't code-signed, so Windows SmartScreen may warn you; choose **More info › Run anyway**.
4. Enter your hotspot details:
   - **Hotspot name**: your iPhone's name, exactly as shown in **Settings › General › About › Name**. iPhone names use a curly apostrophe (’); if you type a straight one, OneTapConnect offers to fix it.
   - **Password**: shown in **Settings › Personal Hotspot › Wi-Fi Password**.

From then on OneTapConnect starts with Windows.

## Use

- **Click** the tray icon to connect.
- **Right-click** it for the menu: connect or disconnect, data used this month, settings, start with Windows, uninstall, and quit.
- To keep the button always visible, drag the icon out of the tray overflow (**^**) onto the taskbar. You can also pin `OneTapConnect.exe` to the taskbar; clicking the pinned app connects too.

### Settings

| Option | Default | What it does |
| --- | --- | --- |
| Connect only when I click the icon | On | Windows joins the hotspot only when you ask it to. |
| Connect automatically whenever it's in range | Off | Windows joins the hotspot on its own whenever it finds it. To find a hidden network, Windows has to call out its name, so your iPhone's name is broadcast wherever your PC goes. |
| Scan for the hotspot before connecting | Off | Looks for the hotspot before joining. Windows only allows Wi-Fi scans for apps with **Location** access, so it asks for permission the first time. |
| Treat as a metered connection | On | Tells Windows to go easy on data (Windows Update, OneDrive and other background downloads). |

### Data usage

The menu shows how much data went through the hotspot this month while OneTapConnect was running. The count resets at the start of each month.

## Privacy

Your password goes straight to Windows, which stores it encrypted in its Wi-Fi profile like any network you join. OneTapConnect never saves it. The app keeps only the hotspot name, your options and the monthly data count in `HKCU\Software\OneTapConnect`, and it never connects to the internet itself.

## Uninstall

Right-click the tray icon and choose **Uninstall**. This removes the saved network, the startup entry and all settings, then deletes `OneTapConnect.exe`.

## Build from source

You need Visual Studio 2022 (or its Build Tools) with the **Desktop development with C++** workload, which includes CMake.

```bash
cmake -B build -A x64
cmake --build build --config Release
```

The app is written to `build\Release\OneTapConnect.exe`.

## License

[MIT](LICENSE)
