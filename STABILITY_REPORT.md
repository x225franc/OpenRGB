# OpenRGB / Ambient / Visual Map investigation — 2026-10-11

Historical Windows events include heap corruption 0xC0000374 and access violation 0xC0000005. Basic minidump inspection identifies HID-library addresses in one heap-corruption thread and NVIDIA/D3D11 addresses in another. Stack-memory address scanning is not a symbolized call stack and does not establish a single root cause.

Reviewed Claude's refresh/cleanup guard. Retained it, re-disabled refresh after detection waits, and avoided holding the pair mutex while unregistering HID callbacks. A late shared-pair registration now defers replacement of a live controller until a full rescan, where plugins can drain their references. Normal in-place USB/HyperSpeed transport selection remains available.

Ambient's matrix screen-copy renderer retained an unowned SDK matrix pointer while Visual Map could reallocate it. It now gets an owned matrix snapshot through the existing locked SDK value API, for zones and segments. Width, height and storage length are checked together. No plugin API change was needed.

Visual Map now serializes its virtual matrix rebuild and render, dispatches lifecycle changes to the Qt thread, preserves edited geometry across reconstruction, saves named maps automatically after a quiet interval, reports failed Save operations and writes files atomically. Details and isolated Qt regression are in Plugin/OpenRGBVisualMapPlugin/STABILITY_REPORT.md.

Validation performed:
- LightSync Plugin and Standalone regression suites pass; Toolkit Debug build has no warnings/errors; Standalone Debug has the pre-existing MSB3276 warning. Both deployed using the existing LightSync deployment script.
- Old Visual Map tab fails `Recreate lost edited map geometry`; corrected tab passes the same test, concurrent rebuild stress and persistence tests.
- Ambient tests pass, including immediate invalidation of the live matrix after a snapshot, malformed matrix lengths, mapping, smoothing, FPS gates and real Windows capture start/stop twice.
- Native application and both plugin DLLs compile Windows x64 Release against Qt 6.8.3 / API 5.

Configuration preservation: the pre-intervention config and binaries are copied under build/crash-investigation-20261011-225306. A deployment backup will additionally hold the latest config and original installed EXE/DLLs. The installed config folder inherits Program Files permissions. A write probe launched through the desktop shell succeeded, but its token was elevated; it does not establish a permissions failure for the currently running application. No ACL modification is necessary on the basis of this probe.

Rollback: Quit OpenRGB; restore OpenRGB.exe and the two plugin DLLs from the deployment backup, then reopen. Restore the saved config separately only if wanted.

User acceptance test: load your saved map; enable Ambient at 60 FPS; move and scale zones, resize the map, switch tabs, save, reload, then quit/reopen. Check geometry and observe stability for at least 30 minutes. Device reconnect/rescan and shutdown also need a real hardware run. No claim is made that all random crashes are eliminated, and no CPU/GPU or P95 measurement is invented.
