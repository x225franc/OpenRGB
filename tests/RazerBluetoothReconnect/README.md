# Bluetooth reconnect regression

The generator compiles the actual production BluetoothWorker class against a fake GATT transport. It tests short connection edges with no new frame, identical pending frames, recovery after a failed send, latest scene preservation, no ownership takeover, and shutdown during backoff. It does not emulate Windows GATT notifications or certify actual hardware recovery.

From the repository root in an MSVC x64 developer prompt:

```
python tests/RazerBluetoothReconnect/GenerateWorkerTest.py build/bluetooth-reconnect-test.cpp
cl /nologo /EHsc /std:c++17 /O2 /MD build/bluetooth-reconnect-test.cpp /Fe:build/bluetooth-reconnect-test.exe
.\build\bluetooth-reconnect-test.exe
```

An optional second argument selects a previous controller source for a before/after regression check.

The mouse test also simulates a silent hardware lighting reset while Windows continues to report the transport as connected. The worker must read back and restore both zones without any new requested frame.
