# Native RX 7900 XTX reference / Avatar controller

## What is implemented

Windows device: **AMD Radeon RX 7900 XTX Reference / Avatar**. The detector appears under this name in OpenRGB's supported detector list. It enumerates AMD ADL adapter metadata and accepts only PCI `1002:744C`, subsystem `1002:0E3B`, with a present display adapter. Logical display outputs are deduplicated by PCI bus/device/function. Partner designs are not matched. The PCI IDs cannot distinguish cosmetic Avatar boards from other matching reference boards.

One logical LED in one **GPU Lighting** single zone represents the whole lighting assembly. This is not an assertion about the number of physical LEDs. Modes: **Static** (global RGB, brightness 1 through 5 = observed 20/40/60/80/100% steps) and **Off**. No Direct mode, save-to-device feature, firmware update, or arbitrary I2C scan is implemented. No modifications to the plugin API (still 5), profiles or Visual Map format.

## Transport and initialization

The shared native transport uses the exact legacy `ADL_Display_WriteAndReadSMUI2C` function observed in RGB Tool, line 0, addresses B4/B5, offset 0, speed 100 kHz. It loads the AMD DLL from the Windows system directory. The driver must already be installed. Neither Cooler Master RGB Tool nor Synapse/Chroma needs to remain running; the implementation does not load their libraries. PawnIO is not used by this controller.

The first explicit update performs the captured `12 02` identity query, validates its response opcode, then `41 80`, `51 96`, and `51 28 ... E0`. Only after these succeed does it send the static color message (when applicable), the `51 A0` on/off mode message and `51 28 ... E0`. Detection and construction do not write lighting commands. Subsequent updates reuse initialization. Save command `50 55` is never sent. Persistence of the remaining apply command has not been characterized, so this is a static controller, not a validated Ambient transport.

Frames are 40 + 23 bytes, 57 + 23 bytes, AE + 18 bytes. Reads verify FF/E8/D1 frame markers and the full echoed payload for updates; the identity response instead verifies the identity opcode and frame markers because it contains device data. The same buffer is retained between read calls as in the official application. Any failed transfer or invalid response immediately stops the update. The controller latches that failure until a rescan to prevent retry storms and partial follow-on writes. A warning/error is logged rather than silently claiming success.

ADL2 PMLog device creation and KeepAwake are scoped to each exchange and cleaned up afterward. KeepAwake returns -1 with the tested driver; this is treated as an unsupported optional power-management feature, not a successful device response. Actual SMU responses remain mandatory. No PMLog sensor logging is started. The shared legacy ADL session is reused across overlapping rescans and destroyed when its last controller is released.

The update path blocks writes while the official RGB Tool is running, suppresses identical repeated requests, spaces update starts by at least 250 ms, and acquires a named per-PCI mutex so two cooperating OpenRGB/probe processes cannot write the same board simultaneously. This limit is a precaution, not measured Ambient compatibility. Close RGB Tool before using this device.

## Hardware results on this PC, 2026-10-26 (Europe/Paris)

The standalone native program includes the **same production transport and packet builders** as the RGBController. It does not use the official UI, Frida, Chroma or Synapse. Target: PCI 3:0:0, ADL index 5, exact IDs above. Do not hard-code this index on another PC/session.

| Operation | Native replies | User visual confirmation |
| --- | --- | --- |
| Blue static, maximum | All valid | Blue fixed |
| Green static, maximum | All valid | Green fixed |
| Off | All valid | Completely off |
| Red static, low (33 hex brightness) | All valid | Dim red fixed |
| Red static, maximum | All valid | Bright red fixed |

Final state: red static at maximum brightness. Earlier experimental invocations before the complete initialization was implemented failed at the first invalid read/acknowledgement and stopped. ADL2 SMU and missing-identification variants are not shipped as fallback writers. Adding the official identity query before activation produced successful complete exchanges; this is observed evidence, not proof that it was the only relevant initialization difference.

The complete OpenRGB GUI/SDK integration is **not yet hardware-validated**: no full Qt/MSVC build was available here. Native transport/packet testing is distinct from testing the installed application. No installed binary, profile, plugin or Visual Map setting has been replaced.

## Verification performed

- The production Windows controller compiled successfully to a Windows object using portable Zig/Clang 0.16.0 and the real OpenRGB/ADL/Windows headers.
- `ProtocolTests.cpp` compiled and ran successfully. Golden fixtures are complete messages extracted from the official captures. Tests check red/green/blue/low brightness, mode on/off, apply, activation, identity, brightness bounds, all six transfer failure positions, all reply marker/payload corruption positions, buffer continuity and identity validation.
- The native hardware probe compiled and ran as described above.
- Full OpenRGB Qt/MSVC build and `test-modern.cmd` remain for the build PC. A successful object compile is not a full application build.

## One build on the other PC

1. Pull `feature/fluent-interface` with `git pull --ff-only origin feature/fluent-interface` after preserving any local work.
2. Run **`scripts\CompileModern.cmd`** in the usual Qt/MSVC environment. It now first runs `scripts\test-radeon-reference.cmd`, which builds/runs the protocol tests and builds the optional probe, without executing hardware commands. It then runs qmake (required to discover the newly added controller), compiles OpenRGB and performs the existing build-isolation check.
3. Run `scripts\test-modern.cmd` for the Fluent interface regression checks. Retain the build logs.
4. Bring the newly built OpenRGB runtime to this PC. With OpenRGB closed, first make dated backups of both the complete installed application directory and the actual configuration directory, as documented in `WARNING-FIX.md`. Preserve existing plugins/profiles/Visual Map. No configuration reset is required.
5. Close RGB Tool, start the new build, and verify that exactly one **AMD Radeon RX 7900 XTX Reference / Avatar** appears. Test Static RGB/brightness and Off, then reopen a saved profile and confirm API-5 plugins load. Also verify the warning-dialog suppression after restart.

Optional native probe (no OpenRGB build required, but both OpenRGB and RGB Tool must be closed): `build\radeon-tests\NativeProbe.exe blue` (or `red`, `green`, `off`, `red-low`). It checks exact PCI IDs and process conflicts before writing and stops on the first error. It is a finite manual test, not an animation service. If the GPU stops responding, inspect the log and stop; do not loop retries. The previously successful full power-removal recovery remains an observation, not an automatic recovery step.

To roll back a deployment, close OpenRGB and restore the dated application backup; restore the saved configuration if needed. To disable just this controller, uncheck its named detector and restart/rescan. The source and probe do not modify firmware.
