# WakeWizard User Guide

## What WakeWizard does

WakeWizard is an ESP32 Wake-on-LAN appliance. It stores multiple targets and sends Magic Packets from a local browser UI. No cloud account or desktop client is required.

WakeWizard can request a wake; it cannot make an unsupported or incorrectly configured target wake. BIOS/UEFI, NIC/OS WOL settings and LAN topology still matter.

## First-time setup

1. Power the ESP32.
2. Connect to the Wi-Fi network `WakeWizard-XXXX`.
3. Enter the setup AP password `wakewizard-setup`.
4. Open `http://192.168.4.1/`.
5. Choose the UI language.
6. Set a hostname.
7. Select the normal Wi-Fi network and enter its password if protected.
8. Create an administrator password of at least 8 characters.
9. Save and allow WakeWizard to reboot.
10. Reconnect your browser to the normal LAN.
11. Open `http://<hostname>.local/` where mDNS works, or use the ESP32 IP.
12. Log in with the administrator password.

Language selection can be saved during Initial Setup before full provisioning completes.

## Main navigation

WakeWizard provides pages for:

- **Devices** — discovery, configured targets, Wake and Wake All
- **Configuration** — hostname, Wi-Fi, language, retention, admin password and configuration backup
- **Logs** — log browsing, filtering, refresh, download and deletion
- **System** — hardware/network/storage status, reboot, OTA and factory reset

## Devices

Use **Devices → Add Device** to create a target.

| Setting | Purpose |
| --- | --- |
| Name | Friendly device name |
| MAC Address | Required six-byte MAC used in the Magic Packet |
| IP Address | Optional reachability address for `stopWhenReachable` |
| Enabled | Controls eligibility for operations such as Wake All |
| Wake on Boot | Queue a wake job when WakeWizard starts |
| Initial Delay | Delay before the first packet |
| Magic Packets | Number of packets in the wake sequence |
| Packet Interval | Time between packets |
| UDP Port | Destination port; default 9 |
| Reachability checks | Maximum ICMP checks for stop-when-reachable behavior |
| Ping timeout | Configured ICMP timeout; runtime is capped to prevent long blocking |
| Broadcast IP | Optional explicit broadcast target; otherwise calculated automatically |
| SecureOn password | Optional six-byte value such as `11:22:33:44:55:66` |
| Stop when reachable | Stop the wake job once the target answers reachability checks |
| Category | Optional grouping/label value |
| Notes | Optional description |

WakeWizard applies the same server-side validation to Add/Edit/Import/boot data, so invalid values are rejected before persistence.

Use **Wake**, **Edit** and **Delete** for a single device. **Wake All** queues all enabled eligible devices using each device's individual settings.

## Wake behavior

A standard Magic Packet is 102 bytes. If a valid SecureOn password is configured, WakeWizard sends 108 bytes with the six SecureOn bytes appended.

Reachability checks are scheduled to keep the UI responsive: all due Magic Packets are processed first, and only one ICMP reachability check is performed per WakeEngine loop using round-robin scheduling.

## Network discovery

### Normal Scan

**Scan Network** searches the usable local subnet for host IP/MAC information.

### Advanced Scan

Advanced Scan supports:

- Start + End → exact range
- Start only → from Start through the usable local range
- End only → from the first usable address through End

Ranges must stay in the local subnet and are limited to 254 addresses. Network/broadcast addresses and invalid/reversed ranges are rejected.

The browser divides large scans into small batches so the web server remains available between batches. Results are shown after the complete scan.

Discovery does not prove that a discovered host supports WOL.

## Configuration and Wi-Fi

The Configuration page manages hostname, language, logging retention, Wi-Fi and administrator password.

Wi-Fi password behavior is deliberately conservative:

- saving the same SSID with the password field blank keeps the currently stored password
- changing to a different protected SSID requires a new password
- selecting a different open network clears the old stored Wi-Fi password
- an invalid network-change request is rejected before persistent configuration is modified

The setup AP is excluded from the normal network list.

## Backup and restore

### Devices

The Devices page can export/import saved device configuration. Device import is validated and persisted atomically.

WakeWizard keeps active, backup and temporary device files internally so a corrupted active file can recover from a valid backup/temp candidate at boot.

### Appliance configuration

Configuration export omits Wi-Fi/admin secrets.

Configuration import is a browser-side review flow: it loads the JSON values into the form, after which you review them, enter any required secrets and press **Save Configuration**.

Export configuration before factory reset or destructive maintenance.

## Languages

English and Italian are included. Available languages are discovered dynamically from `.properties` files under LittleFS.

## Logs

Logs provide file selection, search/filtering, auto-refresh, manual refresh, download and deletion. Wake, Wake All and scan activity are logged.

WakeWizard controls log growth using retention plus a calculated per-day filesystem budget capped at 128 KiB/day.

Early boot messages can appear before NTP synchronization. Once synchronized, dated UTC logs are used.

## System page

System reports:

- firmware version and uptime
- ESP32 model/revision/CPU/flash/heap
- hostname, IP, MAC, SSID/RSSI, gateway and subnet
- LittleFS total/used/free space
- NTP/time state

The page also provides reboot, firmware OTA, filesystem OTA and factory reset.

## Updating WakeWizard

WakeWizard has two separate artifacts:

1. firmware `firmware.bin`
2. LittleFS filesystem `littlefs.bin`

Install matching artifacts from the same release when both firmware and `data/` changed. After a filesystem update, hard-refresh the browser.

## Factory reset

The protected System action can perform a factory reset. If the UI is unavailable, hold the ESP32 **BOOT** button for approximately 10 seconds.

A known deferred RC1 edge case concerns partial failures while clearing different persistence areas; normal validated factory reset behavior returns the device to Initial Setup.

See [TROUBLESHOOTING.md](TROUBLESHOOTING.md) for recovery help.
