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
    @direct :white_check_mark:
    @effects :white_check_mark:
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
    mode direct=static_mode;direct.name="Direct";direct.value=2;modes.push_back(direct);
    for(int value:{3,4})
    {
        mode effect=static_mode;effect.name=value==3?"Breathing (software)":"Color Cycle (software)";effect.value=value;
        effect.flags|=MODE_FLAG_HAS_SPEED;effect.speed_min=1;effect.speed_max=10;effect.speed=3;
        modes.push_back(effect);
    }
    // Append native modes so saved indices of existing modes stay valid.
    mode breathing{};
    breathing.name = "Breathing (hardware)";
    breathing.value = 5;
    breathing.flags = MODE_FLAG_HAS_MODE_SPECIFIC_COLOR | MODE_FLAG_HAS_RANDOM_COLOR
                    | MODE_FLAG_HAS_SPEED | MODE_FLAG_HAS_BRIGHTNESS;
    breathing.color_mode = MODE_COLORS_MODE_SPECIFIC;
    breathing.colors_min = breathing.colors_max = 1;
    breathing.colors = {ToRGBColor(255, 0, 0)};
    breathing.speed_min = breathing.brightness_min = 1;
    breathing.speed_max = breathing.brightness_max = 3;
    breathing.speed = 2;
    breathing.brightness = 3;
    modes.push_back(breathing);
    mode cycle = breathing;
    cycle.name = "Color Cycle (hardware)";
    cycle.value = 6;
    cycle.flags = MODE_FLAG_HAS_SPEED | MODE_FLAG_HAS_BRIGHTNESS;
    cycle.color_mode = MODE_COLORS_NONE;
    cycle.colors_min = cycle.colors_max = 0;
    cycle.colors.clear();
    modes.push_back(cycle);
    description+=". Direct/software effects: one global zone, capped at 4 updates/s; sustained hardware validation pending.";
    SetupZones();
    softwareEffect=std::make_unique<SoftwareLighting::Worker>([this](std::vector<RGBColor>& frame){
        if(!frame.empty())ApplyState(frame[0],false,software_brightness.load());
    },std::chrono::milliseconds(250));
}

RGBController_AMDRadeonReference::~RGBController_AMDRadeonReference() { Shutdown();if(softwareEffect)softwareEffect->Shutdown(); }
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
void RGBController_AMDRadeonReference::DeviceUpdateMode() { if(softwareEffect)softwareEffect->Stop();Update(); }

void RGBController_AMDRadeonReference::Update()
{
    if(colors.empty()||active_mode<0||static_cast<size_t>(active_mode)>=modes.size())return;
    const auto& selected=modes[active_mode];
    if(selected.value==3||selected.value==4)
    {
        software_brightness.store(selected.brightness);
        auto base=colors;
        if(selected.value==4 && base[0]==0)base[0]=ToRGBColor(255,255,255);
        if(softwareEffect)softwareEffect->Set(std::move(base),selected.speed,selected.value==4);
        return;
    }
    if(selected.value == 5)
    {
        const auto color = selected.colors.empty() ? colors[0] : selected.colors[0];
        ApplyMessage(RadeonReference::Breathing(RGBGetRValue(color), RGBGetGValue(color), RGBGetBValue(color),
            selected.brightness, selected.speed, selected.color_mode == MODE_COLORS_RANDOM), 0x01);
    }
    else if(selected.value == 6)
        ApplyMessage(RadeonReference::ColorCycle(selected.brightness, selected.speed), 0x02);
    else ApplyState(colors[0],selected.value==1,selected.brightness);
}
void RGBController_AMDRadeonReference::ApplyState(RGBColor color,bool off,unsigned brightness)
{
    ApplyMessage(RadeonReference::Static(RGBGetRValue(color),RGBGetGValue(color),RGBGetBValue(color),brightness), off ? 0xFE : 0x00);
}
void RGBController_AMDRadeonReference::ApplyMessage(const RadeonReference::Message& message, uint8_t selection)
{
    std::lock_guard<std::mutex> guard(io_mutex);
    if(failed)return;
    const bool off = selection == 0xFE;
    if(have_previous && selection == previous_selection && (off || previous == message)) return;
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
    // Reuse captured live-lighting frames. No explicit persistent-save command is sent;
    // persistence of Apply itself across power loss has not been established.
    const auto earliest = last_update + std::chrono::milliseconds(250);
    if(std::chrono::steady_clock::now() < earliest) std::this_thread::sleep_until(earliest);
    last_update = std::chrono::steady_clock::now();
    const bool initialized = lighting_initialized || (adl->Exchange(adapter, RadeonReference::Identify(), false)
        && adl->Exchange(adapter, RadeonReference::Enable())
        && adl->Exchange(adapter, RadeonReference::SelectControl())
        && adl->Exchange(adapter, RadeonReference::Apply()));
    const bool ok = initialized && (off || adl->Exchange(adapter, message))
        && adl->Exchange(adapter, selection == 0x01 || selection == 0x02
            ? RadeonReference::EffectSelection(selection == 0x02) : RadeonReference::Mode(off))
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
    previous_selection = selection;
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
