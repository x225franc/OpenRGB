/* SPDX-License-Identifier: GPL-2.0-or-later */
// Uses the same ADL transport and packet builders as the native controller.
#include "../../Controllers/AMDRadeonReferenceController/RadeonReferenceADL_Windows.h"
#include <tlhelp32.h>
#include <cwctype>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>

int main(int argc, char** argv)
{
    const std::string action = argc == 2 ? argv[1] : "";
    if(action != "red" && action != "green" && action != "blue" && action != "off" && action != "red-low")
    {
        std::cerr << "Usage: NativeProbe.exe red|green|blue|off|red-low\n";
        return 1;
    }
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if(snapshot == INVALID_HANDLE_VALUE) return 2;
    PROCESSENTRY32W process{};
    process.dwSize = sizeof(process);
    bool busy = false;
    if(!Process32FirstW(snapshot, &process)) { CloseHandle(snapshot); return 2; }
    do
    {
        std::wstring name(process.szExeFile);
        for(auto& character : name) character = static_cast<wchar_t>(std::towlower(character));
        if(name == L"openrgb.exe" || name.find(L"amd_radeon_rx_7900_xtx_series_rgb_led") == 0) busy = true;
    } while(Process32NextW(snapshot, &process));
    CloseHandle(snapshot);
    if(busy) { std::cerr << "Close OpenRGB and AMD RGB Tool first\n"; return 2; }

    RadeonReferenceADL session;
    if(!session.Initialize()) { std::cerr << "ADL SMU initialization failed\n"; return 3; }
    int count = 0;
    if(session.count(&count) != ADL_OK || count < 1 || count > 128) return 3;
    std::vector<AdapterInfo> adapters(static_cast<size_t>(count));
    for(auto& adapter : adapters) adapter.iSize = sizeof(AdapterInfo);
    if(session.info(adapters.data(), static_cast<int>(adapters.size() * sizeof(AdapterInfo))) != ADL_OK) return 3;
    for(const auto& adapter : adapters)
    {
        const std::string pnp(adapter.strPNPString, strnlen(adapter.strPNPString, sizeof(adapter.strPNPString)));
        if(pnp.find("VEN_1002&DEV_744C&SUBSYS_0E3B1002") == std::string::npos || !adapter.iPresent) continue;
        std::ostringstream location;
        location << "AMD SMU I2C: " << adapter.iBusNumber << ':' << adapter.iDeviceNumber << ':' << adapter.iFunctionNumber;
        RadeonReferenceDeviceLock device_lock(location.str());
        if(!device_lock.Acquired()) { std::cerr << "Controller busy in another instance\n"; return 2; }
        std::cout << "Target 1002:744C / 1002:0E3B, PCI " << adapter.iBusNumber << ':' << adapter.iDeviceNumber
                  << ':' << adapter.iFunctionNumber << ", ADL index " << adapter.iAdapterIndex << '\n';
        const bool off = action == "off";
        const auto color = RadeonReference::Static(action == "red" || action == "red-low" ? 255 : 0,
            action == "green" ? 255 : 0, action == "blue" ? 255 : 0, action == "red-low" ? 1 : 5);
        const bool ok = session.Exchange(adapter.iAdapterIndex, RadeonReference::Identify(), false)
            && session.Exchange(adapter.iAdapterIndex, RadeonReference::Enable())
            && session.Exchange(adapter.iAdapterIndex, RadeonReference::SelectControl())
            && session.Exchange(adapter.iAdapterIndex, RadeonReference::Apply())
            && (off || session.Exchange(adapter.iAdapterIndex, color))
            && session.Exchange(adapter.iAdapterIndex, RadeonReference::Mode(off))
            && session.Exchange(adapter.iAdapterIndex, RadeonReference::Apply());
        std::cout << (ok ? "PASS: all native transactions and echo replies verified; visual confirmation required\n"
                         : "FAIL: transport or reply; stopped immediately, no retry or save command\n");
        if(!ok) for(const auto& trace : session.trace) std::cerr << trace << '\n';
        return ok ? 0 : 4;
    }
    std::cerr << "Exact supported PCI subsystem not found\n";
    return 5;
}
