# WakeWizard 1.0.0-rc1 Release Notes

Release Candidate 1 is the feature-frozen candidate for WakeWizard 1.0.

## Release status

The final RC1 sanity audit reported no concrete remaining release blocker:

```text
BUILD: SUCCESS
WARNINGS: None
RAM: 66,604 / 327,680 bytes (20.3%)
FLASH: 998,013 / 1,310,720 bytes (76.1%)
RC1 VERDICT: GO
CONFIDENCE: HIGH
```

## Major capabilities

- Multi-device Wake-on-LAN
- Wake All and Wake-on-Boot
- Per-device delay, packet count, interval, port and broadcast target
- Optional reachability stop logic
- SecureOn 6-byte password support
- Normal and Advanced LAN scanning
- Authentication and Initial Setup AP
- Configuration/device backup and restore
- English/Italian localization
- Persistent logs
- System information and OTA for firmware/LittleFS

## Reliability/security hardening completed for RC1

- Privileged APIs restricted before provisioning
- Atomic `saved_devices.json` persistence with backup/temp recovery
- Semantic device-file validation at boot
- Shared Device validation across API/import/boot
- Non-destructive LittleFS mount behavior
- Checked/single-commit NVS configuration writes
- Corrected log quota calculation
- XSS-safe rendering of untrusted UI values
- Reachability ping round-robin with bounded runtime
- Batched LAN scanning to keep the web server responsive
- Correct SSID/password transition behavior, including open networks
- Exact pinned PlatformIO/platform/package/library versions

## SecureOn

Without SecureOn, WakeWizard sends standard 102-byte WOL payloads. With a valid six-byte SecureOn password it sends 108 bytes. This was verified on hardware with packet capture.

## Upgrade notes

Firmware and LittleFS are independent images. If upgrading from an earlier development build, install both matching RC1 artifacts when web resources changed, then hard-refresh the browser.

Existing device files are migrated/defaulted and semantically validated. Recovery tries active, backup and temporary device files in that order.

## Known/deferred items

The following were explicitly accepted as non-blockers for RC1:

- factory-reset incomplete-failure edge behavior
- explicit payload/string size limits
- synchronous SSID network scanning
- automated tests and non-functional refactoring

## Feedback

Report reproducible RC issues with version, hardware, browser/OS, steps and sanitized logs. Do not disclose exploitable security issues publicly; follow [../SECURITY.md](../SECURITY.md).
