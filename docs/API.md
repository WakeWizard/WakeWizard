# WakeWizard HTTP API

WakeWizard serves an HTTP/JSON API used by the bundled browser UI. `1.0.0` is intended to establish the first public compatibility baseline; until then, treat this document as an RC interface description rather than a permanent external API contract.

All endpoints are relative to the WakeWizard host, for example `http://wakewizard.local`.

## Authentication model

- `GET /api/auth/status` is public.
- `POST /api/auth/login` authenticates a provisioned unit.
- `POST /api/auth/logout` ends the session.
- During Initial Setup, only setup-required endpoints are allowed without an admin session.
- Device, log, Wake, reboot, factory-reset and OTA APIs require authentication.

## Endpoint summary

### Authentication and localization

```text
GET    /api/auth/status
POST   /api/auth/login
POST   /api/auth/logout
GET    /api/languages
POST   /api/config/language
```

### System and configuration

```text
GET    /api/system
GET    /api/config/wakewizard
POST   /api/config/wakewizard
GET    /api/config/wakewizard/export
GET    /api/wifi/networks
POST   /api/system/reboot
POST   /api/system/factory-reset
POST   /api/system/ota/firmware
POST   /api/system/ota/filesystem
```

There is intentionally no direct `/api/config/wakewizard/import` endpoint. General configuration import is a browser-side review-then-save workflow so secrets can be supplied explicitly before applying changes.

### Devices and Wake

```text
GET    /api/devices
POST   /api/devices
PUT    /api/devices/{id}
DELETE /api/devices/{id}
POST   /api/devices/{id}/wake
POST   /api/devices/wake-all
GET    /api/config/devices/export
POST   /api/config/devices/import
```

### LAN discovery

```text
GET /api/scan
GET /api/scan?start=<IPv4>&end=<IPv4>
```

`start` and `end` are optional. The server rejects malformed, out-of-subnet, network/broadcast, reversed or oversized ranges. The bundled frontend splits a full scan into batches of at most 8 addresses and aggregates the final result.

### Logs

```text
GET    /api/logs/files
GET    /api/logs/file?name=<file>
GET    /api/logs/download?name=<file>
DELETE /api/logs/file?name=<file>
DELETE /api/logs/history
```

## Device request fields

Add/Edit requests use JSON. The current device model accepts:

| Field | Type | Notes |
| --- | --- | --- |
| `name` | string | Required |
| `mac` | string | Required strict six-byte hexadecimal MAC |
| `ip` | string | Optional reachability IP |
| `enabled` | boolean | Default `true` |
| `wakeOnBoot` | boolean | Default `true` |
| `udpPort` | integer | Must be > 0; default 9 |
| `initialDelaySec` | integer | Converted to milliseconds |
| `packetCount` | integer | Must be > 0 |
| `packetIntervalSec` | integer | Must result in a positive interval |
| `stopWhenReachable` | boolean | Enables reachability stop logic |
| `maxReachabilityChecks` | integer | Must be > 0 |
| `pingTimeoutMs` | integer | Must be > 0; WakeEngine caps effective runtime timeout to 1000 ms |
| `broadcast` | string | Optional; calculated broadcast is used when empty/invalid at runtime |
| `secureOn` | string | Optional strict six-byte hex value such as `11:22:33:44:55:66` |
| `category` | string | Optional |
| `notes` | string | Optional |

Add/Edit apply shared server-side validation before store mutation. Duplicate MAC addresses return conflict semantics.

The device response additionally includes `id`, millisecond timing fields and metadata (`created`, `updated`).

## Wake responses

`POST /api/devices/{id}/wake` queues a single job and returns HTTP 202 on success.

`POST /api/devices/wake-all` returns counts for configured, eligible, queued and failed devices. Disabled devices are skipped.

## Configuration semantics

`GET /api/config/wakewizard` returns public configuration only; secrets are omitted.

Configuration save currently accepts hostname, SSID, optional Wi-Fi password, Wi-Fi-open indication, language, retention and optional administrator-password change fields.

Wi-Fi credential rules are intentionally strict:

- unchanged SSID + empty password → keep current password
- changed protected SSID + empty password → HTTP 400, no persistence change
- changed protected SSID + new password → accepted
- changed open SSID → accepted with empty password and old password cleared

Configuration export omits Wi-Fi/admin secrets.

## Error shape

Most JSON errors use:

```json
{
  "success": false,
  "message": "Human-readable message"
}
```

Successful mutation endpoints typically return:

```json
{
  "success": true
}
```

## Security notes

The API is served over HTTP and assumes a trusted LAN. Do not expose it directly to the public Internet. See [../SECURITY.md](../SECURITY.md).
