# WakeWizard web installer

Public URL: https://wakewizard.github.io/WakeWizard/

The static site in `web-installer/` uses ESP Web Tools 10.4.0 from unpkg.
GitHub Pages serves it over HTTPS. No server, account or Wi-Fi credential
collection is involved in the installer. A working internet connection to
GitHub Pages and unpkg is required. The ESP Web Tools dialog is supplied by
the library and may display English text.

## Hardware and behavior

- Classic ESP32, PlatformIO `esp32dev`, 4 MB flash, DIO, 40 MHz.
- Not an ESP32-C3, S2 or S3 build.
- Complete erase followed by a merged firmware + LittleFS image at offset 0.
- The user must acknowledge data loss before opening the USB installer.
- Improv Serial detection is disabled because WakeWizard uses its own setup AP.
- After flashing: join `WakeWizard-XXXX`, password `wakewizard-setup`, then
  visit `http://192.168.4.1/` and complete setup.
- For updates preserving data, use the existing device OTA interface.

## Deployment

Repository Settings → Pages → Source: **GitHub Actions**.
`.github/workflows/web-installer.yml` validates and deploys only `web-installer/`
when its content changes on `main`; it also supports manual dispatch.
The firmware source commit is recorded in `firmware/1.0.0/provenance.json`.
The source code and pinned toolchain in the initial public commit match the
owner-supplied release archive. The original firmware/LittleFS checksums were
verified before creating the merged image. Boot support files came from the
same archived build and matching pinned Arduino package.

## Local preview

From the repository root:

```sh
python3 scripts/check_web_installer.py
python3 -m http.server 8080 --directory web-installer --bind 127.0.0.1
```

Open `http://localhost:8080/`. Localhost is allowed for Web Serial; ordinary
HTTP on a LAN IP is not. Public deployment must use HTTPS.

## Preparing another release

Build firmware and LittleFS from the same reviewed source revision with the
pinned PlatformIO toolchain. Do not reuse bootloaders or partition tables
from a different build. Inspect the generated partition table: this release
uses app0 at `0x10000` (size `0x140000`) and LittleFS at `0x290000`
(size `0x160000`). Check those values again when changing build settings.

With esptool 4.11.0, create the image (paths relative to the repository):

```sh
python /path/to/esptool.py --chip esp32 merge_bin \
  -o web-installer/firmware/VERSION/wakewizard-esp32-full.bin \
  --flash_mode dio --flash_freq 40m --flash_size 4MB \
  0x1000 .pio/build/esp32dev/bootloader.bin \
  0x8000 .pio/build/esp32dev/partitions.bin \
  0xe000 /path/to/framework-arduinoespressif32/tools/partitions/boot_app0.bin \
  0x10000 .pio/build/esp32dev/firmware.bin \
  0x290000 .pio/build/esp32dev/littlefs.bin
```

Use a new version directory; retain published version directories unchanged.
Add a manifest pointing at the merged image with offset 0, a SHA256SUMS.txt,
and provenance.json with source commit, toolchain and component hashes from
the merged image (esptool patches the bootloader header). Update the page's
manifest URL and visible version together. Run the validator, inspect the
page, and test on a physical ESP32 before announcing a release.

The current validator intentionally enforces this release's 4 MB partition
layout. Adapt it with the build if the partition layout changes.

## Validation limits

Package checks validate headers, SHA-256, partition table checksum and bounds,
embedded component hashes and empty NVS. Browser checks cover loading and the
consent gate. They do not replace flashing a physical board and verifying
first boot, the setup AP and the web UI.

Upstream integration reference: https://esphome.github.io/esp-web-tools/

## Page languages

The installer has English (`index.html`, the default) and Italian (`it.html`)
pages with a flag and language-code switch in the header. The bare site URL
restores the last chosen language using local browser storage. Explicit page
links always open the selected language, even if storage is unavailable.
Both pages use the same firmware manifest and installer logic. Changing
language reloads the page; select a language before starting installation.
