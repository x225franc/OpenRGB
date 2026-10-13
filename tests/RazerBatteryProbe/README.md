# Bluetooth battery probe

Read-only Windows 11 diagnostic for the paired BlackWidow V3 Mini and BSKV3P 35K.
Build: `dotnet build RazerBatteryProbe.csproj -c Release`.
Run the resulting EXE in the interactive user's session with both devices awake.
It reads Battery Service 180F / characteristic 2A19 with an uncached value read.
It sends no vendor commands and does not change the lighting.
See `Documentation/RazerBattery.md` for the integrated implementation and tests.
