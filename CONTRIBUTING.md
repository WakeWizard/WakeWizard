# Contributing to WakeWizard

Thanks for contributing.

Until `1.0.0`, WakeWizard is feature-frozen. Prefer bug/security fixes, documentation, tests and release-quality improvements over new features.

## Workflow

1. Fork and create a focused branch.
2. Inspect the current implementation; do not rely on stale roadmap/patch assumptions.
3. Make the smallest coherent change.
4. Build with PlatformIO.
5. If `data/` changes, rebuild/test the LittleFS image as well.
6. Run the relevant regression/hardware tests.
7. Update documentation/localization when behavior changes.
8. Open a focused PR with test evidence.

## Validated build

Use the pinned `esp32dev` environment in `platformio.ini`:

```text
pio run -e esp32dev
```

Do not upgrade pinned platform/framework/library versions as part of an unrelated change.

## Principles

- Prefer conservative changes over broad refactors.
- Do not reintroduce EasyWOL naming/endpoints.
- Preserve Initial Setup without exposing privileged operational/admin APIs.
- Keep user-visible strings localizable.
- Render untrusted browser values safely; avoid interpolating them into `innerHTML`.
- Keep firmware and web API changes synchronized.
- Preserve atomic/recoverable device persistence.
- Preserve shared Device validation across Add/Edit/Import/boot.
- Avoid cumulative synchronous blocking in the main loop.
- Do not commit `.pio`, `.DS_Store`, `__MACOSX` or local artifacts.

A device-model change may affect persistence, JSON, UI, validation, import/export, Wake execution, localization and documentation.

## Pull requests

Explain:

- the problem
- the change
- why the change is needed
- build/test evidence
- compatibility/persistence/recovery implications

Include screenshots for visible UI changes.

## Security

Do not disclose exploitable vulnerabilities publicly. Follow [SECURITY.md](SECURITY.md).
