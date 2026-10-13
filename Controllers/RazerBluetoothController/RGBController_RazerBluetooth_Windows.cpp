/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "RazerBluetoothTransport_Windows.h"
#include "DetectionManager.h"
#include "LogManager.h"
#include "RGBController.h"
#include "../RazerController/RazerBattery.h"
#include <hidapi.h>
#include <future>
#include <thread>

namespace
{
bool WiredPresent(bool keyboard)
{
    auto info = hid_enumerate(0x1532, keyboard ? 0x0258 : 0x00D6);
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
    bool owns_lighting = false;
    std::vector<RazerBLE::Bytes> desired;
public:
    RazerBatteryCache battery;
    bool keyboard = false;
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
                    zones = transport.zones;
                    initial = transport.initial;
                    if(WiredPresent(keyboard)) { ready.set_value(false); return; }
                    ready.set_value(true);
                }
                catch(std::exception const& e) { LOG_DEBUG("[Razer Bluetooth] Initial connection: %s", e.what()); ready.set_value(false); return; }
                catch(winrt::hresult_error const& e) { LOG_DEBUG("[Razer Bluetooth] Initial connection: %s", winrt::to_string(e.message()).c_str()); ready.set_value(false); return; }
                auto next_battery = std::chrono::steady_clock::time_point{};
                auto sent = initial;
                bool connected = true;
                auto next = std::chrono::steady_clock::now();
                std::unique_lock<std::mutex> lock(mutex);
                while(!stop)
                {
                    changed.wait_for(lock, std::chrono::seconds(2), [this]{ return stop || pending; });
                    if(stop) break;
                    if(std::chrono::steady_clock::now() >= next_battery)
                    {
                        lock.unlock();
                        RazerBatteryStatus value;
                        value.supported = true; value.transport = "Bluetooth";
                        try { value.percent = transport.ReadBattery(); value.reachable = value.percent >= 0; } catch(...) {}
                        battery.Publish(value);
                        next_battery = std::chrono::steady_clock::now() + std::chrono::seconds(30);
                        lock.lock();
                        if(stop) break;
                    }
                    if(!pending)
                    {
                        if(!owns_lighting || transport.IsConnected()) continue;
                        pending = true;
                        connected = false;
                    }
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
                        if(WiredPresent(keyboard)) throw std::runtime_error("USB transport has priority");
                        if(!connected) { transport.Open(id); connected = true; sent.clear(); }
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
                        transport.Close();
                        connected = false;
                        next = std::chrono::steady_clock::now() + std::chrono::seconds(5);
                        // Preserve the latest requested scene for reconnect, no frame queue.
                        lock.lock();
                        if(!pending) { desired = target; pending = true; }
                        lock.unlock();
                    }
                    lock.lock();
                }
            }
            catch(...) { try { ready.set_value(false); } catch(...) {} }
        });
        return future.get();
    }
    void Submit(std::vector<RazerBLE::Bytes> state)
    {
        { std::lock_guard<std::mutex> lock(mutex); desired = std::move(state); pending = true; owns_lighting = true; }
        changed.notify_all();
    }
};

class RGBController_RazerBluetooth : public RGBController, public RazerBatteryProvider
{
    std::unique_ptr<BluetoothWorker> worker;
public:
    RazerBatteryStatus GetBatteryStatus() const override { return worker->battery.Snapshot(); }
    explicit RGBController_RazerBluetooth(std::unique_ptr<BluetoothWorker> value) : worker(std::move(value))
    {
        name = worker->keyboard ? "Razer BlackWidow V3 Mini (Bluetooth)" : "Razer Basilisk V3 Pro 35K Phantom Green (Bluetooth)";
        vendor = "Razer";
        type = worker->keyboard ? DEVICE_TYPE_KEYBOARD : DEVICE_TYPE_MOUSE;
        description = "Bluetooth lighting groups; static color, brightness and keyboard hardware Wave. No per-key/Direct streaming.";
        location = "Bluetooth";
        mode Static;
        Static.name = "Static";
        Static.value = 0;
        Static.flags = MODE_FLAG_HAS_PER_LED_COLOR | MODE_FLAG_HAS_BRIGHTNESS;
        Static.color_mode = MODE_COLORS_PER_LED;
        Static.brightness_min = 0;
        Static.brightness_max = 255;
        Static.brightness = worker->initial.front()[10];
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
        active_mode = worker->initial.front()[10] == 0 ? 1 : (worker->keyboard && worker->initial.front()[0] == 4 ? 2 : 0);
        SetupZones();
        for(size_t i = 0; i < colors.size(); ++i)
            colors[i] = ToRGBColor(worker->initial[i][4],worker->initial[i][5],worker->initial[i][6]);
    }
    ~RGBController_RazerBluetooth() override { Shutdown(); worker.reset(); }
    void SetupZones()
    {
        zones.clear(); leds.clear();
        for(auto id : worker->zones)
        {
            zone z;
            z.name = worker->keyboard ? "Keyboard (all keys)" : (id == 1 ? "Scroll Wheel" : "Underglow (group)");
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
        for(auto color : colors)
        {
            if(active_mode == 1) target.push_back({1,0,0,1,0,0,0,0,0,0,0});
            else if(active_mode == 2 && worker->keyboard)
                target.push_back({4,1,40,1,0,255,0,0,0,0,static_cast<uint8_t>(modes[2].brightness)});
            else target.push_back({1,0,0,1,static_cast<uint8_t>(RGBGetRValue(color)),static_cast<uint8_t>(RGBGetGValue(color)),static_cast<uint8_t>(RGBGetBValue(color)),0,0,0,static_cast<uint8_t>(modes[0].brightness)});
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
                    if(d.Name() == L"BSKV3P 35K" || d.Name() == L"BlackWidow V3 Mini") ids.push_back(d.Id());
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
