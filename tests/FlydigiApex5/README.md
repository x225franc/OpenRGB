# Flydigi APEX 5 native lighting support

Validated locally on 28–29 October 2026. Scope: lighting only. No controller-wide software mode, input mapping, trigger configuration, firmware update or profile save command is implemented.

## Transports and layout

- USB and 2.4 GHz both enumerate as VID:PID **37D7:2501**. RGB uses HID interface **2**, usage **FFA0:0001**, with 32-byte output reports. The keyboard, pointer, XInput and secondary vendor interfaces are never opened by this driver.
- Product strings distinguish `Flydigi APEX5 Wireless` from wired `Flydigi APEX5 `. OpenRGB names the entries `Flydigi APEX 5 (2.4 GHz)` and `Flydigi APEX 5 (USB)`.
- Twelve independently controlled RGB LEDs in one fixed linear zone. A static red/green/blue spatial pattern was visually confirmed over both transports. LED names are ordinal; do not assume an unmeasured physical left/right order.
- When USB and the receiver are present together, receiver lighting writes are suspended while wired hardware is present. A previously owned receiver scene can resume after USB removal. Settings belong to each connection entry; cross-entry profile transfer is not implemented.

## Native modes

Direct, Off, Static, Breathing, Gradient, Default and Dynamic glow.

Breathing/Gradient upload native color frames, not a timed host animation. Default is the built-in left-to-right gradient confirmed by the owner; it remains distinct from Dynamic glow. Native presets continued after the diagnostic sender exited. The OpenRGB-generated green Breathing and native Off were also visually confirmed. Onboard game controls remained functional during USB/RF tests.

Brightness uses the observed 0–100 metadata field. The speed slider maps inversely to the cycle-period byte: increasing speed reduces period (1–100). Breathing supports up to five colors, with alternating color/black frames; Gradient supports two to five color frames. Palette generation beyond the observed cases remains a compatibility limit, not a manufacturer-certified capability statement.

## Direct, Ambient and Visual Map

Select Direct in Devices when sending custom colors, or let an effect select it. Ambient/Visual Map see the twelve LEDs through the normal OpenRGB RGBController API. No separate capture engine or lighting bridge is used.

A dedicated worker retains only the latest scene. Initial Direct setup uploads lighting metadata, subsequent frames patch only the twelve colors. Policy target: 20 scenes/s with a start-to-start limiter. The SDK test submitted at that target and measured approximately **81 successful output scenes per 5.02 seconds** (about 16.1/s); USB was confirmed fluid and reactive, and RF animation was confirmed visible after correcting the SDK test's hotplug addressing. OpenRGB-generated native Gradient on RF was also confirmed fluid with the game profile intact. This is not a certified hardware maximum or long-duration battery measurement.

While Direct submissions remain active, lighting metadata can be refreshed every two seconds. No autonomous keepalive or game-input/battery polling is performed when idle. RF recovery after a silent controller sleep without a USB change has not yet been physically validated.

## Wire protocol and scope safeguards

Only captured lighting opcodes A8 (begin chunk transfer) and A9 (chunk data) are permitted. Report ID 03, prefix 5A A5, 20-byte data chunks. Checksum is the byte sum from opcode through payload, modulo 256. Two header bytes select the starting 20-byte block; the next byte is chunk count. Metadata occupies block zero; twelve RGB values begin at block one and can straddle page boundaries.

The 20-byte metadata contains observed cycle, brightness, LED count and effect fields. No A6 commit/save command is issued. The encoder rejects unsupported opcodes before HID I/O. Captured fixtures contain only lighting reports, with no serial numbers, device paths, gameplay input or extracted manufacturer application code.

Discovery alone does not claim lighting. All writes occur on the worker, outside the UI lock. Failed writes preserve the latest scene, retries are bounded by a one-second backoff, and destruction joins the worker and closes owned handles.

## Bluetooth investigation

The tested Windows mode identifies as `Xbox Wireless Controller`, Bluetooth Classic, emulating **045E:02E0**. Its exposed HID collection has **16 input bytes, zero output bytes and zero Feature bytes**. Windows discovery returned no associated paired BLE gamepad and no RFCOMM services. No outgoing RGB command was attempted on Bluetooth.

Consequently this delivery does **not** advertise Bluetooth RGB support. Do not register the generic Microsoft VID/PID as a Flydigi RGB controller: real Xbox devices share that identity. A separate vendor-capable Bluetooth mode or documented transport would require further investigation. The separate Flydigi BS2 Pro BLE device on this PC is not the Apex 5.

## Reproducing tests

- `Run-Tests.cmd`: encoder layout/checksums, split RGB components, zero padding, native metadata, forbidden-save and oversize rejection.
- `Run-Worker-Tests.cmd`: actual production worker with fake HID, ownership, latest-frame behavior, idle traffic, failures, handle lifetime and USB priority.
- `Test-SDK.py`: isolated Flydigi-only server on 6745; SDK v6 stable IDs and error checking are required after hotplug. Example: `python tests/FlydigiApex5/Test-SDK.py --mode Direct --seconds 60 --fps 20`.

Keep Space Station fully closed for tests. Do not run two OpenRGB instances targeting the same controller. Native USB and RF tests and the final installed application test remain separate evidence; do not claim deployment until the executable has actually been installed.
