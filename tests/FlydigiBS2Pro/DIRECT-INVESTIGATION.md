# BS2 Pro Direct/Ambient — remaining work

The user prioritizes Direct for Ambient. Native-mode completion does **not** close that requirement. Firmware actually read over Bluetooth: 0.0.3.5. Stock commands identified and exact-model firmware reverse show no demonstrated arbitrary-color RAM-render command.

## Observed blocker

0x41 initializes staging; 0x47 writes native RGB table chunks; 0x43 applies and persists. The table is 186 bytes: 00 02, first frame, last frame, transition ticks, brightness, then 6 LED-major groups of 10 RGB frames. Two Bluetooth tests staged red then issued 0x44 00 without 0x43, with valid ACKs; the strip remained green. Subsequent USB-only command tests used (1) indexed 0x47 staging and (2) sequential 0x42 blocks of at most 15 bytes, each after 0x41 and followed by 0x44 00, never 0x43. Both received valid USB replies (prefix 3) but the owner still saw the previous pattern, not red. The observed rendering blocker therefore also exists on the tested USB paths; it is not inferred solely from Bluetooth. Power removal restored the previous green Breath without manufacturer RGB retransmission. Native controls are valid; continuous commit-based animation is deliberately not offered.

## Non-solutions

Do not silently map Direct onto persistent uploads. Rate limiting to 1 or 5 FPS does not make a flash-writing path volatile. Do not drive the fan realtime mode to unlock fixed RGB presets: that is neither six arbitrary colors nor cooling-neutral. Do not use out-of-bounds frame indices to reach renderer RAM. The native encoder bounds the upload to the actual RGB program; do not weaken it for experiments. The BS3 patched firmware in THRM is not a compatible BS2 release.

## Potential next milestone

Obtain an authorized copy of the exact BS2 0.0.3.5 image and verify its hash and recovery path before proposing a firmware change. No image was found in the local application/cache during this run. The third-party analysis describes addresses but is not a tested binary patch. No firmware update or erase command was sent.

A model-specific firmware extension could add a bounded volatile RGB command that updates a double-buffered six-position RAM framebuffer and publishes atomically, without touching persistent animation data, fan control, thermal protection or power policy. Preserve existing official commands. Use a new, explicitly assigned opcode, exact length/range validation, a sequence number, transport-size-aware fragmentation if necessary, and matching ACKs. Keep a release command that restores the native animation. Decide last-frame/screensaver behavior explicitly; an automatic timeout that restores a native animation would conflict with holding Ambient colors during screensaver. Build and test the firmware in isolation, preserve an official recovery image, and obtain the user's explicit approval of the concrete reviewed flashing operation before changing their hardware.

Only after a proven volatile command exists: add OpenRGB Direct, one latest scene/no queue, independent rate limit by measured transport timings, no GUI-thread I/O, no RGB writes for unclaimed devices, and reconnection restoration. Test USB and Bluetooth separately, physically remove power, check RGB/ventilation, then verify real Ambient and Visual Map. Never call a native-mode or arbitrary upload test an Ambient validation.

Reference: https://github.com/TIANLI0/THRM/blob/main/docs/bs2pro-ota-ble-commands.md . Historical initial preparation remains in Documentation/HANDOFF-BS2-PRO.md; this file supersedes its unknowns about HID framing and the schema.
