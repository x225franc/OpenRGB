# RX 7900 XTX reference / Avatar investigation

Investigation started 2026-10-25. A native Windows controller has now been implemented; see **[NATIVE-CONTROLLER.md](NATIVE-CONTROLLER.md)** for the current capabilities, successful standalone native hardware tests, and remaining full-application validation. Earlier sections below record the investigation before that implementation.

## Hardware observed on this PC

- GPU: AMD Radeon RX 7900 XTX.
- PCI instance: `PCI\VEN_1002&DEV_744C&SUBSYS_0E3B1002&REV_C8\6&13FCA301&0&00000009`.
- Vendor/device: `1002:744C`; subsystem vendor/device: `1002:0E3B`; revision `C8`.
- Windows driver: `32.0.32015.7029`.
- A Radeon 780M is also present. Never assume ADL adapter index zero identifies the discrete GPU.
- No present USB device with Cooler Master VID `2516` appeared in the PnP inventory. This does not, by itself, prove the transport.
- These PCI IDs match the reference board in upstream issue 3037. They do not independently identify the Avatar cosmetic edition or the RGB controller silicon.

## Official application

Directory: `C:\Program Files (x86)\AMD\AMD Radeon RX 7900 XTX Series RGB LED`.

Four executable files expose Windows file/product version `1.0.0.1`; this is executable metadata, not a verified UI release number.

| Executable | SHA256 |
| --- | --- |
| `AMD_Radeon_RX_7900_XTX_Series_RGB_LED.exe` | `3E5587C5DBF5A94952A59A5B4C190D73A738CD676E6ED898DC26FB0BA11765D3` |
| `AMD_Radeon_RX_7900_XTX_Series_RGB_LED_HID.exe` | `6E12EFC6D3A663CCED3BAB56FA57C817BBE99036D12880FCFFC7AF14F4D9E823` |
| `AMD_Radeon_RX_7900_XTX_Series_RGB_LED_HID_x64.exe` | `317917E3982240DEAFFC4A7056831637D23A704745BE8481CC47CBFE194666E5` |
| `AMD_Radeon_RX_7900_XTX_Series_RGB_LED_x64.exe` | `D13F73B7D61C68EC88E4AB52C7F4E4851A5450A79899C25BE01415DDE01CF2F1` |

The running HID_x64 process (PID 16120 at inventory time) loads `C:\Windows\System32\atiadlxx.dll` and `HID.DLL`. Both x64 executable images contain:

- `ADL_Display_WriteAndReadSMUI2C` and ADL/ADL2 initialization symbols.
- `ADL2_Adapter_KeepAwake_Start/Stop` and `ADL2_Device_PMLog_Device_Create/Destroy`.
- Diagnostic strings naming ADLI2C write address `0xb4`, read address `0xb5`, speed `100`, and buffers `i2cData[0]~[24]`.
- `RzChromaBroadcastAPI64.dll` (not observed among loaded modules during inventory).
- A diagnostic path template `%s\Navi debug.log`. No existing log was found in the inspected AMD install, ProgramData AMD and Administrator VirtualStore AMD trees. Other locations remain possible.

`0xb4/0xb5` conventionally represent seven-bit address `0x5a` plus direction. This remains an interpretation until runtime calls confirm the layout. The filename suffix HID does not establish USB as the active transport.

The installation also contains FWUpdate executables. They have not been launched.

## Existing OpenRGB support

Local branch: `feature/fluent-interface`, initial commit `151c821e19c50532045e82ec93b920cfba303b8a`, initially clean. Plugin API macro remains 5.

The local Cooler Master detector registers Radeon 6000/6900 via HID. The upstream master detector inspected online also contains those GPU registrations, without a reference 7900 XTX entry. Partner-board 7900 controllers in this repository are not evidence of reference-board compatibility.

The existing Windows AMD backend uses `ADL2_Display_WriteAndReadI2C`, with I2C line 1. It does not implement the SMU entry point. A native reference-board implementation must establish the SMU ABI, adapter selection, initialization/lifetime requirements, packet framing and acknowledgements. Do not substitute the existing ADL or Radeon 6000 HID path blindly.

Sources:

- https://raw.githubusercontent.com/CalcProgrammer1/OpenRGB/master/Controllers/CoolerMasterController/CoolerMasterControllerDetect.cpp
- https://gitlab.com/CalcProgrammer1/OpenRGB/-/work_items/3037

The issue description links SMU/I2C comments/captures. GitLab's public notes API returned HTTP 401 in this session; those comments have not been fully reviewed. Searches did not establish a ready-to-use implementation.

