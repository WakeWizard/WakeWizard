# Troubleshooting

## `wakewizard.local` does not open

Use the ESP32 IP address instead. Confirm the browser is on the same LAN and that the client/network supports mDNS.

## Initial Setup network is missing

After a new/factory-reset boot, look for `WakeWizard-XXXX` on 2.4 GHz Wi-Fi. The setup AP password is `wakewizard-setup`. If it does not appear, inspect the serial log at 115200 baud.

## WakeWizard returns to Initial Setup after changing Wi-Fi

Re-enter setup and verify the selected SSID/password. Current RC behavior will not silently reuse an old password when changing to a different protected SSID; a protected-network change with a blank password is rejected before save.

## Target does not wake

Check:

- BIOS/UEFI Wake-on-LAN setting
- OS/NIC WOL settings
- target MAC
- UDP port
- broadcast address
- VLAN/router/AP isolation
- whether the target is connected by a WOL-capable interface

Use Logs to confirm the wake job ran. On macOS, packet traffic can be inspected with:

```text
sudo tcpdump -i en0 -n -vv 'udp port 9'
```

Expected payloads are 102 bytes normally or 108 bytes with SecureOn.

## SecureOn target does not wake

Verify the NIC/firmware actually supports SecureOn and that the exact six-byte value configured in the target matches WakeWizard. A value such as `11:22:33:44:55:66` is transmitted as six raw trailing bytes.

## Scan misses a device

Confirm the host is online and inside the local subnet/range. Try Advanced Scan with a smaller explicit range. Discovery is IPv4/LAN reachability discovery and does not prove WOL capability.

## Scan takes time

Large ranges are intentionally processed in batches. The UI waits until the full range completes before showing the final aggregate. Other WakeWizard work can run between batches.

## Device disappears after a malformed configuration/import

Current RC validates active/backup/temp device files semantically at boot and should recover from a valid backup when the active file is invalid. If all recovery candidates are invalid, inspect serial logs and restore a known-good exported device configuration.

## Configuration fails after an update

Firmware and LittleFS are separate. Install the matching pair from the same release and hard-refresh the browser. A stale `app.js` can call incompatible firmware APIs.

## Logs look empty

Select the current log file, refresh, clear any filters and check NTP/time status. Early boot messages may live in the unsynchronized startup log until time is available.

## Weak Wi-Fi

Orange/red RSSI indicates weak reception. Improve 2.4 GHz coverage or ESP32 placement.

## Forgot administrator password

If no recovery path is available, export anything still accessible first and use Factory Reset.

## Hardware reset

Hold **BOOT** for approximately 10 seconds. Normal successful reset clears WakeWizard configuration/device state and returns to Initial Setup.

A known deferred edge case exists if one persistence-area clear succeeds and another fails; this is tracked for post-RC hardening.

## LittleFS mount/OTA recovery

WakeWizard no longer auto-formats LittleFS after a mount failure. This avoids silent data loss but means a damaged filesystem may require recovery with a matching filesystem image over USB/PlatformIO.

If filesystem OTA fails or is aborted, WakeWizard attempts to remount the existing LittleFS. If browser recovery is unavailable, flash a known-good matching firmware/filesystem pair over USB.

## Browser DevTools warns about HTTP

WakeWizard intentionally serves HTTP on the trusted LAN. Browsers may warn that downloads/resources are not HTTPS. This is expected; do not expose the appliance directly to the public Internet.

## Bug reports

Include:

- WakeWizard version
- ESP32 hardware
- browser/OS
- exact reproduction steps
- sanitized Logs/serial output
- whether firmware and LittleFS come from the same release
