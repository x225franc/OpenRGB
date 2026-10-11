# HID lifecycle concurrency regression (Windows x64)

Uses the shipped hidapi-hotplug DLL. Only enumerates a nonexistent VID/PID and initializes HIDAPI; never sends commands to a device.

From an x64 MSVC developer prompt at the repository root:

```cmd
cl /nologo /EHsc /std:c++17 /I. /Idependencies\hidapi-hotplug-win\include tests\HidLifecycle\HidLifecycleRegression.cpp /Fe:build\HidLifecycleRegression.exe /Fo:build\HidLifecycleRegression.obj /link dependencies\hidapi-hotplug-win\x64\hidapi-hotplug.lib
copy dependencies\hidapi-hotplug-win\x64\hidapi-hotplug.dll build\
build\HidLifecycleRegression.exe
```

The default path performs 600 enumerations on four threads concurrently with repeated initialization, using the shared lifecycle guard. An extra `raw` argument intentionally bypasses the guard for a negative control; run that only under a debugger because it reproduces an invalid free/heap corruption with the current dependency.

HIDAPI lifecycle operations share global initialization/error storage, independently of per-device transport mutexes. The application guard does not serialize RGB frame writes.
