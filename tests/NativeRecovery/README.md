# Native Windows recovery tests

Run `Run-Tests.cmd` on the Windows build machine (MSVC 18, Qt 6.8.3 x64). The first executable launches only isolated copies of itself, simulates exception-style exit codes, and verifies replacement, normal exit, explicit cancellation, a three-restart budget and Windows argument quoting. It never terminates installed OpenRGB. A readiness handshake ensures the observer validates its parent before the parent can exit.

The Qt test uses the production Windows event listener with a QCoreApplication and injected native event records. It verifies that suspend/resume callbacks are outside the native filter, display settling delays resume, duplicates coalesce, session end cancels pending work and destruction cancels callbacks. These tests do not put Windows to sleep, hibernate, restart the machine or manipulate hardware.

Real sleep/hibernate cycles and logon remain manual validation after installation. An exception inside Qt screen handling has not been reproduced or conclusively diagnosed by these synthetic tests. See Documentation/NATIVE-RESUME-RECOVERY.md for runtime behavior and limits.
