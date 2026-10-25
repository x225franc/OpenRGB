/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "RadeonReferenceProtocol.h"
#include "adl_sdk.h"
#include <windows.h>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>
#include <mutex>

class RadeonReferenceDeviceLock
{
    HANDLE handle = nullptr;
    bool locked = false;
public:
    explicit RadeonReferenceDeviceLock(const std::string& pci)
    {
        const std::wstring name = L"Global\\OpenRGB_AMDReferenceRGB_" + std::wstring(pci.begin(), pci.end());
        handle = CreateMutexW(nullptr, FALSE, name.c_str());
        if(handle)
        {
            const DWORD result = WaitForSingleObject(handle, 0);
            locked = result == WAIT_OBJECT_0 || result == WAIT_ABANDONED;
        }
    }
    ~RadeonReferenceDeviceLock()
    {
        if(locked) ReleaseMutex(handle);
        if(handle) CloseHandle(handle);
    }
    bool Acquired() const { return locked; }
    RadeonReferenceDeviceLock(const RadeonReferenceDeviceLock&) = delete;
    RadeonReferenceDeviceLock& operator=(const RadeonReferenceDeviceLock&) = delete;
};
class RadeonReferenceADL
{
public:
    RadeonReferenceADL() = default;
    RadeonReferenceADL(const RadeonReferenceADL&) = delete;
    RadeonReferenceADL& operator=(const RadeonReferenceADL&) = delete;
    static void* __stdcall Allocate(int size) { return std::malloc(size); }
    using Create = int (*)(ADL_MAIN_MALLOC_CALLBACK, int);
    using Destroy = int (*)();
    using Count = int (*)(int*);
    using Info = int (*)(AdapterInfo*, int);
    using Transfer = int (*)(int, ADLI2C*);
    using Create2 = int (*)(ADL_MAIN_MALLOC_CALLBACK, int, ADL_CONTEXT_HANDLE*);
    using Destroy2 = int (*)(ADL_CONTEXT_HANDLE);
    using CreateDevice = int (*)(ADL_CONTEXT_HANDLE, int, ADL_D3DKMT_HANDLE*);
    using DestroyDevice = int (*)(ADL_CONTEXT_HANDLE, ADL_D3DKMT_HANDLE);
    using Awake = int (*)(ADL_CONTEXT_HANDLE, int, ADL_D3DKMT_HANDLE);
    HMODULE library = nullptr;
    bool initialized = false;
    Destroy destroy = nullptr;
    Count count = nullptr;
    Info info = nullptr;
    Transfer transfer = nullptr;
    std::vector<std::string> trace;
    ADL_CONTEXT_HANDLE context2 = nullptr;
    Destroy2 destroy2 = nullptr;
    CreateDevice create_device = nullptr;
    DestroyDevice destroy_device = nullptr;
    Awake awake_start = nullptr;
    Awake awake_stop = nullptr;
    std::mutex transfer_mutex;

    bool Initialize()
    {
        library = LoadLibraryExW(L"atiadlxx.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if(!library) library = LoadLibraryExW(L"atiadlxy.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if(!library) return false;
        auto create = reinterpret_cast<Create>(GetProcAddress(library, "ADL_Main_Control_Create"));
        destroy = reinterpret_cast<Destroy>(GetProcAddress(library, "ADL_Main_Control_Destroy"));
        count = reinterpret_cast<Count>(GetProcAddress(library, "ADL_Adapter_NumberOfAdapters_Get"));
        info = reinterpret_cast<Info>(GetProcAddress(library, "ADL_Adapter_AdapterInfo_Get"));
        transfer = reinterpret_cast<Transfer>(GetProcAddress(library, "ADL_Display_WriteAndReadSMUI2C"));
        initialized = create && destroy && count && info && transfer && create(Allocate, 1) == ADL_OK;
        auto create2 = reinterpret_cast<Create2>(GetProcAddress(library, "ADL2_Main_Control_Create"));
        destroy2 = reinterpret_cast<Destroy2>(GetProcAddress(library, "ADL2_Main_Control_Destroy"));
        create_device = reinterpret_cast<CreateDevice>(GetProcAddress(library, "ADL2_Device_PMLog_Device_Create"));
        destroy_device = reinterpret_cast<DestroyDevice>(GetProcAddress(library, "ADL2_Device_PMLog_Device_Destroy"));
        awake_start = reinterpret_cast<Awake>(GetProcAddress(library, "ADL2_Adapter_KeepAwake_Start"));
        awake_stop = reinterpret_cast<Awake>(GetProcAddress(library, "ADL2_Adapter_KeepAwake_Stop"));
        return initialized && create2 && destroy2 && create_device && destroy_device && awake_start && awake_stop
            && create2(Allocate, 1, &context2) == ADL_OK && context2;
    }
    ~RadeonReferenceADL()
    {
        if(context2 && destroy2) destroy2(context2);
        if(initialized && destroy) destroy();
        if(library) FreeLibrary(library);
    }
    bool Exchange(int adapter, const RadeonReference::Message& message, bool echo = true)
    {
        std::lock_guard<std::mutex> lock(transfer_mutex);
        trace.clear();
        ADL_D3DKMT_HANDLE device = 0;
        const int created = create_device(context2, adapter, &device);
        if(created != ADL_OK) { trace.push_back("PMLog device create failed: " + std::to_string(created)); return false; }
        const int awake = awake_start(context2, adapter, device);
        // Some driver versions do not support KeepAwake. The SMU response
        // remains mandatory regardless; never treat this as an acknowledgement.
        if(awake != ADL_OK) trace.push_back("KeepAwake unavailable: " + std::to_string(awake));
        const bool ok = RadeonReference::Exchange(message, [this, adapter](bool write, uint8_t* data, unsigned int size)
        {
            ADLI2C request{};
            request.iSize = sizeof(request);
            request.iLine = 0;
            request.iAddress = write ? 0xB4 : 0xB5;
            request.iAction = write ? ADL_DL_I2C_ACTIONWRITE : ADL_DL_I2C_ACTIONREAD;
            request.iSpeed = 100;
            request.iDataSize = static_cast<int>(size);
            request.pcData = reinterpret_cast<char*>(data);
            const int result = transfer(adapter, &request);
            std::ostringstream record;
            record << (write ? "write" : "read") << " ADL=" << result << " data=";
            for(unsigned int i = 0; i < size; ++i) record << std::hex << static_cast<unsigned int>(data[i]) << ' ';
            trace.push_back(record.str());
            return result == ADL_OK;
        }, [] { Sleep(30); }, echo);
        if(awake == ADL_OK) awake_stop(context2, adapter, device);
        destroy_device(context2, device);
        return ok;
    }
};
