/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "RazerBluetoothTransport_Windows.h"
#include "DetectionManager.h"
#include "LogManager.h"
#include "RGBController.h"
#include "RazerBluetoothLighting.h"
#include "../RazerController/RazerBattery.h"
#include <hidapi.h>
#include <future>
#include <thread>

namespace
{
bool WiredPresent(bool keyboard, bool v4 = false)
{
    auto info = hid_enumerate(0x1532, keyboard ? 0x0258 : v4 ? 0x00E2 : 0x00D6);
    bool present = info != nullptr;
    hid_free_enumeration(info);
    return present;
}

class BluetoothWorker
{
    std::mutex mutex;
    std::condition_variable changed;
    std::thread thread;
    bool stop = false;
    bool pending = false;
    bool battery_requested = false;
    bool owns_lighting = false;
    std::vector<RazerBLE::Bytes> desired;
public:
    RazerBatteryCache battery;
    bool keyboard = false;
    bool v4 = false;
    std::vector<uint8_t> zones;
    std::vector<RazerBLE::Bytes> initial;
    ~BluetoothWorker()
    {
        { std::lock_guard<std::mutex> lock(mutex); stop = true; }
        changed.notify_all();
        if(thread.joinable()) thread.join();
    }
    bool Start(winrt::hstring id)
    {
        battery.Enable("Bluetooth");
        std::promise<bool> ready;
        auto future = ready.get_future();
        thread = std::thread([this,id,ready=std::move(ready)]() mutable
        {
            try
            {
                RazerBLE::Apartment apartment;
                RazerBLE::Transport transport;
                try
                {
                    transport.Open(id);
                    keyboard = transport.keyboard;
                    v4 = transport.v4;
                    zones = transport.zones;
                    initial = transport.initial;
                    if(WiredPresent(keyboard, v4)) { ready.set_value(false); return; }
                    ready.set_value(true);
                }
                catch(std::exception const& e) { LOG_DEBUG("[Razer Bluetooth] Initial connection: %s", e.what()); ready.set_value(false); return; }
                catch(winrt::hresult_error const& e) { LOG_DEBUG("[Razer Bluetooth] Initial connection: %s", winrt::to_string(e.message()).c_str()); ready.set_value(false); return; }
                auto next_readback = std::chrono::steady_clock::time_point{};
                auto next_warning = std::chrono::steady_clock::time_point{};
                auto next_battery = std::chrono::steady_clock::time_point{};
                auto sent = initial;
                bool connected = true;
                auto connection_generation = transport.ConnectionGeneration();
                auto next = std::chrono::steady_clock::now();
                std::unique_lock<std::mutex> lock(mutex);
                while(!stop)
                {
                    changed.wait_for(lock, std::chrono::seconds(2), [this]{ return stop || pending || battery_requested; });
                    if(stop) break;
                    if(battery_requested || std::chrono::steady_clock::now() >= next_battery)
                    {
                        battery_requested = false;battery.SetRefreshing(true);
                        lock.unlock();
                        RazerBatteryStatus value;
                        value.supported = true; value.transport = "Bluetooth";
                        try { value.percent = transport.ReadBattery(); value.reachable = value.percent >= 0; } catch(...) {}
                        battery.Publish(value);battery.SetRefreshing(false);
                        next_battery = std::chrono::steady_clock::now() + RazerBatteryPollInterval;
                        lock.lock();
                        if(stop) break;
                    }
                    // Check every pass, including repeated identical effect frames.
                    // An edge counter also catches a reconnect between two polls.
                    if(owns_lighting && (!transport.IsConnected()
                        || transport.ConnectionGeneration() != connection_generation))
                    {
                        connected = false;
                        pending = true;
                    }
                    // Some mouse wakeups do not produce a Windows disconnect edge.
                    // Verify the device state at low frequency, outside the UI/mutex.
                    if(owns_lighting && !keyboard && connected && !sent.empty()
                        && std::chrono::steady_clock::now() >= next_readback)
                    {
                        lock.unlock();
                        bool matches = true;
                        try
                        {
                            if(WiredPresent(keyboard, v4)) throw std::runtime_error("USB transport has priority");
                            for(size_t i = 0; i < sent.size(); ++i)
                                matches = transport.MatchesLighting(zones[i], sent[i]) && matches;
                        }
                        catch(...) { connected = false; matches = false; }
                        next_readback = std::chrono::steady_clock::now() + std::chrono::seconds(5);
                        lock.lock();
                        if(stop) break;
                        if(!matches)
                        {
                            sent.clear();
                            pending = true;
                            LOG_INFO("[Razer Bluetooth] Mouse lighting state lost or unavailable; restoring requested scene");
                        }
                    }
                    if(!pending) continue;
                    if(std::chrono::steady_clock::now() < next)
                    {
                        changed.wait_until(lock,next,[this]{ return stop; });
                        if(stop) break;
                    }
                    auto target = desired;
                    pending = false;
                    lock.unlock();
                    try
                    {
                        // A wired controller has priority; never write BLE concurrently.
                        if(WiredPresent(keyboard, v4)) throw std::runtime_error("USB transport has priority");
                        if(!connected || !transport.IsConnected()
                            || transport.ConnectionGeneration() != connection_generation)
                        {
                            transport.Open(id);
                            connection_generation = transport.ConnectionGeneration();
                            connected = true;
                            // Reapply every controlled zone, including unchanged colors,
                            // hardware effect and brightness after the new GATT session.
                            sent.clear();
                            LOG_INFO("[Razer Bluetooth] %s reconnected; restoring requested lighting state", keyboard ? "Keyboard" : "Mouse");
                        }
                        for(size_t i = 0; i < target.size(); ++i)
                        {
                            if(i < sent.size() && target[i] == sent[i]) continue;
                            transport.Set(zones[i],target[i]);
                        }
                        sent = target;
                        next = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
                    }
                    catch(...)
                    {
                        if(std::chrono::steady_clock::now() >= next_warning)
                        {
                            std::string reason = "Unknown error";
                            try { throw; }
                            catch(std::exception const& error) { reason = error.what(); }
                            catch(winrt::hresult_error const& error) { reason = winrt::to_string(error.message()); }
                            catch(...) {}
                            LOG_WARNING("[Razer Bluetooth] %s restore failed: %s; retrying", keyboard ? "Keyboard" : "Mouse", reason.c_str());
                            next_warning = std::chrono::steady_clock::now() + std::chrono::seconds(30);
                        }
                        transport.Close();
                        connected = false;
                        next = std::chrono::steady_clock::now() + std::chrono::seconds(5);
                        // Preserve the latest requested scene for reconnect, no frame queue.
                        lock.lock();
                        if(!pending) pending = true; // desired keeps the undimmed latest scene.
                        lock.unlock();
                    }
                    lock.lock();
                }
            }
            catch(...) { try { ready.set_value(false); } catch(...) {} }
        });
        return future.get();
    }
    bool RequestBatteryRefresh()
    {
        std::lock_guard<std::mutex> lock(mutex);
        if(stop || battery_requested || battery.Snapshot().refreshing) return false;
        battery_requested = true;battery.SetRefreshing(true);changed.notify_all();return true;
    }
    void Submit(std::vector<RazerBLE::Bytes> state)
    {
        { std::lock_guard<std::mutex> lock(mutex);desired = std::move(state);pending = true;owns_lighting = true; }
        changed.notify_all();
    }
};

class RGBController_RazerBluetooth : public RGBController, public RazerBatteryProvider
{
    std::unique_ptr<BluetoothWorker> worker;
public:
    RazerBatteryStatus GetBatteryStatus() const override { return worker->battery.Snapshot(); }
    bool RequestBatteryRefresh() override { return worker->RequestBatteryRefresh(); }
    explicit RGBController_RazerBluetooth(std::unique_ptr<BluetoothWorker> value) : worker(std::move(value))
    {
        name = worker->keyboard ? "Razer BlackWidow V3 Mini (Bluetooth)" : worker->v4 ? "Razer Basilisk V4 Pro (Bluetooth)" : "Razer Basilisk V3 Pro 35K Phantom Green (Bluetooth)";
        vendor = "Razer";
        type = worker->keyboard ? DEVICE_TYPE_KEYBOARD : DEVICE_TYPE_MOUSE;
        description = "Bluetooth hardware lighting effects; independently controlled mouse groups. No per-key/Direct streaming.";
        location = "Bluetooth";
        mode Static;
        Static.name = "Static";
        Static.value = 0;
        Static.flags = MODE_FLAG_HAS_PER_LED_COLOR | MODE_FLAG_HAS_BRIGHTNESS;
        Static.color_mode = MODE_COLORS_PER_LED;
        Static.brightness_min = 0;
        Static.brightness_max = 255;
        Static.brightness = 0;for(const auto& state:worker->initial)Static.brightness=std::max(Static.brightness,unsigned(state[10]));
        modes.push_back(Static);
        mode Off;
        Off.name = "Off";
        Off.value = 1;
        Off.color_mode = MODE_COLORS_NONE;
        modes.push_back(Off);
        if(worker->keyboard)
        {
            mode Wave;
            Wave.name = "Wave";
            Wave.value = 2;
            Wave.flags = MODE_FLAG_HAS_BRIGHTNESS;
            Wave.color_mode = MODE_COLORS_NONE;
            Wave.brightness_min = 0;
            Wave.brightness_max = 255;
            Wave.brightness = Static.brightness;
            modes.push_back(Wave);
        }
        auto addMode=[&](const char* name,int value,bool specific){
            mode effect;effect.name=name;effect.value=value;
            effect.flags=MODE_FLAG_HAS_BRIGHTNESS|(specific?MODE_FLAG_HAS_MODE_SPECIFIC_COLOR:0)|(!worker->keyboard?MODE_FLAG_HAS_PER_LED_COLOR:0);
            effect.color_mode=specific?MODE_COLORS_MODE_SPECIFIC:(!worker->keyboard?MODE_COLORS_PER_LED:MODE_COLORS_NONE);
            effect.brightness_min=0;effect.brightness_max=255;effect.brightness=Static.brightness;
            if(specific){effect.colors_min=effect.colors_max=1;effect.colors={ToRGBColor(worker->initial[0][4],worker->initial[0][5],worker->initial[0][6])};}
            modes.push_back(effect);
        };
        if(worker->keyboard)
        {
            addMode("Breathing",RazerBLELighting::Breathing,true);
            addMode("Spectrum Cycle",RazerBLELighting::Spectrum,false);
            addMode("Reactive",RazerBLELighting::Reactive,true);
            addMode("Starlight",RazerBLELighting::Starlight,true);
        }
        else
        {
            addMode("Breathing",RazerBLELighting::Breathing,false);
            addMode("Spectrum Cycle",RazerBLELighting::Spectrum,false);
            addMode("Wave",RazerBLELighting::Wave,false);
            if(!worker->v4) addMode("Reactive",RazerBLELighting::Reactive,false);
        }
        auto initialState=worker->initial.front();for(const auto& state:worker->initial)if(state[10]){initialState=state;break;}
        const int initialMode=RazerBLELighting::ModeFromState(initialState[0],initialState[10]);
        active_mode=0;for(size_t i=0;i<modes.size();++i)if(modes[i].value==initialMode)active_mode=int(i);
        SetupZones();
        for(size_t i = 0; i < colors.size(); ++i)
        {
            const auto& state=worker->initial[i];
            colors[i]=state[10]?ToRGBColor(state[4],state[5],state[6]):0;
            if(!worker->keyboard && state[10] && state[0]!=1 && colors[i]==0)colors[i]=ToRGBColor(255,255,255);
        }
    }
    ~RGBController_RazerBluetooth() override { Shutdown(); worker.reset(); }
    void SetupZones()
    {
        zones.clear(); leds.clear();
        for(auto id : worker->zones)
        {
            zone z;
            z.name = worker->keyboard ? "Keyboard (all keys)" : (id == 4 ? "Logo" : id == 1 ? "Scroll Wheel" : "Underglow (group)");
            z.type = ZONE_TYPE_SINGLE;
            z.leds_min = z.leds_max = z.leds_count = 1;
            zones.push_back(z);
            led l;
            l.name = z.name;
            leds.push_back(l);
        }
        SetupColors();
    }
    void DeviceUpdateLEDs() override
    {
        std::vector<RazerBLE::Bytes> target;
        auto& effect=modes[active_mode];
        for(auto color : colors)
        {
            if(effect.color_mode==MODE_COLORS_MODE_SPECIFIC && !effect.colors.empty())color=effect.colors[0];
            const auto state=RazerBLELighting::KeyboardState(!worker->keyboard && color==0?RazerBLELighting::Off:effect.value,color,static_cast<uint8_t>(effect.brightness));
            target.emplace_back(state.begin(),state.end());
        }
        worker->Submit(std::move(target));
    }
    void DeviceUpdateZoneLEDs(int) override { DeviceUpdateLEDs(); }
    void DeviceUpdateSingleLED(int) override { DeviceUpdateLEDs(); }
    void DeviceUpdateMode() override { DeviceUpdateLEDs(); }
};
}

