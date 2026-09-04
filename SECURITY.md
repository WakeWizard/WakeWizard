# Security Policy

WakeWizard is designed for a **trusted local network**.

The management UI and API use HTTP, not HTTPS. Application authentication controls privileged access, but LAN traffic is not transport-encrypted.

**Do not expose WakeWizard directly to the public Internet.**

## Initial Setup

A new or factory-reset device exposes a setup AP named `WakeWizard-XXXX` with the documented setup password `wakewizard-setup`.

The setup credential is not intended to provide strong security. Instead, WakeWizard restricts the unprovisioned API surface: only setup-required operations are available without administrator authentication. Privileged operations such as device management, logs, reboot, factory reset and OTA remain protected.

Create a strong administrator password during provisioning; the validated RC requires at least 8 characters for first setup.

## Credentials

Administrator credential material is stored as salt/hash rather than plaintext. General configuration export intentionally omits Wi-Fi and administrator secrets.

Wi-Fi password update logic prevents a password from a previous SSID being silently reused when changing to a different protected network.

Never post Wi-Fi/admin secrets in issues, screenshots or logs.

## Web UI input handling

Untrusted values such as SSIDs/system data/device names are rendered using safe DOM text operations where required. This prevents the confirmed pre-RC XSS path based on unescaped dynamic HTML.

## Persistence/recovery

Device persistence uses validated temporary/backup promotion instead of direct destructive overwrite. LittleFS mount failures do not automatically format the filesystem.

## Supported versions

Until `1.0.0` is public, only the latest Release Candidate should be considered for fixes. Add a supported-version table after the first stable release.

## Vulnerability reporting

Do not open a public issue for an exploitable vulnerability. Before public launch, enable GitHub Private Vulnerability Reporting or publish a private security contact here.

## Deployment recommendations

- Keep WakeWizard on a trusted LAN/VLAN.
- Do not port-forward its HTTP service.
- Use a strong administrator password.
- Keep firmware and LittleFS matched to the same release.
- Back up configuration/devices before destructive maintenance.
- Treat SecureOn as a legacy WOL feature, not cryptographic authentication.
