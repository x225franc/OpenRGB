# Visual Map stability correction — 2026-10-11

Verified defects:
- The virtual device thread read `setup.matrix_map` and `led_routes` while the Qt thread resized/rebuilt them. A recursive state mutex now covers reads, writes and nested rendering.
- Device-list callbacks touched the tab list outside its Qt thread. Pause and rebuild now execute synchronously on the owning thread, with direct invocation when already there.
- `active_state` recorded a zone when it was added but did not update that entry after moving or scaling it. Recreate therefore restored stale geometry. Live state now replaces matching entries before reconstruction or detection, retaining unavailable devices.
- Failed file opens were reported as successful saves. QSaveFile now performs atomic writes and returns actual failure. Explicit Save shows a failure dialog.
- Edits to an already saved/loaded map are now saved after a 300 ms quiet interval. Restore does not trigger writes. New unnamed maps still need their first Save.

Validation: the old tab implementation fails the geometry preservation regression. The corrected implementation passes geometry and detection-pause preservation, 500 concurrent resize/recreate iterations, save/load roundtrip, rejected directory-target writes, debounced autosave and persistent Clear.

Build: Qt 6.8.3 MSVC x64, Release, API 5, using the exact installed host SDK interfaces. Existing size conversion warnings remain.

Rebuild the plugin normally with qmake / jom. To build tests, run qmake VisualMapRegression.pro in a separate build directory, then jom. Run VisualMapRegression.exe with Qt on PATH and QT_QPA_PLATFORM=offscreen. FakeHost never enumerates or writes physical devices.

Remaining validation: exercise Ambient at 60 FPS while editing a real map, reconnecting devices, rescanning and exiting. Passing the isolated regression is not proof that every historical native crash has the same cause.
