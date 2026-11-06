# Flydigi BS2 Pro — native RGB support

Validated on 6 November 2026, firmware **0.0.3.5**, VID/PID **37D7:1002**. Lighting only: the driver cannot encode fan speed, thermal policy, factory reset, pairing or firmware-update commands.

## Supported modes and physical layout

Off, Flow, Rotate, Breath, Single Static and Multi Static run on the cooler after one RGB upload. Brightness is 0–100; animation speed is 1–10. Two linear zones, **Left** and **Right**, each contain **Top / Middle / Bottom**. Six distinct control positions were checked with a six-color table and a red-only upper-left test. The diffuser blends adjacent colors; the number of visible gradients is not the number of independently addressed positions.

USB: interface 0, usage FFA0:0001, output 32 bytes including HIDAPI's zero prefix, input protocol prefix 3. Bluetooth: HID bus Bluetooth, usage FFA0:00FF, 25-byte reports, output ID 2 / input ID 1. Both interfaces can coexist. OpenRGB exposes one entry per transport; queued Bluetooth writes defer while the USB data connection is present. Keep the power supply connected when removing a USB data connection. The USB presence policy currently targets the single-cooler setup validated here; multiple physical BS2 units have not been tested.

## Direct / Ambient limitation

**Direct is not implemented or advertised for this stock firmware.** This is a remaining requested feature, not a completed capability.

The exact-model firmware analysis describes RGB commit 0x43 as a persistent flash write. The user confirmed green Breath survives physically disconnecting power. Passive observation found no manufacturer RGB resend during that test. Two bounded Bluetooth experiments wrote a red table into staging RAM (0x41 / indexed 0x47) and requested user-buffer selection (0x44 00), deliberately omitting 0x43: all ACKs passed, but the user still saw green. Two subsequent USB tests, using indexed 0x47 and sequential 0x42 staging followed by 0x44 00, also received valid USB replies but did not display red. Staging alone did not render the requested colors on either tested transport.

Do not implement Ambient by sending 0x43 every frame. Do not exploit the firmware's out-of-bounds frame-index bug, alter fan mode to select RGB presets, or flash a BS3 image onto a BS2. A genuine Direct path requires an independently validated volatile-render command or a separately reviewed model-specific firmware extension. See [DIRECT-INVESTIGATION.md](DIRECT-INVESTIGATION.md).

## Write discipline

Discovery does not claim RGB. One worker owns each HID handle, with one in-flight native scene and one replaceable pending scene. Matching opcode, report prefix, checksum and success status are required before advancing. A failed transfer stops without periodic retries. Identical successfully sent scenes are skipped; color/brightness edits wait for 500 ms of quiet before one final commit. On/Off writes are skipped when the queried state already matches, and persistent operations have a 200 ms guard window. No native effect is streamed or periodically re-uploaded, and no input/battery polling is added. Destruction wakes and joins the worker before closing its handle.

## Validation performed

- USB RGB status query and red/green/blue manufacturer captures: validated.
- USB fixed multicolor table, autonomous green Breath and Flow: owner confirmed, fan normal.
- Actual OpenRGB USB Rotate and Off: owner confirmed; Rotate moves bottom-to-top, Off leaves ventilation running.
- Bluetooth RGB status, single fixed green: owner confirmed even while USB was attached.
- Actual OpenRGB Bluetooth-only Breath after removing PC USB and powering separately: owner confirmed autonomous green breathing and normal ventilation.
- Unit encoder/schema/palette checks and production-worker fake-HID checks: run locally. A burst of 100 UI edits produces only one additional persistent commit.
- Windows x64 build and checkout-header isolation: verified. Hardware RPM/temperature measurements, multiple cooler units, extended stress, suspend/resume, and an independent firmware flashing workflow are not validated.

## Reproduce safely

`Run-Tests.cmd` tests protocol/modes. `Run-Worker-Tests.cmd` tests the actual worker extracted from production source. `Test-SDK.py --transport usb|bluetooth --mode "Breath" --brightness 30 --speed 5` targets a BS2-only local test server on 6745, using stable SDK v6 IDs. Close competing RGB controllers, retain cooling power and leave the manufacturer's cooling service running if it manages your fan policy. No test asks for a global cooler configuration upload.

Protocol sources: local Space Station 36.9.5 plus captures; [THRM BS2 Pro firmware analysis](https://github.com/TIANLI0/THRM/blob/main/docs/bs2pro-ota-ble-commands.md) (same 0.0.3.5); [THRM RGB implementation](https://github.com/TIANLI0/THRM/blob/main/internal/device/rgb.go); [flydigictl firmware analysis](https://github.com/ElXreno/flydigictl/blob/main/docs/FIRMWARE.md) concerns BS3 Pro and is supplementary, not a substitute for BS2 evidence. No proprietary application or firmware binary is redistributed.
