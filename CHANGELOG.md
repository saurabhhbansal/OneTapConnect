# Changelog

All notable changes to OneTapConnect are listed here. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and versions follow [Semantic Versioning](https://semver.org/).

## [1.1.0] - 2026-09-27

### Added

- The tray icon turns amber while connecting, with a wave flowing outwards, and green once connected. The animation can be turned off in Settings.
- **Start with Windows** and **Uninstall** in Settings.
- A ready-made iPhone automation (iOS 27 or later), linked from the README and shown as a QR code in Settings.

### Changed

- The tray menu now holds only Connect, data used this month, Settings and Quit.
- Quitting asks for confirmation.

## [1.0.0] - 2026-09-27

### Added

- One-click connection to an iPhone's Personal Hotspot from the Windows system tray. The iPhone is woken over Bluetooth through a Shortcuts automation, and Bluetooth is disconnected again once the PC has joined.
- Options for connecting on click or automatically, scanning before connecting, and treating the hotspot as a metered connection.
- Monthly data usage in the tray menu.

[1.1.0]: https://github.com/saurabhhbansal/OneTapConnect/compare/v1.0.0...v1.1.0
[1.0.0]: https://github.com/saurabhhbansal/OneTapConnect/releases/tag/v1.0.0
