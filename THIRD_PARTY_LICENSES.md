# WakeWizard — Third-Party Software Notices

This file documents third-party software used to build and/or included in WakeWizard firmware.

> This is a compliance aid, not legal advice. If the dependency set or build platform changes,
> regenerate/review these notices before publishing a release.

## Release baseline reviewed

- PlatformIO Core: 6.1.19 (build tool; not shipped in firmware)
- PlatformIO Espressif32 platform: 7.0.1 (build platform; not itself shipped in firmware)
- Arduino-ESP32 PlatformIO package: `framework-arduinoespressif32` 3.20017.241212+sha.dcc1105b
  (Arduino-ESP32 2.0.17 generation; ESP-IDF 4.4.7 lineage)
- ArduinoJson: 7.4.3
- Target: ESP32 / `esp32dev`

The linker map review showed Arduino framework libraries and ESP-IDF runtime components are
linked into the firmware. Build-only tools such as esptool, mklittlefs, mkspiffs, OpenOCD and
the Xtensa compiler are not treated as firmware dependencies merely because PlatformIO installs
or invokes them.

## ArduinoJson 7.4.3

Copyright (c) 2014–2026 Benoit BLANCHON.

License: MIT.

WakeWizard uses ArduinoJson as an application dependency. The complete MIT license and copyright
notice are reproduced in `licenses/ArduinoJson-MIT.txt`.

Upstream: https://github.com/bblanchon/ArduinoJson

## Arduino-ESP32

WakeWizard is built using the Arduino core for ESP32. The framework package declares
`LGPL-2.1-or-later`; individual files/components may carry other compatible or permissive licenses.
For example, Arduino's main API header carries LGPL-2.1-or-later terms, while parts authored by
Espressif may be Apache-2.0.

WakeWizard's own source code remains licensed under Apache-2.0. Third-party code retains its own
license.

The complete GNU LGPL v2.1 text is included as
`licenses/Arduino-ESP32-LGPL-2.1.txt`.

Upstream: https://github.com/espressif/arduino-esp32

## ESP-IDF 4.4.7 lineage

Arduino-ESP32 includes and links components from Espressif IoT Development Framework (ESP-IDF).
Espressif's original ESP-IDF code is Apache-2.0, while ESP-IDF also incorporates third-party
components under other licenses. Source-file copyright/license headers take precedence over
summary documentation.

For the reviewed WakeWizard build, the linker map includes ESP-IDF runtime libraries such as
FreeRTOS, lwIP, mbedTLS, NVS, Wi-Fi/networking, app_update and other ESP32 support libraries.
This notice therefore points users to the complete upstream ESP-IDF 4.4.7 copyright/license
inventory rather than claiming that every linked object is Apache-2.0.

See `licenses/ESP-IDF-4.4.7-NOTICES.md`.

Upstream: https://github.com/espressif/esp-idf

## Public-domain component observed in installed framework

The installed Arduino-ESP32 package contains `cores/esp32/libb64/LICENSE`, a
Copyright-Only Dedication / Public Domain Certification. It is noted here for completeness.
The applicable upstream text should be preserved if that component is redistributed in source form.

## Binary releases and Web Installer

A WakeWizard binary release (including a firmware image offered through a Web Installer) is a
distribution of the compiled firmware and must not obscure the licenses of incorporated third-party
software.

Each binary release should therefore ship or link prominently to:

1. WakeWizard's Apache-2.0 LICENSE;
2. this THIRD_PARTY_LICENSES.md;
3. the license texts/notices in `licenses/`;
4. the corresponding WakeWizard source revision and reproducible build instructions;
5. the relinking/rebuild information in `RELINKING.md`.

The Web Installer itself must undergo a new dependency-license review if JavaScript packages,
frameworks, flashing libraries, fonts, icons or other third-party assets are added.

## Updating this inventory

Before every release that changes `platformio.ini`, PlatformIO platform/framework versions,
ArduinoJson, or Web Installer dependencies:

1. run `pio pkg list`;
2. build the release;
3. inspect `firmware.map`;
4. review newly introduced dependencies and their upstream licenses;
5. update this file and `licenses/` as needed.
