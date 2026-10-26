# Razer and Radeon lighting modes — 2026-10-26

## Hardware effects and software effects

A hardware effect is sent once and continues inside the device after OpenRGB exits. A software effect needs OpenRGB running and periodically sends colors. Razer Bluetooth uses native effects only: the experimental software breathing was removed because its observed animation was choppy.

## Razer BlackWidow V3 Mini, Bluetooth

Static, Off and Wave remain available at their original profile indices. Added native Breathing, Spectrum Cycle, Reactive and Starlight. The latter modes match states read from the keyboard after its Fn shortcuts; the user confirmed the effects on hardware. Breathing was also confirmed through OpenRGB itself. Bluetooth exposes the whole keyboard as one group, not individual keys or Direct streaming.

## Razer Basilisk V3 Pro 35K Phantom Green, Bluetooth

Static and Off retain their original profile indices. Added native Breathing, Spectrum Cycle, Wave and Reactive. Each group is independently enabled through its per-LED color: black sends Off with zero brightness to that group. Breathing and Spectrum Cycle were visually confirmed on the wheel with the underglow off, and Wave on the underglow with the wheel off. The final signed OpenRGB executable also passed the native Bluetooth Breathing test: the user confirmed smooth wheel breathing with the underglow off. The probe exited after sending each effect; the animation continued autonomously.

Reactive was accepted and read back but its visible behavior is NOT validated: the user's Smart Dimming turns off lighting while moving and masks the observation. The tested Starlight command was rejected with vendor status 5, so Starlight is NOT exposed for this mouse. This does not prove that every possible Starlight command is unsupported. Fire was deliberately left out at the user's request, as was Audio Meter.

Initial brightness is read from the brightest group rather than always the wheel, so wheel-off/underglow-on is represented correctly. Reconnection retains the latest requested native scene. Existing battery polling remains every five minutes with manual refresh; Smart Dimming, sensor polling and sleep settings are untouched.

## Basilisk Phantom Green, USB / HyperSpeed

Added native Breathing at the END of the existing mode list to preserve saved mode indices. A one-shot extended-matrix Breathing command (class 0F, command 02, green, transaction 1F on the shared receiver) was accepted and visually confirmed fluid on HyperSpeed after the test tool exited. The final OpenRGB controller was then tested through its SDK and visually confirmed fluid as well. No continuous software animation is used. The corresponding wired PID uses the same native path but has NOT been visually retested in this session. Other Basilisk variants retain their previous capability declarations.

## AMD RX 7900 XTX reference / Avatar

The pulled native driver already has hardware-tested Static and Off on the other PC. Added Direct, Breathing (software), and Color Cycle (software), using the known volatile static/apply protocol and ONE global RGB zone. No guessed firmware effect codes or persistent-save frames are sent. Continuous updates are capped at four starts per second; repeated unchanged states are skipped. A background worker handles software effects and is stopped before changing to a native mode or destroying the controller. RGB Tool must be closed.

IMPORTANT: the GPU is on another PC. Direct and sustained software animation have NOT been hardware-validated here; the four-Hz limit is a conservative implementation limit, not a certified hardware rating. Smoothness and stability require testing there. The separate firmware investigation is described in tests/RX7900Reference/HANDOFF-FIRMWARE-MODES.md.

## Reproducible checks

- scripts/CompileModern.cmd: Release x64 build and header dependency check.
- tests/LightingModes/LightingModesTests.cpp: captured packet formats, mode decoding, timing, black masks and worker shutdown.
- tests/RazerBluetoothReconnect/GenerateWorkerTest.py: actual production worker against a fake transport (18 checks).
- scripts/test-modern.cmd: existing Qt navigation tests (17 checks).
- scripts/test-radeon-reference.cmd: captured ADL messages, failure handling and acknowledgement validation; does not write to a GPU.
- tests/RazerSupport/Test-Bluetooth-SDK.py: isolated SDK server, explicit device/mode selection; --include-hyperspeed additionally permits only the verified shared Phantom Green controller. SDK acceptance alone is not visual hardware validation.

Hardware probe logs and compiler/test logs are included in the delivery validation directory. No claim is made about hardware modes inferred only from Synapse's Quick Effects menu.

The OpenRGB controller selects one mode for the mouse, with independent per-zone colors/enabling. Two different simultaneous modes per zone are not exposed by the current UI. Wave is spatially meaningful on the underglow rather than the single wheel group.