## Observation tool and sequence

`capture_adl.py` attaches Frida interceptors to the official application's AMD ADL exports, including when the AMD DLL loads later. It logs arguments, data before/after calls, return codes and UTC timestamps as JSONL. It never invokes an ADL function or changes arguments/return values. Frida instrumentation itself modifies process code temporarily; detaching removes the instrumentation. The optional `--launch` mode starts the official application and observes its child processes; the official application can itself send device commands during normal startup.

Python dependency: `frida`. Temporary local analysis dependencies were downloaded into `tmp-rx7900-tools` (frida 17.24.3, pefile 2024.8.26, capstone 5.0.9). They are investigation tools, not runtime dependencies of OpenRGB.

Example (substitute the current official PID):

```powershell
python tests\RX7900Reference\capture_adl.py --pid 16120 --output tests\RX7900Reference\capture-session.jsonl --tools tmp-rx7900-tools --seconds 600
```

Close OpenRGB normally before applying official changes. Capture red, green, blue, off, red at low brightness, red at maximum brightness with three seconds between steps; record visual confirmation. A successful ADL return alone is not proof of visible hardware behavior.

The first attachment recorded successful hook installation but no ADL transactions. The user reopened the launcher, which displayed **Plug in your AMD Device to get started** and created new main/auxiliary PIDs (10408/14460). The old attachment cannot observe a replacement process. The user confirms that RGB Tool previously controlled this card; current non-detection needs investigation before color testing. No color test has been completed in this session.

To capture initialization, close the official application and its notification icon normally, keep OpenRGB closed, then use:

```powershell
python tests\RX7900Reference\capture_adl.py --launch "C:\Program Files (x86)\AMD\AMD Radeon RX 7900 XTX Series RGB LED\AMD_Radeon_RX_7900_XTX_Series_RGB_LED_x64.exe" --output tests\RX7900Reference\capture-startup.jsonl --tools tmp-rx7900-tools --seconds 120
```

This launches the interactive official UI. It does not bypass detection or synthesize RGB commands. Only run it once previous official processes are closed, to prevent competing instances.

## Integration criteria and rollback

Priority is a narrowly matched native controller. Static/brightness observations alone cannot justify per-LED control, a Direct mode, or Ambient update rates. Establish whether apply commands persist settings before repeated updates. An Ambient/Visual Map implementation requires a confirmed volatile update path and measured behavior.

Chroma Broadcast is an alternate software path to investigate, not a confirmed OpenRGB interface. It would likely retain the official receiver and a compatible Chroma provider/runtime; exact installed/running dependencies and update behavior remain unverified. No standalone Cooler Master SDK was found in the install directory.

No OpenRGB executable or settings have been replaced. No profiles, plugins, Visual Map data, firmware or device registers were changed by the investigation tools. There is no binary rollback required at this stage. Before any future replacement, copy the complete deployed OpenRGB directory and its actual configuration directory to a dated backup; restore both with OpenRGB closed if rollback is required. Do not overwrite or reset existing configuration during testing.

Compilation: no production code changes, no OpenRGB build performed. The Python investigation scripts have been syntax checked. No native OpenRGB color test has been performed.

## Captured startup after reboot

The official x64 UI was launched once under observation, with no other RGB processes in the preceding inventory. The observer detached after 90 seconds. The official UI remains a normal running application. The user confirms it still shows **Plug in your AMD Device to get started**.

- RX 7900 XTX: Windows device status OK, error code 0, driver unchanged (`32.0.32015.7029`).
- Radeon 780M: disabled (Windows error code 22); no attempt was made to enable it.
- Loaded AMD ADL file version: `7.26.20.1616`.
- ADL initialization and adapter enumeration return 0 (success), reporting ten logical adapters.
- Read-only ADL metadata enumeration (`inspect_adl.py`) maps indices 0–4 to the 780M and 5–9 to the same RX 7900 XTX at PCI bus 3/device 0/function 0. Indices must be discovered again in future sessions.

`capture-startup.jsonl` contains actual calls made by the official application:

| Field | Observed value |
| --- | --- |
| API | `ADL_Display_WriteAndReadSMUI2C` |
| ADLI2C structure size | 40 bytes (x64) |
| Line / offset | 0 / 0 |
| Speed | 100 kHz |
| Write address / action | `0xB4` / 2 |
| Read address / action | `0xB5` / 1 |

