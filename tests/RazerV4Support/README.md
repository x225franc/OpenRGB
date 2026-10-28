# Basilisk V4 Pro — verified support and remaining validation

Add V4 as a distinct model; preserve every existing V3 device and transport. Do not alter DPI, polling, bindings, Smart Dimming or sleep settings.

Official source: https://www.razer.com/gaming-mice/razer-basilisk-v4-pro (seven Chroma zones).

## Verified on 2026-10-28

- USB VID:PID 1532:00E2; receiver 1532:00E3. Both use HID interface 3, usage 000C:0001, 91-byte Feature reports and transaction 1F. Pointer interface 0 has no Feature reports.
- Firmware 1.1. Identity/normal control replies require valid checksums and success status. An unavailable RF mouse may return status 4.
- RF battery raw 245 (96%); wired charging was also read successfully. Preserve five-minute battery polling and manual refresh.
- Physical Direct mapping confirmed by owner: index 0 logo, index 1 wheel, indices 2–6 underglow. Three zones, seven LEDs. Individual underglow spatial order remains unmeasured.
- RF multicolor and wheel-only blue with every other LED black were visually confirmed. Wired logo blue / wheel red / underglow green also confirmed.
- Passive Synapse capture shows 0F:03 RGB frames are WRITE-ONLY: size 26, row 0, range 0–6. Reading a reply to these frames returns BUSY and is not evidence of failed output. Production skips GET only for this exact V4 streaming frame; other commands retain ACK validation. See official-rgb-examples.json and official-rgb-summary.json.
- Device software mode 3 plus custom mode selects Direct; hardware mode 0 selects native effects. Wired native green Breathing confirmed smooth after the test tool exited.
- Bluetooth name BSK V4 PRO, PnP identity 028E06E4000001 (VID 068E, PID 00E4). Vendor service 52401523-f97c-7f90-0e7f-6c6f4e36db1c. Battery service reported 100%.
- Bluetooth catalogue groups: 4 logo, 1 wheel, 10 underglow. Bluetooth exposes these three native groups; independent five-position underglow streaming is NOT established.
- Bluetooth static logo red / wheel blue / underglow green confirmed. Wheel-only native green Breathing and underglow-only native Wave confirmed autonomous and smooth with other groups off.
- Bluetooth Reactive: green and then red commands were accepted and read back, but owner observed NO click/scroll reaction with Smart Dimming disabled. Red state 05000301FF0000000000, brightness A0. Synapse Reactive was subsequently confirmed visually; after Synapse/Chroma quit, all groups read back Spectrum Cycling at 100%. Concurrent reads while Synapse remained running returned AccessDenied. The owner then explicitly deferred Reactive: V4 Bluetooth exposes only validated native modes; V3 modes remain unchanged.

## Remaining work

1. Reactive is deferred at the owner's request. Do not spend further hardware investigation on it in this delivery.
2. Shared BlackWidow routing on the V4 receiver was verified by identity (target 9F) and actual OpenRGB colors: keyboard green, V4 logo red / wheel blue / underglow green. The keyboard receiver was also tested in actual OpenRGB with the same independent colors. USB priority was confirmed with the keyboard receiver present: Bluetooth suspends writes. This exposed retained per-group brightness (wheel 96, logo 0, underglow 0); setting groups 1/4/10 to 96 restored all three colors visually. Production now restores all three group brightness values; the corresponding transport regression passes.
3. Actual OpenRGB V4 RF Direct and shared-keyboard colors are now visually confirmed. Actual Bluetooth static colors and wheel-only native Breathing were visually confirmed. After power-off/on in Bluetooth, the requested Breathing returns automatically, with logo and underglow remaining off.
4. Build, sign with the existing identity, package with backups, commit and push. Preserve profiles and Visual Map.

## Evidence and regression checks

Sanitized identity/HID/battery fixtures, official RGB examples and read-only GATT survey source are here. Full local diagnostic logs live in build/basilisk-v4 (ignored). Frida is a diagnostic-only dependency in an ignored environment, not an application dependency.

Razer layout tests retain V3 geometry checks. Transport tests pass V4 acknowledged initialization, write-only RGB, cached Direct setup, seven-position mapping and corrupt-control-ACK rejection alongside the legacy transport tests. Bluetooth reconnect tests retain the existing V3 cases; V4 three-group reconnect, silent state loss, independent colors and off-group restoration also pass.

## Shared receiver configuration

Back up OpenRGB.json and close OpenRGB before changing it. Under RazerHyperspeed, set BlackWidowV3MiniAndBasiliskV4ProSharedPair to true when this BlackWidow is paired with the V4. For a keyboard receiver this selects the explicit V4 peer model; the receiver PID alone cannot reveal the mouse model. The former BlackWidowV3MiniAndPhantomGreenSharedPair setting remains available for the V3 pair. Avoid enabling both for the same keyboard receiver. These settings do not pair the devices: pair them using the manufacturer tool first.

V4 USB/RF geometry is seven LEDs across Logo, Scroll Wheel and Underglow. Bluetooth controls three native groups, not seven independent pixels. Per-group black disables the group for supported Bluetooth effects. USB/RF Direct supplies all seven pixels to Effects and Visual Map.
