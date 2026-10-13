#include "RazerController.h"
#include "RazerDevices.h"
#include <atomic>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

struct Fixture
{
    razer_report request{};
    std::vector<razer_report> sent;
    bool corrupt = false, wrong_target = false, wrong_command = false, short_read = false, failed = false;
    unsigned int busy = 0, stale = 0, reads = 0, transaction_reads = 0, closes = 0;
    bool empty_ack = false;
};
static std::atomic<int> pending{0}, collisions{0};
static bool usb_connected = false;
static Fixture usb_fixture;
extern "C" hid_device_info* hid_enumerate(unsigned short, unsigned short pid)
{
    if(!usb_connected || pid != RAZER_BLACKWIDOW_V3_MINI_WIRED_PID) return nullptr;
    auto* info = new hid_device_info{};
    info->path = _strdup("usb-keyboard");
    info->interface_number = 3; info->usage_page = 0x0C; info->usage = 1;
    return info;
}
extern "C" void hid_free_enumeration(hid_device_info* info)
{
    if(info) { free(info->path); delete info; }
}
extern "C" hid_device* hid_open_path(const char*) { return reinterpret_cast<hid_device*>(&usb_fixture); }
static unsigned char Crc(const razer_report& report)
{
    const auto* bytes = reinterpret_cast<const unsigned char*>(&report);
    unsigned char crc = 0;
    for(unsigned int i = 3; i < sizeof(report)-2; i++) crc ^= bytes[i];
    return crc;
}
extern "C" int hid_send_feature_report(hid_device* handle, const unsigned char* data, size_t length)
{
    auto& fixture = *reinterpret_cast<Fixture*>(handle);
    if(length != sizeof(razer_report)) return -1;
    memcpy(&fixture.request, data, length);
    fixture.sent.push_back(fixture.request);
    fixture.transaction_reads = 0;
    if(fixture.request.command_id.id & 0x80 || fixture.request.status == 0)
    {
        if(pending.fetch_add(1) != 0) collisions++;
    }
    return static_cast<int>(length);
}
extern "C" int hid_get_feature_report(hid_device* handle, unsigned char* data, size_t length)
{
    auto& fixture = *reinterpret_cast<Fixture*>(handle);
    fixture.reads++;
    fixture.transaction_reads++;
    razer_report response = fixture.request;
    response.status = fixture.failed ? 3 : 2;
    memset(response.arguments, 0, sizeof(response.arguments));
    if(response.command_id.id == 0x81) { response.arguments[0] = 2; response.arguments[1] = 51; }
    if(response.command_id.id == 0x82) memcpy(response.arguments, "TEST_SERIAL", 11);
    if(response.command_id.id == 0x86) { response.arguments[0] = 4; response.data_size = 2; }
    if(response.command_class == 0x07 && response.command_id.id == 0x80)
        response.arguments[1] = response.transaction_id.id == 0x9F ? 255 : 215;
    if(response.command_class == 0x07 && response.command_id.id == 0x84)
        response.arguments[1] = usb_connected ? 1 : 0;
    if(fixture.wrong_target) response.transaction_id.id ^= 0x80;
    if(fixture.wrong_command) response.command_id.id ^= 1;
    if(fixture.empty_ack && !(response.command_id.id & 0x80)) response.data_size = 0;
    if(fixture.stale)
    {
        fixture.stale--;
        response.command_id.id ^= 1;
        if(fixture.transaction_reads == 16) pending--;
    }
    else if(fixture.busy)
    {
        fixture.busy--;
        response.status = 1;
        if(fixture.transaction_reads == 16) pending--;
    }
    else pending--;
    response.crc = Crc(response);
    if(fixture.corrupt) response.crc ^= 1;
    memcpy(data, &response, length);
    return fixture.short_read ? 10 : static_cast<int>(length);
}
extern "C" void hid_close(hid_device* handle)
{
    reinterpret_cast<Fixture*>(handle)->closes++;
}
static void Check(bool ok, const char* message)
{
    if(!ok) throw std::runtime_error(message);
    std::cout << "PASS " << message << '\n';
}
int main()
{
    try
    {
        Fixture mouse, keyboard;
        {
            RazerController m(reinterpret_cast<hid_device*>(&mouse), reinterpret_cast<hid_device*>(&mouse),
                              "receiver", RAZER_BASILISK_V3_PRO_35K_PG_WIRELESS_PID, "mouse");
            RazerController k(reinterpret_cast<hid_device*>(&keyboard), reinterpret_cast<hid_device*>(&keyboard),
                              "receiver", RAZER_BLACKWIDOW_V3_MINI_WIRELESS_PID, "keyboard");
            Check(k.ProbeSharedKeyboard(), "secondary target verifies AZERTY identity and serial");
            Check(keyboard.sent[0].transaction_id.id == 0x9F, "keyboard commands address 9F");
            Check(m.GetFirmwareString() == "v2.51" && mouse.sent.back().transaction_id.id == 0x1F,
                  "mouse commands address 1F independently");
            const auto keyboard_battery = k.ReadBattery(), mouse_battery = m.ReadBattery();
            Check(keyboard_battery.percent == 100 && keyboard_battery.reachable && keyboard_battery.charging == 0,
                  "battery GET reads the secondary keyboard independently");
            Check(mouse_battery.percent == 84 && mouse_battery.reachable && mouse_battery.transport == "HyperSpeed",
                  "battery GET converts mouse raw charge to percent without a write command");
            mouse.corrupt = true;
            Check(!m.ReadBattery().reachable && m.ReadBattery().percent == -1, "bad battery CRC is unavailable, never a fake zero percent");
            mouse.corrupt = false;
            RazerBatteryCache cache;
            cache.Enable("HyperSpeed"); cache.Publish(mouse_battery);
            const auto last_good = cache.Snapshot();
            cache.Publish({});
            Check(cache.Snapshot().percent == 84 && !cache.Snapshot().reachable && cache.Snapshot().updated == last_good.updated,
                  "failed battery polling retains last reading and its original timestamp");
            RazerBatteryMonitor monitor;
            std::atomic<int> polls{0};
            monitor.Start([&]{ polls++; return mouse_battery; });
            const auto deadline = std::chrono::steady_clock::now()+std::chrono::seconds(1);
            while(polls == 0 && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(std::chrono::milliseconds(1));
            const auto stopping = std::chrono::steady_clock::now();
            monitor.Stop();
            Check(polls == 1 && std::chrono::steady_clock::now()-stopping < std::chrono::milliseconds(500),
                  "battery worker stops without waiting for its 30 second polling interval");
            for(int fault = 0; fault < 5; fault++)
            {
                mouse.corrupt = fault == 0; mouse.wrong_target = fault == 1;
                mouse.wrong_command = fault == 2; mouse.short_read = fault == 3; mouse.failed = fault == 4;
                Check(m.GetFirmwareString().empty(), "invalid, mismatched or failed GET is rejected");
            }
            mouse.corrupt = mouse.wrong_target = mouse.wrong_command = mouse.short_read = mouse.failed = false;
            mouse.busy = 2;
            Check(m.GetFirmwareString() == "v2.51", "BUSY response retries and then succeeds");
            mouse.busy = 16;
            unsigned int before = mouse.reads;
            Check(m.GetFirmwareString().empty() && mouse.reads-before == 16, "BUSY retries are bounded to sixteen reads");
            RGBColor colors[12];
            for(unsigned int i = 0; i < 12; i++) colors[i] = ToRGBColor(i, i+20, i+40);
            mouse.sent.clear();
            m.SetLEDs(colors);
            Check(mouse.sent.size() == 2 && mouse.sent[0].arguments[4] == 11,
                  "Phantom Green custom frame ends at physical column 11");
            Check(mouse.sent[0].arguments[5+11*3] == 11 && mouse.sent[0].data_size == 41,
                  "last strip LED is transmitted without a thirteenth color");
            RGBColor keyboard_colors[80];
            for(auto& color : keyboard_colors) color = ToRGBColor(0,255,0);
            keyboard.sent.clear();
            k.SetLEDs(keyboard_colors);
            Check(keyboard.sent.size() == 6, "Mini keyboard sends five rows followed by custom mode");
            bool padded_rows = true;
            for(unsigned int row = 0; row < 5; row++)
            {
                const auto& report = keyboard.sent[row];
                padded_rows &= report.data_size == 0x47 && report.arguments[2] == row
                            && report.arguments[4] == 15 && report.arguments[5+15*3+1] == 255;
                for(unsigned int i = 53; i < 71; i++) padded_rows &= report.arguments[i] == 0;
            }
            Check(padded_rows, "Mini keyboard preserves RGB and zero-pads each extended row to 0x47 bytes");
            Fixture wired_keyboard;
            {
                RazerController wired(reinterpret_cast<hid_device*>(&wired_keyboard), reinterpret_cast<hid_device*>(&wired_keyboard),
                                      "usb", RAZER_BLACKWIDOW_V3_MINI_WIRED_PID, "keyboard");
                wired.SetLEDs(keyboard_colors);
                Check(wired_keyboard.sent[0].data_size == 0x47 && wired_keyboard.sent[0].transaction_id.id == 0x1F,
                      "wired Mini uses the same padded row format with the wired target");
            }
            collisions = 0; pending = 0;
            std::thread a([&]{for(int i=0;i<12;i++) m.GetFirmwareString();});
            std::thread b([&]{for(int i=0;i<12;i++) k.GetFirmwareString();});
            a.join(); b.join();
            Check(collisions == 0, "shared Windows mutex covers the complete concurrent send/receive transaction");
        }
        Check(mouse.closes == 1 && keyboard.closes == 1, "independent owned handles close exactly once");
        Fixture acknowledged;
        acknowledged.empty_ack = true;
        {
            RazerController shared(reinterpret_cast<hid_device*>(&acknowledged), reinterpret_cast<hid_device*>(&acknowledged),
                                   "receiver", RAZER_BASILISK_V3_PRO_35K_PG_WIRELESS_PID, "mouse", true);
            shared.SetModeStatic(255,0,255);
            Check(acknowledged.reads == 2 && acknowledged.sent.size() == 2,
                  "shared RGB write consumes its own ACK after the presence check");
            acknowledged.stale = 2;
            Check(shared.GetFirmwareString() == "v2.51", "stale previous ACKs are ignored until the requested response arrives");
        }
        Fixture missing;
        missing.failed = true;
        RazerController unavailable(reinterpret_cast<hid_device*>(&missing), reinterpret_cast<hid_device*>(&missing),
                                    "receiver", RAZER_BLACKWIDOW_V3_MINI_WIRELESS_PID, "keyboard", true);
        unavailable.SetModeStatic(0,255,0);
        Check(missing.sent.size() == 1 && missing.sent[0].command_id.id == 0x81,
              "unresponsive shared target receives no RGB write");
        usb_connected = true;
        std::this_thread::sleep_for(std::chrono::milliseconds(1050));
        Check(unavailable.IsAvailable() && usb_fixture.sent.back().transaction_id.id == 0x1F,
              "USB arrival routes the shared keyboard to the wired target without a new controller");
        usb_connected = false;
        std::this_thread::sleep_for(std::chrono::milliseconds(1050));
        Check(!unavailable.IsAvailable() && usb_fixture.closes == 1,
              "USB removal closes its handle and falls back to unavailable RF safely");
        missing.failed = false;
        std::this_thread::sleep_for(std::chrono::milliseconds(1050));
        Check(unavailable.IsAvailable() && missing.sent.back().transaction_id.id == 0x9F,
              "RF reconnection restores the original keyboard target");
        return 0;
    }
    catch(const std::exception& error) { std::cerr << "FAIL " << error.what() << '\n'; return 1; }
}
