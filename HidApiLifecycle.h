/* HIDAPI's process-global initialization/error state must not be accessed concurrently. */
#pragma once
#include <hidapi.h>
#include <mutex>

namespace HidApiLifecycle
{
inline std::recursive_mutex& Mutex()
{
    static std::recursive_mutex mutex;
    return mutex;
}
inline int Init() { std::lock_guard<std::recursive_mutex> guard(Mutex()); return hid_init(); }
inline int Exit() { std::lock_guard<std::recursive_mutex> guard(Mutex()); return hid_exit(); }
inline hid_device_info* Enumerate(unsigned short vendor, unsigned short product)
{
    std::lock_guard<std::recursive_mutex> guard(Mutex());
    return hid_enumerate(vendor, product);
}
inline hid_device* OpenPath(const char* path)
{
    std::lock_guard<std::recursive_mutex> guard(Mutex());
    return hid_open_path(path);
}
inline void Close(hid_device* device)
{
    std::lock_guard<std::recursive_mutex> guard(Mutex());
    hid_close(device);
}
}
