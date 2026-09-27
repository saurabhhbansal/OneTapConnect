<p align="center">
  <img src="assets/icon.png" width="96" alt="OneTapConnect icon">
</p>

# OneTapConnect

Connect your Windows PC to your iPhone's Personal Hotspot with one click, without touching your phone.

On a Mac, your iPhone's hotspot is always in the Wi-Fi menu, and choosing it wakes the phone. OneTapConnect brings the same experience to Windows. Click the tray icon, and your PC switches on your iPhone's hotspot and joins it while the phone stays in your pocket.

## How it works

1. OneTapConnect connects to your iPhone over Bluetooth.
2. A Shortcuts automation on the iPhone switches Personal Hotspot on.
3. OneTapConnect joins the hotspot, then closes the Bluetooth connection again. The phone stays paired.

## Requirements

- Windows 10 or 11 with Wi-Fi and Bluetooth.
- An iPhone running iOS 17 or later, with Personal Hotspot set up.

## Setup

1. **Pair your iPhone with the PC** in **Windows Settings › Bluetooth & devices › Add device**.
2. **Add the automation to your iPhone:** scan the code below with the iPhone's camera, or open the [OneTapConnect Automation](https://www.icloud.com/shortcuts/7265a23b4d524836a9f0d26e65da695e) link on the iPhone. Make sure its Bluetooth trigger is set to your PC. The same code is shown in OneTapConnect's Settings.

   <img src="assets/automation-qr.png" width="180" alt="QR code for the OneTapConnect Automation">


   On iOS 17 to 26, create it yourself instead: open **Shortcuts › Automation › New Automation › Bluetooth**, choose your PC, then **Is Connected** and **Run Immediately**. Tap **Next › New Blank Automation**, add the **Set Personal Hotspot** action and set it to **On**.
3. **Install OneTapConnect:** download `OneTapConnect.exe` from the [latest release](../../releases/latest), move it somewhere permanent (for example `%LOCALAPPDATA%\Programs\OneTapConnect\`), and run it. The app isn't code-signed, so Windows SmartScreen may warn you; choose **More info › Run anyway**.
4. **Enter your hotspot details:**
   - **Hotspot name**: your iPhone's name, exactly as shown in **Settings › General › About › Name**. iPhone names use a curly apostrophe (’); if you type a straight one, OneTapConnect offers to fix it.
   - **Password**: shown in **Settings › Personal Hotspot › Wi-Fi Password**.
   - **iPhone**: your paired iPhone, which is selected for you.

From then on OneTapConnect starts with Windows.

## Use

- **Click** the tray icon to connect. The icon turns amber while connecting and green once you're online, a few seconds later.
- **Right-click** it for the menu: connect or disconnect, data used this month, settings, and quit.
- To keep the button always visible, drag the icon out of the tray overflow (**^**) onto the taskbar. You can also pin `OneTapConnect.exe` to the taskbar; clicking the pinned app connects too.
- While the hotspot is on, Windows also lists your iPhone by name in its own Wi-Fi menu.

### Settings

| Option | Default | What it does |
| --- | --- | --- |
| iPhone | Your paired iPhone | The phone OneTapConnect connects to over Bluetooth to switch the hotspot on. |
| Connect only when I click the icon | On | Windows joins the hotspot only when you ask it to. |
| Connect automatically whenever it's in range | Off | Windows joins the hotspot on its own whenever it's on, which can use mobile data without you noticing. |
| Scan for the hotspot before connecting | Off | Looks for the hotspot before joining. Windows only allows Wi-Fi scans for apps with **Location** access, so it asks for permission the first time. |
| Treat as a metered connection | On | Tells Windows to go easy on data (Windows Update, OneDrive and other background downloads). |
| Start with Windows | On | Starts OneTapConnect when you sign in. |
| Animate the tray icon while connecting | On | Shows a wave flowing through the amber icon while connecting. When off, the icon stays amber until connected. |

### Data usage

The menu shows how much data went through the hotspot this month while OneTapConnect was running. The count resets at the start of each month.

## Privacy

Your password goes straight to Windows, which stores it encrypted in its Wi-Fi profile like any network you join. OneTapConnect never saves it. The app keeps only the hotspot name, the chosen iPhone, your options and the monthly data count in `HKCU\Software\OneTapConnect`, and it never connects to the internet itself.

OneTapConnect connects to your iPhone as a Bluetooth hands-free device, only for the few seconds it takes to switch the hotspot on. A call arriving in that moment could briefly route to the PC.

Because the hotspot is hidden, Windows finds it by asking nearby devices for it by name. Every saved hidden network works this way, so your iPhone's name is included in your PC's Wi-Fi scans.

## Uninstall

Right-click the tray icon, choose **Settings**, and click **Uninstall**. This removes the saved network, the startup entry and all settings, then deletes `OneTapConnect.exe`. The iPhone stays paired; remove it in Windows Bluetooth settings if you like, and delete the automation in Shortcuts.

## Build from source

You need Visual Studio 2022 (or its Build Tools) with the **Desktop development with C++** workload, which includes CMake.

```bash
cmake -B build -A x64
cmake --build build --config Release
```

The app is written to `build\Release\OneTapConnect.exe`.

## License

[MIT](LICENSE)
