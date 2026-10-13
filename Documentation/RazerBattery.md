# Razer battery telemetry

Battery appears at the bottom of the device page in **Devices** for the
BlackWidow V3 Mini and Basilisk V3 Pro 35K Phantom Green handled by this fork.

- Bluetooth reads the standard Battery Service (180F), Battery Level (2A19),
  directly from the device. This service does not report charging state.
- USB / HyperSpeed uses read-only GET 07:80 and 07:84. Raw charge 0-255 is
  converted to percent. Shared receiver targets remain separate (mouse 1F,
  keyboard 9F); the existing wired-priority routing and transaction guard apply.
- Polling runs every 30 seconds outside the Qt UI thread. The UI only reads a
  synchronized snapshot. A sleeping/unavailable device retains its last valid
  reading and original timestamp instead of becoming a false 0%.
- The optional telemetry interface is core-only; RGBController and plugin API 5
  remain unchanged. No lighting, pairing, DPI, or power setting is written.
- USB poll workers are stopped and joined before their controller is destroyed.
  Bluetooth reads use the existing Bluetooth worker, not another lighting writer.

## Verified hardware (2026-10-13)

| Connection | Keyboard | Mouse |
|---|---|---|
| Bluetooth | 100%, read successful | 84%, read successful |
| Shared mouse HyperSpeed dongle | 100%, not charging | 84%, not charging |
| Keyboard USB + mouse HyperSpeed | 99%, charging | 84%, not charging |
| Keyboard HyperSpeed + mouse USB | 100%, not charging | 84%, charging |

The mouse receiver returned timeout while the mouse was wired; the USB endpoint
reported the battery correctly. Values are device reports, not independent
measurements of remaining capacity. Small differences between transports are
possible due to firmware updates/rounding. Continuous discharge accuracy has
not been measured.

## Reproducible checks

- `dotnet build tests/RazerBatteryProbe/RazerBatteryProbe.csproj -c Release`
  then run the built EXE in the logged-in Windows session, with the devices awake
  on Bluetooth. It reads only the standard battery service.
- `python tests/RazerSupport/Inspect-Battery.py --query` inventories HID and reads
  battery/charge. It validates command/target/status/CRC and holds the same named
  Razer interprocess mutex as OpenRGB.
- `tests/RazerSupport/Run-Transport-Tests.cmd` tests secondary target isolation,
  conversion, malformed responses, last-value retention, and interruptible worker
  shutdown alongside the existing RGB and wired/RF transport tests.
- In OpenRGB, select each device. Observe battery and connection. Connect/remove
  its USB cable and allow up to 30 seconds for a fresh read. Switch off a device
  and check that a failed read is marked unavailable with its last-reading age.

## Installation / rollback

The signed OpenRGB EXE must retain the existing OpenRGB.LightSync identity for
Lenovo Windows Dynamic Lighting. Use the identity preparation and installation
procedure in the native LightSync plugin documentation. A plain unsigned EXE
replacement can lose durable keyboard control.

Save the profile and VMap, quit OpenRGB, back up the old signed EXE and config,
then replace only the EXE with the new signed build. Restore the old EXE while
OpenRGB is closed to roll back. Telemetry has no saved battery values and does
not modify lighting profiles.
