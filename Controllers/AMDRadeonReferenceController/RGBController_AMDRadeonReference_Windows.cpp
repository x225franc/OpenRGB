/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "RGBController_AMDRadeonReference_Windows.h"
#include "RadeonReferenceADL_Windows.h"
#include "DetectionManager.h"
#include "LogManager.h"
#include "adl_sdk.h"
#include <windows.h>
#include <tlhelp32.h>
#include <cstdlib>
#include <cstring>
#include <cwctype>
#include <set>
#include <sstream>
#include <thread>
#include <vector>

namespace
{
bool OfficialToolRunning()
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if(snapshot == INVALID_HANDLE_VALUE) return true;
    PROCESSENTRY32W process{};
    process.dwSize = sizeof(process);
    bool found = false;
    if(Process32FirstW(snapshot, &process))
    {
        do
        {
            std::wstring name(process.szExeFile);
            for(wchar_t& character : name) character = static_cast<wchar_t>(std::towlower(character));
            if(name.find(L"amd_radeon_rx_7900_xtx_series_rgb_led") == 0)
            {
                found = true;
                break;
            }
        } while(Process32NextW(snapshot, &process));
    }
    else found = true;
    CloseHandle(snapshot);
    return found;
}
}

/**------------------------------------------------------------------*\
    @name AMD Radeon RX 7900 XTX Reference
    @category GPU
    @type I2C
    @save :x:
    @direct :x:
    @effects :x:
    @detectors DetectAMDRadeonReference
    @comment Windows AMD SMU transport. One global lighting zone.
        PCI 1002:744C subsystem 1002:0E3B only, including Avatar edition.
        Native implementation awaits hardware validation.
\*-------------------------------------------------------------------*/
RGBController_AMDRadeonReference::RGBController_AMDRadeonReference(std::shared_ptr<RadeonReferenceADL> session, int index, const std::string& pci_location)
    : adl(std::move(session)), adapter(index)
{
    name = "AMD Radeon RX 7900 XTX Reference / Avatar";
    vendor = "AMD";
    description = "AMD reference board RGB lighting (Cooler Master)";
    location = "AMD SMU I2C: " + pci_location;
    type = DEVICE_TYPE_GPU;

    mode static_mode{};
    static_mode.name = "Static";
    static_mode.value = 0;
    static_mode.flags = MODE_FLAG_HAS_PER_LED_COLOR | MODE_FLAG_HAS_BRIGHTNESS;
    static_mode.color_mode = MODE_COLORS_PER_LED;
    static_mode.brightness_min = 1;
    static_mode.brightness_max = 5;
    static_mode.brightness = 5;
    modes.push_back(static_mode);
    mode off{};
    off.name = "Off";
    off.value = 1;
    off.color_mode = MODE_COLORS_NONE;
    modes.push_back(off);
    SetupZones();
}

RGBController_AMDRadeonReference::~RGBController_AMDRadeonReference() { Shutdown(); }
void RGBController_AMDRadeonReference::SetupZones()
{
    zone global{};
    global.name = "GPU Lighting";
    global.type = ZONE_TYPE_SINGLE;
    global.leds_min = global.leds_max = global.leds_count = 1;
    zones.push_back(global);
    led light{};
    light.name = "GPU Lighting";
    leds.push_back(light);
    SetupColors();
}
void RGBController_AMDRadeonReference::DeviceUpdateLEDs() { Update(); }
void RGBController_AMDRadeonReference::DeviceUpdateZoneLEDs(int) { Update(); }
void RGBController_AMDRadeonReference::DeviceUpdateSingleLED(int) { Update(); }
void RGBController_AMDRadeonReference::DeviceUpdateMode() { Update(); }

void RGBController_AMDRadeonReference::Update()
{
    std::lock_guard<std::mutex> guard(io_mutex);
    if(failed || colors.empty() || active_mode < 0 || static_cast<size_t>(active_mode) >= modes.size()) return;
    const bool off = modes[active_mode].value == 1;
    const auto message = RadeonReference::Static(RGBGetRValue(colors[0]), RGBGetGValue(colors[0]), RGBGetBValue(colors[0]), modes[0].brightness);
    if(have_previous && off == previous_off && (off || previous == message)) return;
    if(OfficialToolRunning())
    {
        LOG_WARNING("[AMD Radeon Reference] RGB Tool is running; close it before changing lighting.");
        return;
    }
    RadeonReferenceDeviceLock device_lock(location);
    if(!device_lock.Acquired())
    {
        LOG_WARNING("[AMD Radeon Reference] Another OpenRGB instance owns this lighting controller.");
        return;
    }
    // Static updates only. Never run the official persistent-save flow here.
    const auto earliest = last_update + std::chrono::milliseconds(250);
    if(std::chrono::steady_clock::now() < earliest) std::this_thread::sleep_until(earliest);
    last_update = std::chrono::steady_clock::now();
    const bool initialized = lighting_initialized || (adl->Exchange(adapter, RadeonReference::Identify(), false)
        && adl->Exchange(adapter, RadeonReference::Enable())
        && adl->Exchange(adapter, RadeonReference::SelectControl())
        && adl->Exchange(adapter, RadeonReference::Apply()));
    const bool ok = initialized && (off || adl->Exchange(adapter, message))
        && adl->Exchange(adapter, RadeonReference::Mode(off))
        && adl->Exchange(adapter, RadeonReference::Apply());
    if(!ok)
    {
        failed = true;
        LOG_ERROR("[AMD Radeon Reference] SMU transfer or acknowledgement failed. Writes stopped until rescan; no retry or save command sent.");
        for(const auto& entry : adl->trace) LOG_DEBUG("[AMD Radeon Reference] %s", entry.c_str());
        return;
    }
    previous = message;
    lighting_initialized = true;
    previous_off = off;
    have_previous = true;
}

DetectedControllers DetectAMDRadeonReference()
{
    DetectedControllers detected;
    // Reuse the legacy ADL session across rescans while old controllers exist.
    static std::weak_ptr<RadeonReferenceADL> existing_session;
    auto session = existing_session.lock();
    if(!session)
    {
        session = std::make_shared<RadeonReferenceADL>();
        if(!session->Initialize()) return detected;
        existing_session = session;
    }
    int count = 0;
    if(session->count(&count) != ADL_OK || count <= 0 || count > 128) return detected;
    std::vector<AdapterInfo> adapters(static_cast<size_t>(count));
    for(auto& adapter : adapters) adapter.iSize = sizeof(AdapterInfo);
    if(session->info(adapters.data(), static_cast<int>(adapters.size() * sizeof(AdapterInfo))) != ADL_OK) return detected;
    std::set<std::string> seen;
    for(const auto& adapter : adapters)
    {
        const std::string pnp(adapter.strPNPString, strnlen(adapter.strPNPString, sizeof(adapter.strPNPString)));
        if(pnp.find("VEN_1002&DEV_744C&SUBSYS_0E3B1002") == std::string::npos || !adapter.iPresent) continue;
        std::ostringstream pci;
        pci << adapter.iBusNumber << ':' << adapter.iDeviceNumber << ':' << adapter.iFunctionNumber;
        if(!seen.insert(pci.str()).second) continue;
        // Metadata enumeration only: detection never sends an RGB/I2C command.
        detected.push_back(new RGBController_AMDRadeonReference(session, adapter.iAdapterIndex, pci.str()));
    }
    return detected;
}

REGISTER_DETECTOR("AMD Radeon RX 7900 XTX Reference / Avatar", DetectAMDRadeonReference);
