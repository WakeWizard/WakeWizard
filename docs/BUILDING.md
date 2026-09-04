# Building and Flashing WakeWizard

## Validated environment

WakeWizard `1.0.0-rc1` uses the PlatformIO environment `esp32dev` with versions pinned in `platformio.ini`:

```text
Espressif32 platform: 7.0.1
Arduino-ESP32 package: 3.20017.241212+sha.dcc1105b
Xtensa ESP32 toolchain: 8.4.0+2021r2-patch5
esptoolpy: 2.41100.0
mklittlefs: 1.203.210628
ArduinoJson: 7.4.3
```

Other build tools installed by the platform are also pinned in `platform_packages`.

## VS Code + PlatformIO

1. Install VS Code.
2. Install the PlatformIO IDE extension.
3. Open the repository root containing `platformio.ini`.
4. Connect the ESP32 over USB.

### Firmware

**PlatformIO: Upload** automatically builds any changed source and then flashes the resulting firmware. A separate Build step is optional if you are going to Upload immediately.

Command-line equivalents:

```text
pio run -e esp32dev
pio run -e esp32dev -t upload
```

The firmware binary is normally:

```text
.pio/build/esp32dev/firmware.bin
```

On macOS, `.pio` is hidden by Finder. Press `Cmd + Shift + .` to show hidden files, or copy the artifact from Terminal:

```text
cp .pio/build/esp32dev/firmware.bin ~/Desktop/WakeWizard-firmware.bin
```

## LittleFS / web UI

Web resources are under `data/`.

Build or upload the filesystem image with:

```text
pio run -e esp32dev -t buildfs
pio run -e esp32dev -t uploadfs
```

The image is normally:

```text
.pio/build/esp32dev/littlefs.bin
```

If only C++ source/header files changed, firmware Upload is sufficient. If anything under `data/` changed, upload the filesystem image as well. When both changed, upload both matching artifacts and hard-refresh the browser.

## Serial monitor

The configured monitor speed is 115200 baud.

```text
pio device monitor -b 115200
```

## WOL packet inspection from macOS

To inspect UDP port 9 traffic on a Mac Wi-Fi interface such as `en0`:

```text
sudo tcpdump -i en0 -n -vv 'udp port 9'
```

To inspect packet bytes, including SecureOn:

```text
sudo tcpdump -i en0 -n -XX 'udp port 9'
```

Expected payload lengths:

- standard WOL: `UDP, length 102`
- SecureOn enabled: `UDP, length 108`

## RC1 resource baseline

```text
Build: SUCCESS
Warnings: none
RAM:   66,604 / 327,680 bytes (20.3%)
Flash: 998,013 / 1,310,720 bytes (76.1%)
```

## Release artifacts

Do not commit `.pio/` output. For a public release, publish matching firmware and LittleFS images produced from the same source tag and pinned `platformio.ini`. Generate checksums and test the distributed pair on a factory-reset ESP32.
