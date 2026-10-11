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


## Follow-up: HIDAPI global error-state corruption (12 October 2026)

The installed corrected build still crashed with Ambient running. CDB analysis of OpenRGB.exe.40976.dmp reports HEAP_FAILURE_LFH_BITMAP_MISMATCH; its GUI stack detects corruption during allocation rather than identifying its source.

A separate copy running with PageHeap stopped at verifier error 0x10 (corrupted start stamp) while freeing a block in hidapi-hotplug!hid_init called by hid_enumerate. Razer transport polling calls enumeration from effect/device threads, independently of detection and per-device mutexes. HIDAPI's documented lifecycle operations are not safe concurrently: https://github.com/libusb/hidapi/wiki/Multi%E2%80%90threading-Notes

The isolated regression deliberately enumerates a nonexistent VID/PID with four threads while another thread initializes the same shipped DLL. The unguarded negative control reproduces Invalid address specified to RtlFreeHeap. The protected variant passes 600 enumerations without hardware writes.

HidApiLifecycle.h supplies one process-wide recursive mutex for initialization, finalization, enumeration, open and close. Detection and Razer transport polling use these wrappers; native HID detector construction is protected separately, releasing the guard before controller registration/callbacks. RGB writes and the capture/Qt pipeline are unchanged. Physical usage still needs validation after deployment.


## Razer Direct brightness recovery (2026-10-12)

Live SDK inspection reported keyboard Direct brightness 255/255, but explicitly
resending that unchanged setting noticeably restored physical brightness (user
confirmed). This establishes a software/hardware state mismatch; the exact event
that discarded the hardware setting was not captured. The shared-pair driver
previously resent RGB frames without reapplying brightness after transport changes.

The USB/HyperSpeed shared-pair driver now retains the last requested brightness,
invalidates the refresh deadline on USB/RF changes and radio return, and reapplies
the requested value at most once every two seconds while sending Direct frames.
It preserves zero and lower brightness requests and never changes the RGB buffer.
The periodic reapply also recovers from an unnoticed hardware reset/lost command.
This is bounded recovery, not a claim that the original reset trigger is known.

Mouse isolation verified live with Ambient running: only the 11 LED Strip colors
were set to black via SDK; scroll-wheel colors continued to change, and all strip
values remained zero on a subsequent read. User confirmed wheel follows Ambient
and strip remains off. Visual Map intentionally preserves excluded-zone colors;
excluding a zone is not an off command. Global mouse brightness had been 0/255
and was restored to 255/255 to permit the wheel to light independently.
