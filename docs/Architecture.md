# WakeWizard Architecture

## Overview

WakeWizard is a single ESP32 appliance combining firmware, HTTP/JSON APIs, a browser UI, persistent storage, LAN discovery and a Wake-on-LAN engine.

| Component | Responsibility |
| --- | --- |
| `main.cpp` | Boot order, LittleFS mount, service initialization, hardware BOOT-button reset loop |
| `Config` | Appliance settings in NVS: hostname, Wi-Fi, language, retention and admin credential material |
| `DeviceStore` | Persistent WOL targets, validation, atomic save/import and active/backup/temp recovery |
| `Network` | Station connection, setup AP and mDNS |
| `NetworkScanner` | IPv4/LAN host discovery and range scanning |
| `WakeEngine` | Wake jobs, delay/retry/interval, reachability checks, Magic Packet/SecureOn transmission |
| `Logger` | Serial + LittleFS logging, retention and NTP-based dated logs |
| `WebApp` | Static UI, authentication, REST APIs, OTA and system actions |
| `data/` | HTML/CSS/JavaScript, logo and language `.properties` files |

## Boot sequence

The firmware mounts LittleFS centrally with non-destructive behavior. A mount failure does **not** automatically format the filesystem. Configuration, networking, logging, device persistence, the web server and WakeEngine are then initialized.

When normal Wi-Fi connection cannot be established or the device is not provisioned, WakeWizard starts its setup access point:

```text
SSID: WakeWizard-XXXX
Password: wakewizard-setup
IP: 192.168.4.1
```

The setup AP uses AP+STA mode so Wi-Fi network scanning remains available during setup.

## Runtime states and authentication

### Unprovisioned / setup mode

Only setup-required APIs are available without administrator authentication. Privileged functions such as device operations, logs, reboot, factory reset and OTA remain protected.

### Provisioned

WakeWizard joins the configured LAN, starts mDNS where available and protects operational/admin APIs with an authenticated session.

The management transport is HTTP, so deployment assumes a trusted local network.

## Device model

Each stored device contains:

- stable numeric ID
- `enabled`
- `wakeOnBoot`
- name
- MAC address
- optional reachability IP
- UDP port
- initial delay
- packet count
- packet interval
- `stopWhenReachable`
- maximum reachability checks
- ping timeout
- optional broadcast address
- optional SecureOn password
- category
- notes
- created/updated metadata fields

Server-side validation is shared across Add/Edit/Import/boot paths so a configuration accepted through the API remains valid after persistence and reboot.

## Wake execution

Wake jobs preserve each device's delay, packet count, packet interval, UDP port, broadcast target and reachability policy.

Magic Packets are:

- **102 bytes** for standard WOL: six `FF` bytes plus the target MAC repeated 16 times
- **108 bytes** when SecureOn is configured: the standard 102 bytes plus the validated 6-byte SecureOn value

Reachability checks are intentionally bounded. `WakeEngine.loop()` processes all Magic Packets that are due, then performs at most one synchronous ICMP reachability check per loop using round-robin scheduling. The effective ping timeout is capped at 1000 ms to prevent cumulative multi-device blocking.

## LAN discovery

`NetworkScanner` supports bounded IPv4 ranges inside the current subnet. The backend already accepts `start`/`end`; the browser divides scans into batches of up to 8 addresses with a short pause between requests. This keeps the synchronous `WebServer` available between batches while preserving Normal Scan and Advanced Scan semantics.

The UI aggregates results only after the full scan and deduplicates by normalized IP+MAC pair while preserving distinct IPs that share a MAC.

## Persistence and recovery

### NVS

NVS stores appliance configuration and admin credential material. Configuration writes use checked NVS operations and one commit after all set operations succeed. Failed saves are not reported as success.

### DeviceStore / LittleFS

The active device file is:

```text
/saved_devices.json
```

Atomic persistence uses:

```text
/saved_devices.import.json
/saved_devices.backup.json
```

New content is written to a temporary file, byte-counted, parsed and semantically validated before promotion. The active file is rotated to backup before the validated temporary file becomes active.

At boot the recovery order is:

1. active file
2. backup
3. temporary/import file

Each candidate is migrated/defaulted and semantically validated before conversion into runtime `Device` objects. Invalid files are never partially loaded.

## Logging

Logs are written to LittleFS and exposed through the Logs page/API. The per-day budget is derived from a fraction of available filesystem capacity and retention days, with a 128 KiB/day maximum. Once NTP time is available, dated UTC log files are used; early boot messages can appear in an unsynchronized log first.

## Frontend security

Dynamic values such as SSIDs, system values and device names are rendered as text using native DOM APIs where required, avoiding the previously confirmed `innerHTML` XSS path.

Wi-Fi password handling distinguishes:

- same SSID + blank password → keep existing password
- different protected SSID + blank password → reject before persistence
- different open SSID → clear any previous Wi-Fi password

## OTA

Firmware and LittleFS are separate OTA artifacts. Filesystem upload failures/aborts attempt a non-destructive remount; successful uploads follow the reboot path. Firmware and filesystem assets should always come from the same release.

## Resource baseline

For the validated `1.0.0-rc1` build:

```text
RAM:   66,604 / 327,680 bytes (20.3%)
Flash: 998,013 / 1,310,720 bytes (76.1%)
```

## Known deferred work

The RC1 audit explicitly deferred:

- factory-reset incomplete-failure edge behavior
- explicit request/payload/string size limits
- synchronous Wi-Fi SSID scanning
- automated tests and non-functional refactoring
