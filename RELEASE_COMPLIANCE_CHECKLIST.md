# Third-Party Compliance Release Checklist

Use this checklist for each WakeWizard public release.

- [ ] WakeWizard root `LICENSE` is Apache-2.0 and copyright information is current.
- [ ] `platformio.ini` dependency/platform versions are intentionally pinned or documented.
- [ ] `pio pkg list` reviewed.
- [ ] Release firmware rebuilt from the tagged source.
- [ ] `firmware.map` reviewed for newly linked libraries/components.
- [ ] `THIRD_PARTY_LICENSES.md` updated.
- [ ] Required third-party license/notice texts present under `licenses/`.
- [ ] `RELINKING.md` still matches the actual build process.
- [ ] Source tag/commit published alongside any precompiled firmware.
- [ ] Firmware and LittleFS release assets correspond to that tag.
- [ ] Web Installer links prominently to source, licenses/notices and release information.
- [ ] Any new Web Installer JS libraries/assets/fonts/icons have undergone license review.
- [ ] Release build artifacts retained internally.
- [ ] No private credentials, Wi-Fi passwords, local configuration or generated secrets are in release assets.

## Before the first public binary release

Because embedded static-linking compliance can be fact-specific, obtain legal review if you need
a legal guarantee about the exact LGPL relinking-material strategy. This package deliberately does
not claim that source availability alone always satisfies every LGPL binary-distribution scenario.
