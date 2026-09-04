# Changelog

Notable WakeWizard changes are documented here. Semantic versioning is intended from `1.0.0`.

## [1.0.0-rc1] - 2026-09-03

### Added

- SecureOn support: optional 6-byte password appended to the standard Wake-on-LAN Magic Packet
- Advanced LAN search with start/end range support
- Dynamic multilingual UI resources; English and Italian included
- Configuration and device backup/export workflows
- System page with ESP32, network, filesystem and time/NTP status
- Separate firmware and LittleFS OTA upload paths

### Changed

- Final EasyWOL → WakeWizard naming/API migration
- Configuration API namespace standardized on `/api/config/wakewizard`
- LAN discovery is now split into frontend batches of up to 8 addresses, allowing the HTTP server to run between batches
- WakeEngine reachability checks now perform at most one ICMP ping per loop using round-robin scheduling
- Effective reachability ping timeout is capped at 1000 ms without changing the persisted/API value
- PlatformIO platform, framework/tool packages and ArduinoJson are pinned to exact validated versions
- Firmware version set to `1.0.0-rc1`

### Fixed / hardened

- Restricted privileged operational APIs before provisioning while keeping setup-required APIs available
- Made `saved_devices.json` persistence atomic with temporary-file validation, backup promotion and boot recovery
- Added semantic validation of active/backup/temp device files before accepting them at boot
- Removed destructive automatic LittleFS formatting on mount failure
- Made NVS configuration writes use checked operations and a single commit path
- Corrected daily log quota calculation
- Removed confirmed XSS sinks by rendering untrusted values with native DOM text APIs
- Unified Device validation across Add/Edit/Import/boot/runtime paths
- Added strict MAC and SecureOn format validation before persistence/use
- Corrected SSID/password update semantics so a password from a previous SSID cannot be silently reused for a new protected network
- Preserved open-network configuration without carrying an old Wi-Fi password forward
- Improved filesystem OTA failure/abort remount behavior
- Improved Advanced Scan result aggregation/deduplication and UI responsiveness

### RC1 validation

The final sanity audit completed with:

```text
BUILD: SUCCESS
WARNINGS: None
RAM: 66,604 / 327,680 bytes (20.3%)
FLASH: 998,013 / 1,310,720 bytes (76.1%)
RC1 VERDICT: GO
CONFIDENCE: HIGH
```

Hardware tests covered provisioning/authentication boundaries, device persistence/recovery, NVS configuration, WOL timing, `stopWhenReachable`, SecureOn 102/108-byte packets, batched LAN scanning, XSS-safe rendering and Wi-Fi credential changes.

### Known/deferred

- Factory-reset incomplete-failure edge handling
- Explicit API/payload/string size limits
- Synchronous Wi-Fi SSID scan behavior
- Automated test suite and non-functional refactoring

## Earlier development

Patch 012 introduced Advanced Device Search. Patch 011 introduced the dynamic multilingual UI. Earlier patches established device CRUD, Wake/Wake All, Wake-on-Boot, persistent logging, authentication, configuration backup/restore, network setup, factory reset and UI refinements.
