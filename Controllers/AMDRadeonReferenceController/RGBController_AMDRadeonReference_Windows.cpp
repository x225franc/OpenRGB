/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "RGBController_AMDRadeonReference_Windows.h"
#include "RadeonReferenceADL_Windows.h"
#include "DetectionManager.h"
#include "LogManager.h"
#include "adl_sdk.h"
#include <windows.h>
#include <tlhelp32.h>
#include <powrprof.h>
#include <powerbase.h>
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

// Runtime-loaded Windows power notifications avoid a new linker dependency.
class RadeonReferencePowerEvents
{
    using Register = DWORD (WINAPI*)(DWORD, HANDLE, PHPOWERNOTIFY);
    using Unregister = DWORD (WINAPI*)(HPOWERNOTIFY);
    HMODULE library = nullptr;
    HPOWERNOTIFY notification = nullptr;
    Unregister unregister = nullptr;
    DEVICE_NOTIFY_SUBSCRIBE_PARAMETERS recipient{};
    static ULONG CALLBACK Notify(PVOID context, ULONG type, PVOID)
    {
        auto worker = static_cast<RadeonReference::OutputWorker*>(context);
        if(type == PBT_APMSUSPEND) worker->Pause(true);
        else if(type == PBT_APMRESUMEAUTOMATIC || type == PBT_APMRESUMESUSPEND) worker->Pause(false);
        return ERROR_SUCCESS;
    }
public:
    explicit RadeonReferencePowerEvents(RadeonReference::OutputWorker* worker)
    {
        library = LoadLibraryExW(L"powrprof.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if(!library) return;
        auto subscribe = reinterpret_cast<Register>(GetProcAddress(library, "PowerRegisterSuspendResumeNotification"));
        unregister = reinterpret_cast<Unregister>(GetProcAddress(library, "PowerUnregisterSuspendResumeNotification"));
        recipient = {Notify, worker};
        if(!subscribe || !unregister || subscribe(DEVICE_NOTIFY_CALLBACK, &recipient, &notification) != ERROR_SUCCESS)
        {
            notification = nullptr;
            LOG_WARNING("[AMD Radeon Reference] Power notification registration unavailable; idle-gap invalidation remains active.");
        }
    }
    ~RadeonReferencePowerEvents()
    {
        if(notification && unregister) unregister(notification);
        if(library) FreeLibrary(library);
    }
};

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
    name = "AMD Radeon RX 7900 XTX Reference";
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
    mode direct = static_mode;
    direct.name = "Direct";
    direct.value = 2;
    direct.flags |= MODE_FLAG_HAS_SPEED;
    direct.speed_min = 1;direct.speed_max = 10;direct.speed = 7;
    modes.push_back(direct);
    // Firmware effect values stay stable; profile migration matches modes by name.
    mode breathing{};
    breathing.name = "Breathing (hardware)";
    breathing.value = 5;
    breathing.flags = MODE_FLAG_HAS_MODE_SPECIFIC_COLOR | MODE_FLAG_HAS_RANDOM_COLOR
                    | MODE_FLAG_HAS_SPEED | MODE_FLAG_HAS_BRIGHTNESS;
    breathing.color_mode = MODE_COLORS_MODE_SPECIFIC;
    breathing.colors_min = breathing.colors_max = 1;
    breathing.colors = {ToRGBColor(255, 0, 0)};
    // Larger wire presets produce slower firmware animation. Qt supports inverted ranges.
    breathing.speed_min = 3;
    breathing.speed_max = 1;
    breathing.brightness_min = 1;
    breathing.brightness_max = 3;
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
    description += ". Native hardware effects; Direct target 1-10 updates/s (default 7), actual rate depends on verified SMU exchanges.";
    SetupZones();
    output_session = std::make_unique<RadeonReference::OutputSession>([this](const RadeonReference::Message& message, bool echo){
        return adl->Exchange(adapter, message, echo);
    });
    output = std::make_unique<RadeonReference::OutputWorker>([this](const RadeonReference::Frame& frame, uint64_t epoch){
        return SendFrame(frame, epoch);
    }, [this]{ ownership.reset(); });
    power_events = std::make_unique<RadeonReferencePowerEvents>(output.get());
}

RGBController_AMDRadeonReference::~RGBController_AMDRadeonReference()
{
    Shutdown();
    power_events.reset();
    if(output) output->Shutdown();
}
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
    if(colors.empty()||active_mode<0||static_cast<size_t>(active_mode)>=modes.size())return;
    const auto& selected=modes[active_mode];
    RadeonReference::Frame frame;
    frame.kind = selected.value;
    frame.fps = selected.value == 2 ? std::clamp(selected.speed, 1u, 10u) : 7;
    if(selected.value == 5)
    {
        const auto color = selected.colors.empty() ? colors[0] : selected.colors[0];
        frame.message = RadeonReference::Breathing(RGBGetRValue(color), RGBGetGValue(color), RGBGetBValue(color),
            selected.brightness, selected.speed, selected.color_mode == MODE_COLORS_RANDOM);
        frame.selection = 1;
    }
    else if(selected.value == 6)
    {
        frame.message = RadeonReference::ColorCycle(selected.brightness, selected.speed);
        frame.selection = 2;
    }
    else
    {
        frame.message = RadeonReference::Static(RGBGetRValue(colors[0]), RGBGetGValue(colors[0]), RGBGetBValue(colors[0]), selected.brightness);
        frame.selection = selected.value == 1 ? 0xFE : 0;
    }
    if(output) output->Submit(std::move(frame));
}

RadeonReference::SendResult RGBController_AMDRadeonReference::SendFrame(const RadeonReference::Frame& frame, uint64_t epoch)
{
    // This function and the session cache belong exclusively to the output worker.
    if(OfficialToolRunning())
    {
        output_session->Invalidate();ownership.reset();
        if(std::chrono::steady_clock::now() >= next_warning)
        {
            LOG_WARNING("[AMD Radeon Reference] RGB Tool is running; output paused until it closes.");
            next_warning = std::chrono::steady_clock::now() + std::chrono::seconds(30);
        }
        return RadeonReference::SendResult::Deferred;
    }
    if(!ownership)
    {
        ownership = std::make_unique<RadeonReferenceDeviceLock>(location);
        if(!ownership->Acquired())
        {
            ownership.reset();output_session->Invalidate();
            if(std::chrono::steady_clock::now() >= next_warning)
            {
                LOG_WARNING("[AMD Radeon Reference] Another process owns this lighting controller; output paused.");
                next_warning = std::chrono::steady_clock::now() + std::chrono::seconds(30);
            }
            return RadeonReference::SendResult::Deferred;
        }
    }
    const uint64_t now = GetTickCount64();
    // Never reuse mode-once assumptions after a long idle interval or sleep gap.
    if(last_activity && now - last_activity > 2000) output_session->Invalidate();
    last_activity = now;
    if(!output_session->Apply(frame, epoch))
    {
        LOG_ERROR("[AMD Radeon Reference] SMU transfer or acknowledgement failed. Writes stopped until rescan; no retry or save command sent.");
        for(const auto& entry : adl->trace) LOG_DEBUG("[AMD Radeon Reference] %s", entry.c_str());
        return RadeonReference::SendResult::Failed;
    }
    return RadeonReference::SendResult::Applied;
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

REGISTER_DETECTOR("AMD Radeon RX 7900 XTX Reference", DetectAMDRadeonReference);
