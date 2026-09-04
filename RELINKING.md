# Relinking and Rebuilding WakeWizard

WakeWizard is intended to make the corresponding source and build recipe available alongside
precompiled firmware.

## Purpose

The firmware is statically linked with the Arduino-ESP32 framework, which contains code under
LGPL-2.1-or-later as well as components under other licenses. This document provides practical
information for rebuilding WakeWizard with a modified compatible Arduino-ESP32 framework.

## Baseline toolchain

The reviewed build used:

- PlatformIO Core 6.1.19
- Espressif32 platform 7.0.1
- `framework-arduinoespressif32` 3.20017.241212+sha.dcc1105b
- ArduinoJson 7.4.3
- environment `esp32dev`

The authoritative dependency constraints are those committed in `platformio.ini` for the release.

## Standard rebuild

From the repository root, with PlatformIO available:

    pio run

If PlatformIO is installed by the VS Code extension on macOS and `pio` is not in PATH:

    ~/.platformio/penv/bin/pio run

The resulting files are normally written below:

    .pio/build/esp32dev/

## Rebuilding with a modified Arduino-ESP32 framework

PlatformIO supports using a local/custom framework package. A user may obtain the corresponding
Arduino-ESP32 source, modify it, and configure PlatformIO to build WakeWizard against that modified
framework. Keep the same ESP32/ESP-IDF generation unless intentionally porting the application.

The exact procedure can vary with PlatformIO versions. The release's `platformio.ini`, source tree,
and documented dependency versions are the reference build recipe.

## Release-source correspondence

For every precompiled WakeWizard release, publish a source tag/commit identifying exactly which
source generated the binary. Do not publish only an unversioned firmware image.

Recommended release assets:

- `firmware.bin`
- LittleFS image, if distributed separately
- source tag/commit
- `platformio.ini`
- build instructions
- `THIRD_PARTY_LICENSES.md`
- `licenses/`
- this `RELINKING.md`

## Object/relink materials

Because LGPL compliance for statically linked embedded firmware can depend on the exact distribution
method and ability to replace/relink the LGPL-covered library, preserve the complete release build
artifacts internally. For public binary releases, consider publishing a reproducible source/build
bundle and, where required for the chosen compliance approach, suitable application object/relink
materials.

Do not delete the release source tag or dependency version information after publishing binaries.

This document is a technical compliance aid and is not legal advice.
