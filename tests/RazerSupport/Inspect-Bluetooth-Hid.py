# SPDX-License-Identifier: GPL-2.0-or-later
"""Read HID report lengths for the paired Razer Bluetooth devices; no reports are written."""
import ctypes as c
import contextlib
import io
import json
import pathlib
import runpy
import sys
inventory_script = pathlib.Path(__file__).with_name("Inspect-Razer.py")
sys.argv = [str(inventory_script)]
with contextlib.redirect_stdout(io.StringIO()):
    rows = runpy.run_path(str(inventory_script))["rows"]
kernel = c.WinDLL("kernel32", use_last_error=True)
hid = c.WinDLL("hid", use_last_error=True)
kernel.CreateFileW.argtypes = [c.c_wchar_p, c.c_uint32, c.c_uint32, c.c_void_p, c.c_uint32, c.c_uint32, c.c_void_p]
kernel.CreateFileW.restype = c.c_void_p
kernel.CloseHandle.argtypes = [c.c_void_p]
hid.HidD_GetPreparsedData.argtypes = [c.c_void_p, c.POINTER(c.c_void_p)]
hid.HidD_FreePreparsedData.argtypes = [c.c_void_p]
class Caps(c.Structure):
    _fields_ = [("usage", c.c_ushort), ("page", c.c_ushort), ("input", c.c_ushort),
                ("output", c.c_ushort), ("feature", c.c_ushort), ("reserved", c.c_ushort * 17), ("counts", c.c_ushort * 10)]
hid.HidP_GetCaps.argtypes = [c.c_void_p, c.POINTER(Caps)]
hid.HidP_GetCaps.restype = c.c_uint32
result = []
for row in rows:
    if row["bus_type"] != 2 or (row["vid"], row["pid"]) not in [("1532", "0272"), ("068E", "00D8")]:
        continue
    item = {key: row[key] for key in ["vid", "pid", "product", "usage_page", "usage"]}
    device = kernel.CreateFileW(row["path"], 0, 3, None, 3, 0, None)
    if device in (None, c.c_void_p(-1).value):
        item["open_error"] = c.get_last_error()
    else:
        preparsed = c.c_void_p()
        try:
            if hid.HidD_GetPreparsedData(device, c.byref(preparsed)):
                caps = Caps()
                status = hid.HidP_GetCaps(preparsed, c.byref(caps))
                item.update(input_bytes=caps.input, output_bytes=caps.output, feature_bytes=caps.feature, caps_status=hex(status))
            else:
                item["caps_error"] = c.get_last_error()
        finally:
            if preparsed: hid.HidD_FreePreparsedData(preparsed)
            kernel.CloseHandle(device)
    result.append(item)
print(json.dumps(result, indent=2))
