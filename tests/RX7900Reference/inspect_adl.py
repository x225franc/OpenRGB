"""Read AMD ADL adapter metadata. No I2C/SMU/device register calls."""
import ctypes as c
import json


class AdapterInfo(c.Structure):
    _fields_ = [('size', c.c_int), ('index', c.c_int), ('udid', c.c_char * 256),
                ('bus', c.c_int), ('device', c.c_int), ('function', c.c_int),
                ('vendor', c.c_int), ('name', c.c_char * 256),
                ('display', c.c_char * 256), ('present', c.c_int),
                ('exist', c.c_int), ('driver', c.c_char * 256),
                ('driver_ext', c.c_char * 256), ('pnp', c.c_char * 256),
                ('os_display', c.c_int)]


if __name__ == '__main__':
    adl = c.CDLL(r'C:\Windows\System32\atiadlxx.dll')
    crt = c.CDLL('msvcrt')
    crt.malloc.argtypes = [c.c_size_t]
    crt.malloc.restype = c.c_void_p
    callback_type = c.CFUNCTYPE(c.c_void_p, c.c_int)
    allocate = callback_type(crt.malloc)
    adl.ADL_Main_Control_Create.argtypes = [callback_type, c.c_int]
    adl.ADL_Main_Control_Destroy.argtypes = []
    adl.ADL_Adapter_NumberOfAdapters_Get.argtypes = [c.POINTER(c.c_int)]
    adl.ADL_Adapter_AdapterInfo_Get.argtypes = [c.POINTER(AdapterInfo), c.c_int]
    result = adl.ADL_Main_Control_Create(allocate, 1)
    if result != 0:
        raise RuntimeError(f'ADL initialization failed: {result}')
    try:
        count = c.c_int()
        result = adl.ADL_Adapter_NumberOfAdapters_Get(c.byref(count))
        if result != 0 or not 0 < count.value <= 128:
            raise RuntimeError(f'Invalid adapter count: {result}, {count.value}')
        adapters = (AdapterInfo * count.value)()
        for adapter in adapters:
            adapter.size = c.sizeof(AdapterInfo)
        result = adl.ADL_Adapter_AdapterInfo_Get(adapters, c.sizeof(adapters))
        if result != 0:
            raise RuntimeError(f'ADL adapter query failed: {result}')
        records = []
        for adapter in adapters:
            record = {}
            for field, _ in AdapterInfo._fields_:
                value = getattr(adapter, field)
                record[field] = value.decode('utf-8', errors='replace') if isinstance(value, bytes) else value
            records.append(record)
        print(json.dumps(records, indent=2))
    finally:
        adl.ADL_Main_Control_Destroy()
