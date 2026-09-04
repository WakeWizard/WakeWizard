# Hardware

## Validated target

WakeWizard is currently built for the PlatformIO board environment:

```text
board = esp32dev
framework = arduino
```

The validated development hardware is a classic ESP32 DevKit-style board with USB serial support. WakeWizard does not require external sensors, relays or other peripherals for normal operation.

## Required connections

- USB cable for power, firmware flashing and serial diagnostics
- 2.4 GHz Wi-Fi connectivity to the target LAN
- Target computers/devices configured for Wake-on-LAN

## BOOT button

WakeWizard monitors GPIO0, normally connected to the board's **BOOT** button. Holding BOOT for approximately 10 seconds triggers the hardware factory-reset path.

Because GPIO0 behavior varies across ESP32 board designs, verify your board exposes the standard BOOT/GPIO0 arrangement before relying on this recovery method.

## Network requirements

Wake-on-LAN depends on the destination environment, not just WakeWizard. The target must support WOL in BIOS/UEFI/NIC/OS configuration. Broadcast forwarding, VLAN boundaries, Wi-Fi client isolation and router policies can prevent Magic Packets from reaching the target.

WakeWizard supports per-device broadcast address and UDP port settings. UDP port 9 is the normal default.

## SecureOn

SecureOn is optional and must also be supported/configured by the target NIC or firmware. WakeWizard appends a six-byte SecureOn value to the standard Magic Packet when configured.

SecureOn is not encryption and should not be treated as strong network authentication.

## USB flashing notes

PlatformIO firmware Upload builds and flashes the application partition. The LittleFS web UI is a separate filesystem image and requires **Upload Filesystem Image** when `data/` changes.
