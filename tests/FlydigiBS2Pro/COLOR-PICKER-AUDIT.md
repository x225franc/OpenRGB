# BS2 Pro manufacturer color-picker audit — 2026-11-07

Scope: local, read-only analysis after Windows UI Automation exposed only Chromium panes and no editable RGB controls. No mouse, keyboard, focus, or cooler-setting changes were performed during this audit. The preceding mouse experiment did not produce a validated color-change capture and is not counted as a hardware result.

## Observed application path

The installed renderer asset `.vite/renderer/main_window/assets/index-CGwMero6.js` wires the palette component `Bt` through `onChange` to `useCoolerLedConfig` (minified hook `dt`). Its color-change callback updates the local RGB configuration and calls the same scheduled send as brightness, speed, and effect changes. The scheduler groups edits with a 100 ms timeout. Its sender builds a six-LED RGB configuration and calls the selected adapter's `setRgbConfig`; additional 200/300 ms checks and an in-flight guard limit submissions.

The WebHID adapter converts the RGB configuration into the firmware table. `CoolerDevice.setRgbConfig` sends prepare 0x41, indexed chunks 0x47, then finalization 0x43. The IPC adapter sends `IpcCommandEnum_SetRgbConfig` with a configuration object to the manufacturer service; the service's internal implementation was not established by this static renderer audit. Prior hardware captures and RAM-only tests remain separate evidence documented in DIRECT-INVESTIGATION.md.

## Result and limits

No separate temporary color-preview or user-defined palette-bank selector was found in the inspected color-picker path. This does not prove that no undocumented firmware command exists. It does mean the color wheel alone supplies no demonstrated way to preload many fixed colors and switch between them without persistent application. Do not treat the 100 ms UI debounce as a safe continuous flash-write rate. Do not implement arbitrary-color Ambient by repeatedly finalizing native tables.

The experimental Direct mode still maps input to the four previously owner-confirmed firmware presets, three of which animate. It does not meet the requested broad fixed-color palette. No new physical color-picker behavior or fixed-palette bank has been validated by this audit.

Local asset SHA-256: `aeae5a7634f7de411ac331d32bca1a1a7bf1f535306bf92a2b7c09055050cdf1`.

Manufacturer source stays under ignored build/ diagnostics and is not copied into tracked source.

## Subsequent controlled interaction capture

At 15:40:31–15:40:36 UTC on 2026-11-07, window-targeted mouse messages opened the actual Space Station color wheel and performed a single short drag. Unlike the previous failed global-coordinate attempts, this interaction produced a complete RGB transfer. No RGB outbound reports were captured before release (15:40:34.182 UTC). Prepare 0x41 began at 15:40:34.315, followed by 19 indexed 0x47 chunks and finalization 0x43 at 15:40:34.438. Duplicate hooks produced 42 observations of 21 distinct command/report pairs, not two independent uploads.

This bounded test provides no evidence of a temporary color-preview command or palette-bank selection. It shows the manufacturer finalizing the RGB table after release in this interaction. The uploaded first color is magenta (255,0,255), matching the initial selection. This validates a configuration reapply, not a distinct final hue or a visually confirmed color transition. The physical LED response was not independently observed by the agent; do not claim visual confirmation. See COLOR-PICKER-CAPTURE.json for timings and decoded payload fields. No fan-control command was sent by the test.

A subsequent single click on the green palette point changed the actual UI from #ff00ff to #00ff00. Its captured table contains first RGB [0,255,0] and again uses prepare 0x41, 19 indexed 0x47 chunks, and finalize 0x43. Thus a distinct custom color was confirmed both in the manufacturer UI and outbound payload. Physical LED appearance was not independently observed. No preview-only command was observed in either interaction.