DetectedControllers DetectRazerBluetoothControllers()
{
    DetectedControllers found;
    // All WinRT discovery occurs in an MTA, independent of the Qt UI apartment.
    std::vector<winrt::hstring> candidates;
    try
    {
        candidates = std::async(std::launch::async, []
        {
            RazerBLE::Apartment apartment;
            std::vector<winrt::hstring> ids;
            {
                auto devices = RazerBLE::Await(RazerBLE::DeviceInformation::FindAllAsync(RazerBLE::BluetoothLEDevice::GetDeviceSelectorFromPairingState(true)));
                for(auto const& d : devices)
                    if(d.Name() == L"BSKV3P 35K" || d.Name() == L"BlackWidow V3 Mini" || d.Name() == L"BSK V4 PRO") ids.push_back(d.Id());
            }
            return ids;
        }).get();
        for(auto const& id : candidates)
        {
            auto worker = std::make_unique<BluetoothWorker>();
            if(worker->Start(id)) found.push_back(new RGBController_RazerBluetooth(std::move(worker)));
        }
    }
    catch(...) { LOG_DEBUG("[Razer Bluetooth] Discovery unavailable"); }
    return found;
}
REGISTER_DETECTOR("Razer Bluetooth (verified lighting groups)", DetectRazerBluetoothControllers);
