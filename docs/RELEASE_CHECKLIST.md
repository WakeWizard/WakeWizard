# WakeWizard 1.0 Release Checklist

## Code / RC1

- [x] Feature freeze
- [x] Final EasyWOL → WakeWizard naming migration
- [x] Privileged pre-provisioning API hardening
- [x] Atomic DeviceStore persistence/recovery
- [x] Non-destructive LittleFS mount behavior
- [x] NVS save consistency hardening
- [x] Log quota fix
- [x] Frontend XSS fix
- [x] WakeEngine ping/round-robin responsiveness fix
- [x] Batched LAN scan responsiveness fix
- [x] SecureOn implementation and packet-capture validation
- [x] Semantic device-file validation/recovery at boot
- [x] Shared Device validation across Add/Edit/Import/boot
- [x] Wi-Fi SSID/password transition fix
- [x] Platform/dependency versions pinned
- [x] Firmware version set to `1.0.0-rc1`
- [x] Final RC1 sanity audit: **GO**, no MUST FIX blockers
- [ ] Clean build from fresh checkout
- [ ] Final end-to-end RC hardware regression
- [ ] Change version from `1.0.0-rc1` to `1.0.0` for final release

## Documentation

- [x] Project README
- [x] User Guide
- [x] Architecture
- [x] API overview
- [x] Building guide
- [x] Hardware guide
- [x] Troubleshooting
- [x] Contributing
- [x] Security policy
- [x] Changelog
- [x] RC1 release notes
- [x] Roadmap/TODO
- [ ] Add screenshots
- [ ] Final English proofread

## Open-source/legal

- [x] Apache-2.0 root `LICENSE`
- [x] Initial dependency/license review and notices
- [x] Exact build dependencies pinned
- [ ] Re-run final compliance review against `v1.0.0` artifacts and browser installer assets
- [ ] Configure private vulnerability reporting/contact

## Repository

- [ ] Confirm `.gitignore`
- [ ] Remove `.DS_Store`, `__MACOSX` and local/build artifacts from publication package
- [ ] Add repository description/topics
- [ ] Add issue/PR templates
- [ ] Add screenshots/assets

## Distribution

- [ ] Produce release `firmware.bin`
- [ ] Produce matching `littlefs.bin`
- [ ] Generate SHA-256 checksums
- [ ] Test release binaries on a factory-reset ESP32
- [ ] Complete/test browser installer or document PlatformIO-only initial release path
- [ ] Publish installation instructions

## Release

- [ ] Tag `v1.0.0`
- [ ] Publish final release notes
- [ ] Attach binaries/checksums
- [ ] Verify public links/install instructions

## Known deferred items

- [ ] Factory-reset partial-failure edge handling — accepted RC1 non-blocker
- [ ] Explicit payload/string limits — post-1.0
- [ ] Synchronous SSID scan — post-1.0
- [ ] Automated tests / non-functional refactor — post-1.0

## Community launch

- [ ] Prepare announcement
- [ ] Prepare screenshots/GIF/video
- [ ] Select ESP32/homelab/WOL communities
- [ ] Monitor first-installation friction and bug reports
