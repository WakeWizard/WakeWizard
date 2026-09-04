# WakeWizard

WakeWizard is a self-contained Wake-on-LAN appliance for ESP32. It hosts its own responsive web UI, stores multiple target devices, discovers hosts on the local LAN and sends configurable Magic Packets without requiring a cloud service or desktop agent.

Current release line: **1.0.0-rc1**.

## Highlights

- Multi-device Wake-on-LAN with per-device delay, packet count, interval, UDP port and broadcast address
- Optional reachability checks with `stopWhenReachable`
- Wake-on-boot and Wake All
- SecureOn support: standard 102-byte Magic Packets or 108-byte packets with a 6-byte SecureOn password
- Normal and Advanced LAN discovery with bounded, batched scanning
- Browser-based device add/edit/delete, import/export and configuration backup
- Persistent logs stored in LittleFS, with download and cleanup controls
- Initial Setup access point, Wi-Fi configuration and administrator authentication
- English and Italian UI resources discovered from LittleFS
- System information, reboot, factory reset and separate firmware/LittleFS OTA upload
- Crash/data-loss hardening: atomic device persistence, backup recovery, semantic validation and non-destructive LittleFS mounting
- Reproducible PlatformIO build with pinned ESP32 platform/tool packages and ArduinoJson

## Hardware and software

The validated build target is PlatformIO `esp32dev` using the Arduino framework on classic ESP32 hardware. A typical ESP32 DevKit board with USB serial is sufficient.

See [docs/Hardware.md](docs/Hardware.md) for hardware notes and [docs/BUILDING.md](docs/BUILDING.md) for the exact pinned toolchain.

## Quick start

### Build and flash with PlatformIO

1. Install VS Code and the PlatformIO extension.
2. Open the repository root containing `platformio.ini`.
3. Connect the ESP32 over USB.
4. Use **PlatformIO: Upload** to build and flash the firmware.
5. Use **PlatformIO: Upload Filesystem Image** to upload the web UI and language resources from `data/`.

The command-line equivalents are:

```text
pio run -e esp32dev
pio run -e esp32dev -t upload
pio run -e esp32dev -t uploadfs
```

### Initial Setup

After a factory reset or on a new device:

1. Connect to the Wi-Fi network `WakeWizard-XXXX`.
2. Setup AP password: `wakewizard-setup`.
3. Open `http://192.168.4.1/`.
4. Choose language, hostname and normal Wi-Fi network.
5. Enter the Wi-Fi password when required and create an administrator password of at least 8 characters.
6. Save. WakeWizard reboots and joins the configured LAN.
7. Open `http://<hostname>.local/` where mDNS is available, or use the ESP32 IP address.

For the full workflow see [docs/USER_GUIDE.md](docs/USER_GUIDE.md).

## Repository layout

```text
data/       Web UI, CSS, JavaScript, images and language resources
include/    C++ headers
src/        ESP32 firmware
licenses/   Third-party license texts/notices
docs/       User, developer, API and release documentation
platformio.ini
```

Key firmware components are described in [docs/Architecture.md](docs/Architecture.md).

## Release status

The current RC1 code has passed repeated build, static review and hardware regression testing. The latest sanity audit reported:

```text
Build: SUCCESS
Warnings: none
RAM:   66,604 / 327,680 bytes (20.3%)
Flash: 998,013 / 1,310,720 bytes (76.1%)
RC1 verdict: GO
```

Known deferred items are tracked in [docs/TODO.md](docs/TODO.md) and [docs/Roadmap.md](docs/Roadmap.md).

## Security

WakeWizard is designed for a **trusted local network**. The management UI uses HTTP, not HTTPS, and should not be exposed directly to the public Internet. See [SECURITY.md](SECURITY.md).

## Documentation

- [User Guide](docs/USER_GUIDE.md)
- [Building and Flashing](docs/BUILDING.md)
- [Hardware](docs/Hardware.md)
- [Architecture](docs/Architecture.md)
- [HTTP API](docs/API.md)
- [Troubleshooting](docs/TROUBLESHOOTING.md)
- [Release Notes](docs/ReleaseNotes.md)
- [Roadmap](docs/Roadmap.md)
- [Release Checklist](docs/RELEASE_CHECKLIST.md)

## Contributing

WakeWizard is feature-frozen for the 1.0 release line. Bug fixes, security improvements, tests and documentation are preferred over new features until 1.0.0 is published. See [CONTRIBUTING.md](CONTRIBUTING.md).

## License

WakeWizard source is licensed under the Apache License 2.0. Third-party components retain their respective licenses. See [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md), [licenses/](licenses/) and [RELINKING.md](RELINKING.md).