The application sends a detection message beginning `12 02`, wrapped in three write transactions: `40` + 23 payload bytes (24 total), `57` + next 23 bytes (24 total), `AE` + remaining 18 bytes (19 total). Static analysis of the official sender at RVA `0x55D10` confirms this framing for a 64-byte message.

Captured results: indices 0–4 fail the first transaction. For index 5, `40` and `57` writes return 0 but `AE` returns -1. Index 6 fails at `57`. Index 7 completes all three writes with return 0, then its `0xB5` read returns -1. Index 8 fails at `AE`, index 9 at `57`. The official application stops each sequence at its failure. These results establish actual SMU/I2C API use against the discrete GPU, but not a successful controller identification or visible RGB operation. A successful driver write is not proof of device acknowledgement. The root cause of the failures is not established.

Unlike the initial read-only inventory, launching the official application did cause the application's own detection writes. The observer did not generate or alter them. No synthetic color command, arbitrary address scan, firmware tool, or register write was performed by our scripts.

Static disassembly (`inspect_binary.py`, requiring pefile and capstone) also found candidate color-related constructors, including a `51 9C` message packing two 24-bit values into three byte components each and other mode/configuration messages. Their initialization, apply/commit requirements, persistence and behavior on this particular board remain unvalidated. They are not exposed as a test writer. The hardware controller silicon identity, per-LED support and sustainable update frequency remain unknown.

Next prerequisite for controlled color tests: establish a complete color update sequence and distinguish transient updates from persistent configuration, or restore successful official detection so those sequences can be captured. User visual confirmation can validate a deliberate color test; it cannot identify side effects of arbitrary packets.

## Successful official color capture after full power removal

The user reports successful official detection after shutdown and removal of mains power. This recovery is observed; the exact cause of the earlier failure remains unknown. The official x64 UI (PID 17276) was observed using the same SMU API, line 0, speed 100 kHz, adapter 5, addresses B4/B5. OpenRGB was subsequently verified no longer running. Initial setup changes before the user's confirmation of closing OpenRGB are included in the raw trace and should not be treated as isolated experiments.

The user visually confirmed, in order: red #FF0000 static at maximum brightness; green #00FF00; blue #0000FF; Light Off with complete extinction; red at the first nonzero brightness step, visibly dimmer; and red restored to maximum brightness. Chroma Sync Off was requested throughout. Final state is red static at maximum brightness. These are tests through the official application, not through native OpenRGB code.

Files: `capture-colors.jsonl` is the raw trace; `captured-messages.json` reconstructs the outgoing 64-byte messages using `summarize_capture.py`. The final trace contains 1,872 completed ADL transactions, all returning 0, and 312 complete outgoing messages. The official application sends multiple configuration messages per user action, including when navigating and changing controls; these counts are not color-update throughput measurements.

The observed static messages begin:

```text
Red max:   51 2C 01 00 00 FF 00 FF FF FF FF 00 00 00 00 00 ...
Green max: 51 2C 01 00 00 FF 00 FF FF FF 00 FF 00 00 00 00 ...
Blue max:  51 2C 01 00 00 FF 00 FF FF FF 00 00 FF 00 00 00 ...
Red low:   51 2C 01 00 00 FF 00 FF FF 33 FF 00 00 00 00 00 ...
```

Within this message, zero-based byte 9 tracks brightness (observed steps 33, 66, 99, CC, FF hexadecimal) and bytes 10/11/12 track R/G/B. Static mode was observed with byte 4 = 00. Mode/configuration `51 A0` and apply-like `51 28 ... E0` messages are also present. Reads successfully return a leading FF frame echoing the message, followed by E8 and D1 frames. This establishes more than a successful driver write, but does not prove persistence semantics. The full raw trace is authoritative; abbreviated examples are not standalone commands to replay.

Additional messages, including `50 55`, occur in the official Apply flow. Their persistence and initialization semantics have not been established, so this full flow must not be reused as an Ambient loop. No per-LED operation, Direct mode, independent-zone capability, native OpenRGB operation or sustained update frequency has been validated.

## Separate warning-dialog fix

The user also reported that the OpenRGB I2C/SMBus warning reappears despite selecting Don't show again. The root cause in this checkout is missing `LogManager.dialog_no_show_hashes` schema registration: settings filtering discards the suppression list. Sources now register this array and use the final checkbox state. See `WARNING-FIX.md` for build, validation, backup and rollback instructions. The user chose compilation on the other PC; no application binary or personal configuration was replaced here. The patch package is at `.build/OpenRGB-dialog-persistence-fix.zip`. Compilation and runtime validation of this fix remain pending on that build PC.
