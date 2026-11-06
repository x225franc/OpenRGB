/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "RGBController_FlydigiBS2Pro.h"
#include "FlydigiBS2ProModes.h"
#include "HidApiLifecycle.h"
#include "LogManager.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
namespace
{
using namespace FlydigiBS2Pro;
struct Scene {Bytes table;bool off=false;bool operator==(const Scene& other)const{return off==other.off&&table==other.table;}};
bool USBPresent()
{
    auto list=HidApiLifecycle::Enumerate(0x37D7,0x1002);bool found=false;
    for(auto item=list;item;item=item->next)
        if(item->bus_type==HID_API_BUS_USB&&item->interface_number==0&&item->usage_page==0xFFA0&&item->usage==1)found=true;
    hid_free_enumeration(list);return found;
}
}
// Single owner for HID, one in-flight scene and one replaceable latest scene.
// Native effects run on the device after one acknowledged transfer.
class FlydigiBS2Worker
{
    hid_device* device;
    Transport transport;
    std::atomic<bool> stop{false};
    std::mutex mutex;std::condition_variable changed;
    bool pending=false;Scene desired;std::thread thread;
    std::chrono::steady_clock::time_point last_submit{},flash_ready{};
    bool WaitFlashWindow()
    {
        std::unique_lock<std::mutex> lock(mutex);
        changed.wait_until(lock,flash_ready,[this]{return bool(stop);});return !stop;
    }
    void NoteFlash(){flash_ready=std::chrono::steady_clock::now()+std::chrono::milliseconds(200);}
    bool Request(RgbCommand command,const Bytes& payload,uint8_t& value)
    {
        std::array<uint8_t,64> input{};
        for(unsigned i=0;i<32;++i)if(hid_read_timeout(device,input.data(),input.size(),0)<=0)break;
        const auto report=HidReport(command,payload,transport);
        if(hid_write(device,report.data(),report.size())!=int(report.size()))return false;
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(1);
        while(!stop&&std::chrono::steady_clock::now()<deadline)
        {
            const int count=hid_read_timeout(device,input.data(),input.size(),20);
            if(count<0)return false;
            if(count&&ParseReply(Bytes(input.begin(),input.begin()+count),transport,command,value))return true;
        }
        return false;
    }
    bool Apply(const Scene& scene)
    {
        if(!WaitFlashWindow())return false;
        uint8_t value=0;
        if(!Request(RgbCommand::GetSwitch,{},value))return false;
        if(scene.off)
        {
            if(value==0)return true;
            const bool ok=Request(RgbCommand::SetSwitch,{0},value)&&value==1;
            NoteFlash();return ok;
        }
        if(value==0)
        {
            const bool ok=Request(RgbCommand::SetSwitch,{1},value)&&value==1;
            NoteFlash();if(!ok||!WaitFlashWindow())return false;
        }
        for(const auto& step:PlanRgbUpload(scene.table))
        {
            const size_t payload_length=step.frame[3]-2;
            Bytes payload(step.frame.begin()+4,step.frame.begin()+4+payload_length);
            if(!Request(step.command,payload,value)||value!=1)return false;
            if(step.command==RgbCommand::Finalize)NoteFlash();
        }
        return true;
    }
    void Run()
    {
        std::unique_lock<std::mutex> lock(mutex);Scene sent;bool have_sent=false;
        while(!stop)
        {
            changed.wait(lock,[this]{return stop||pending;});if(stop)break;
            // RGB apply persists to flash. Wait for editing to settle so dragging
            // a slider/color picker produces one final upload rather than many saves.
            while(!stop&&!desired.off)
            {
                const auto observed=last_submit;
                if(!changed.wait_until(lock,observed+std::chrono::milliseconds(500),[this,observed]{return stop||desired.off||last_submit!=observed;}))break;
            }
            if(stop)break;
            Scene target=desired;pending=false;
            if(have_sent&&target==sent)continue;
            lock.unlock();
            const bool deferred=transport==Transport::Bluetooth&&USBPresent();
            const bool ok=!deferred&&Apply(target);
            lock.lock();
            if(deferred)
            {
                pending=true;changed.wait_for(lock,std::chrono::seconds(1),[this]{return bool(stop);});
            }
            else if(ok){sent=std::move(target);have_sent=true;}
            else
            {
                // Do not advance/retry a partially acknowledged transfer on a timer.
                have_sent=false;LOG_WARNING("[Flydigi BS2 Pro] RGB transfer stopped: matching success ACK missing");
            }
        }
    }
public:
    FlydigiBS2Worker(hid_device* value,bool bluetooth):device(value),transport(bluetooth?Transport::Bluetooth:Transport::USB)
    {thread=std::thread([this]{Run();});}
    ~FlydigiBS2Worker(){stop=true;changed.notify_all();if(thread.joinable())thread.join();HidApiLifecycle::Close(device);}
    void Submit(Scene scene){{std::lock_guard<std::mutex> lock(mutex);desired=std::move(scene);last_submit=std::chrono::steady_clock::now();pending=true;}changed.notify_one();}
};
RGBController_FlydigiBS2Pro::RGBController_FlydigiBS2Pro(hid_device* device,const char* path,bool bluetooth)
{
    name=bluetooth?"Flydigi BS2 Pro (Bluetooth)":"Flydigi BS2 Pro (USB)";vendor="Flydigi";type=DEVICE_TYPE_COOLER;
    description="Six native hardware RGB modes. Direct/Ambient unavailable with firmware 0.0.3.5: custom color apply persists to flash. Fan settings are not controlled.";location=std::string("HID: ")+path;
    auto add=[this](const char* title,unsigned value,unsigned maximum,bool speed)
    {
        mode item;item.name=title;item.value=value;item.color_mode=maximum?MODE_COLORS_MODE_SPECIFIC:MODE_COLORS_NONE;
        item.flags=(maximum?MODE_FLAG_HAS_MODE_SPECIFIC_COLOR:0)|(value==Off?0:MODE_FLAG_HAS_BRIGHTNESS)|(speed?MODE_FLAG_HAS_SPEED:0);
        item.brightness_min=0;item.brightness_max=100;item.brightness=30;
        item.speed_min=1;item.speed_max=10;item.speed=5;
        if(maximum){item.colors_min=1;item.colors_max=maximum;item.colors=value==MultiStatic?std::vector<RGBColor>{ToRGBColor(255,0,0),ToRGBColor(0,255,0),ToRGBColor(0,0,255)}:std::vector<RGBColor>{ToRGBColor(0,255,0)};}
        modes.push_back(item);
    };
    add("Off",Off,0,false);add("Flow",Flow,0,true);add("Rotate",Rotate,3,true);add("Breath",Breath,5,true);
    add("Single Static",SingleStatic,1,false);add("Multi Static",MultiStatic,6,false);
    active_mode=4;
    for(const char* side:{"Left","Right"})
    {
        zone strip;strip.name=side;strip.type=ZONE_TYPE_LINEAR;strip.leds_min=strip.leds_max=strip.leds_count=3;zones.push_back(strip);
        for(const char* position:{"Top","Middle","Bottom"}){led item;item.name=std::string(side)+" "+position;leds.push_back(item);}
    }
    SetupColors();
    worker=std::make_unique<FlydigiBS2Worker>(device,bluetooth);
}
RGBController_FlydigiBS2Pro::~RGBController_FlydigiBS2Pro(){Shutdown();worker.reset();}
void RGBController_FlydigiBS2Pro::DeviceUpdateLEDs()
{
    if(active_mode<0||size_t(active_mode)>=modes.size())return;
    const auto& selected=modes[active_mode];std::vector<Color> palette;
    for(auto color:selected.colors)palette.push_back({uint8_t(RGBGetRValue(color)),uint8_t(RGBGetGValue(color)),uint8_t(RGBGetBValue(color))});
    if(palette.empty())palette.push_back({0,255,0});
    Scene scene;scene.off=selected.value==Off;
    if(!scene.off)scene.table=NativeTable(selected.value,palette,uint8_t(std::min(100u,selected.brightness)),uint8_t(std::clamp(selected.speed,1u,10u)));
    worker->Submit(std::move(scene));
}
void RGBController_FlydigiBS2Pro::DeviceUpdateMode(){DeviceUpdateLEDs();}
void RGBController_FlydigiBS2Pro::DeviceUpdateZoneLEDs(int){DeviceUpdateLEDs();}
void RGBController_FlydigiBS2Pro::DeviceUpdateSingleLED(int){DeviceUpdateLEDs();}
