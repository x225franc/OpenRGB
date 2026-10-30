/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "RGBController_FlydigiApex5.h"
#include "FlydigiApex5Protocol.h"
#include "FlydigiApex5Presets.h"
#include "FlydigiApex5Transport.h"
#include "HidApiLifecycle.h"
#include "LogManager.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace
{
using namespace FlydigiApex5;
enum Effect { Direct=0, Off=1, Static=2, Breathing=3, Gradient=4, Default=5, DynamicGlow=6 };
struct Scene
{
    Bytes data;
    bool direct=false;
    bool operator==(const Scene& other) const { return direct==other.direct && data==other.data; }
};
void AppendColor(Bytes& data, RGBColor color)
{
    for(unsigned int led=0;led<LedCount;++led)
    {
        data.push_back(RGBGetRValue(color));data.push_back(RGBGetGValue(color));data.push_back(RGBGetBValue(color));
    }
}
}

// All HID I/O is owned by this worker. One latest scene, no queued stale frames.
class FlydigiApex5Worker
{
    hid_device* device;
    std::string path;
    bool wireless;
    std::atomic<bool> stop{false};
    std::mutex mutex;
    std::condition_variable changed;
    bool pending=false;
    Scene desired;
    std::thread thread;
public:
    FlydigiApex5Worker(hid_device* value,const char* device_path,bool is_wireless=false) : device(value),path(device_path),wireless(is_wireless)
    {
        thread=std::thread([this]{ Run(); });
    }
    ~FlydigiApex5Worker()
    {
        stop=true;changed.notify_all();if(thread.joinable())thread.join();
        if(device)HidApiLifecycle::Close(device);
    }
    void Submit(Scene value)
    {
        {std::lock_guard<std::mutex> lock(mutex);desired=std::move(value);pending=true;}
        changed.notify_one();
    }
private:
    bool Write(uint8_t profile,uint8_t start,const Bytes& bytes)
    {
        if(!device)device=HidApiLifecycle::OpenPath(path.c_str());
        if(!device)return false;
        const auto reports=Transfer(profile,start,bytes);
        for(size_t index=0;index<reports.size();++index)
        {
            const auto& report=reports[index];
            if(stop)return false;
            if(hid_write(device,report.data(),report.size())!=static_cast<int>(report.size()))
            {
                HidApiLifecycle::Close(device);device=nullptr;return false;
            }
            if(index+1<reports.size())std::this_thread::sleep_for(std::chrono::milliseconds(12));
        }
        return true;
    }
    bool ReadActiveProfile(uint8_t& profile)
    {
        if(!device)device=HidApiLifecycle::OpenPath(path.c_str());
        if(!device)return false;
        std::array<uint8_t,64> input{};
        // Discard stale replies before the read-only A1 request. Never default to slot zero.
        for(unsigned int i=0;i<64;++i)
            if(hid_read_timeout(device,input.data(),input.size(),0)<=0)break;
        Report query{};query[0]=3;query[1]=0x5A;query[2]=0xA5;
        query[3]=0xA1;query[4]=2;query[5]=0xA3;
        if(hid_write(device,query.data(),query.size())!=int(query.size()))return false;
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(150);
        while(!stop && std::chrono::steady_clock::now()<deadline)
        {
            const int count=hid_read_timeout(device,input.data(),input.size(),15);
            if(count<0)return false;
            const size_t offset=count>0&&input[0]==4?1:0;
            if(count<int(offset+15))continue;
            const auto* data=input.data()+offset;
            if(data[0]!=0x5A||data[1]!=0xA5||data[2]!=0xA1)continue;
            // Only the four observed PC slots. Other modes remain untouched.
            if(data[5]>3)return false;
            profile=data[5];return true;
        }
        return false;
    }
    void Run()
    {
        using Clock=std::chrono::steady_clock;
        Scene sent;uint8_t sent_profile=255;bool have_sent=false;bool usb_blocked=false;
        unsigned int output_frames=0;auto sample_start=Clock::time_point{};
        auto next=Clock::time_point{},last_configuration=Clock::time_point{},warning=Clock::time_point{};
        std::unique_lock<std::mutex> lock(mutex);
        while(!stop)
        {
            changed.wait_for(lock,std::chrono::seconds(1),[this]{return stop||pending;});if(stop)break;
            if(wireless&&(have_sent||pending))
            {
                lock.unlock();const bool blocked=WiredPresent();lock.lock();if(stop)break;
                if(blocked)
                {
                    if(!usb_blocked)LOG_DEBUG("[Flydigi Apex 5] USB has priority; receiver lighting writes suspended");
                    usb_blocked=true;pending=true;
                    changed.wait_for(lock,std::chrono::seconds(1),[this]{return bool(stop);});continue;
                }
                if(usb_blocked){have_sent=false;pending=true;usb_blocked=false;}
            }
            // Direct is volatile: the receiver can remain enumerated while the
            // pad sleeps and restores its onboard RGB. Revisit an explicitly
            // owned Direct scene even when the effect sends no new pixels.
            // Native autonomous modes and unclaimed devices remain idle.
            if(!pending && have_sent && desired.direct)pending=true;
            if(!pending)continue;
            changed.wait_until(lock,next,[this]{return bool(stop);});if(stop)break;
            Scene target=desired;pending=false;
            const auto now=Clock::now();
            // Refresh only an explicitly requested Direct scene; no battery or
            // input polling and no writes for unclaimed/native-mode devices.
            const bool refresh=target.direct && now-last_configuration>=std::chrono::seconds(2);
            uint8_t profile=255;
            lock.unlock();const bool profile_known=ReadActiveProfile(profile);lock.lock();
            if(stop)break;
            if(!profile_known)
            {
                if(device){HidApiLifecycle::Close(device);device=nullptr;}
                have_sent=false;pending=true;next=Clock::now()+std::chrono::seconds(1);
                if(Clock::now()>=warning)
                {
                    LOG_WARNING("[Flydigi Apex 5] Active profile unverified; RGB writes suspended");
                    warning=Clock::now()+std::chrono::seconds(10);
                }
                continue;
            }
            if(profile!=sent_profile)have_sent=false;
            if(have_sent && target==sent && !refresh)continue;
            const bool metadata_changed=!have_sent || target.direct!=sent.direct || target.data.size()!=sent.data.size()
                || !std::equal(target.data.begin(),target.data.begin()+20,sent.data.begin());
            lock.unlock();
            bool ok=false;
            try
            {
                if(target.direct)
                {
                    ok=true;
                    if(metadata_changed||refresh)
                    {
                        ok=Write(profile,0,Bytes(target.data.begin(),target.data.begin()+20));
                        if(ok)last_configuration=Clock::now();
                    }
                    if(ok)ok=Write(profile,1,Bytes(target.data.begin()+20,target.data.end()));
                }
                else ok=Write(profile,0,target.data);
            }
            catch(...) {ok=false;}
            lock.lock();
            // Stable start-to-start limiter; do not add HID transfer time to each frame interval.
            next=now+std::chrono::milliseconds(target.direct?50:100);
            if(ok)
            {
                if(target.direct)
                {
                    if(sample_start==Clock::time_point{})sample_start=now;
                    ++output_frames;
                    const double seconds=std::chrono::duration<double>(Clock::now()-sample_start).count();
                    if(seconds>=5.0)
                    {
                        LOG_DEBUG("[Flydigi Apex 5] Direct output: %u scenes in %.2f seconds",output_frames,seconds);
                        output_frames=0;sample_start=Clock::now();
                    }
                }
                else{output_frames=0;sample_start=Clock::time_point{};}
                sent=std::move(target);sent_profile=profile;have_sent=true;
            }
            else
            {
                have_sent=false;
                if(Clock::now()>=warning)
                {
                    LOG_WARNING("[Flydigi Apex 5] Lighting write failed; preserving latest requested scene");
                    warning=Clock::now()+std::chrono::seconds(10);
                }
                pending=true;next=Clock::now()+std::chrono::seconds(1);
            }
        }
    }
};

RGBController_FlydigiApex5::RGBController_FlydigiApex5(hid_device* device,const char* path,bool wireless)
{
    name=wireless?"Flydigi APEX 5 (2.4 GHz)":"Flydigi APEX 5 (USB)";vendor="Flydigi";type=DEVICE_TYPE_GAMEPAD;
    description="Native lighting only; onboard game profiles and input mappings are not modified";
    location=std::string("HID: ")+path;
    auto add=[&](const char* title,unsigned int value,int color_mode,unsigned int flags)
    {
        mode effect;effect.name=title;effect.value=value;effect.flags=flags;
        effect.color_mode=color_mode;effect.brightness_min=0;effect.brightness_max=100;effect.brightness=30;
        if(flags&MODE_FLAG_HAS_SPEED){effect.speed_min=1;effect.speed_max=100;effect.speed=86;}
        if(color_mode==MODE_COLORS_MODE_SPECIFIC)
        {
            effect.colors_min=value==Gradient?2:1;effect.colors_max=value==Static?1:5;
            effect.colors=value==Gradient?std::vector<RGBColor>{ToRGBColor(0,0,255),ToRGBColor(255,0,0),ToRGBColor(0,255,0)}
                                         :std::vector<RGBColor>{ToRGBColor(0,255,0)};
        }
        modes.push_back(effect);
    };
    add("Direct",Direct,MODE_COLORS_PER_LED,MODE_FLAG_HAS_PER_LED_COLOR|MODE_FLAG_HAS_BRIGHTNESS);
    add("Off",Off,MODE_COLORS_NONE,0);
    add("Static",Static,MODE_COLORS_MODE_SPECIFIC,MODE_FLAG_HAS_MODE_SPECIFIC_COLOR|MODE_FLAG_HAS_BRIGHTNESS);
    const unsigned int native_flags=MODE_FLAG_HAS_MODE_SPECIFIC_COLOR|MODE_FLAG_HAS_BRIGHTNESS|MODE_FLAG_HAS_SPEED;
    add("Breathing",Breathing,MODE_COLORS_MODE_SPECIFIC,native_flags);
    add("Gradient",Gradient,MODE_COLORS_MODE_SPECIFIC,native_flags);
    add("Default",Default,MODE_COLORS_NONE,MODE_FLAG_HAS_BRIGHTNESS|MODE_FLAG_HAS_SPEED);
    add("Dynamic glow",DynamicGlow,MODE_COLORS_NONE,MODE_FLAG_HAS_BRIGHTNESS|MODE_FLAG_HAS_SPEED);
    SetupZones();
    worker=std::make_unique<FlydigiApex5Worker>(device,path,wireless);
}
RGBController_FlydigiApex5::~RGBController_FlydigiApex5(){Shutdown();worker.reset();}
void RGBController_FlydigiApex5::SetupZones()
{
    zones.clear();leds.clear();zone strip;strip.name="Light strip";strip.type=ZONE_TYPE_LINEAR;
    strip.leds_min=strip.leds_max=strip.leds_count=LedCount;zones.push_back(strip);
    for(unsigned int i=0;i<LedCount;++i){led item;item.name="LED "+std::to_string(i+1);leds.push_back(item);}
    SetupColors();
}
void RGBController_FlydigiApex5::DeviceUpdateLEDs()
{
    const auto& effect=modes[active_mode];Scene scene;
    const uint8_t brightness=static_cast<uint8_t>(std::min<unsigned int>(100,effect.brightness));
    const uint8_t period=static_cast<uint8_t>(101-std::clamp<unsigned int>(effect.speed,1,100));
    if(effect.value==Default||effect.value==DynamicGlow)
    {
        scene.data=effect.value==Default?DefaultPattern:DynamicGlowPattern;
        scene.data[5]=period;scene.data[6]=brightness;
    }
    else
    {
        uint8_t native=effect.value==Breathing?2:effect.value==Gradient?3:effect.value==Off?6:5;
        Bytes pixels;
        if(effect.value==Direct)
        {
            scene.direct=true;
            for(RGBColor color:colors){pixels.push_back(RGBGetRValue(color));pixels.push_back(RGBGetGValue(color));pixels.push_back(RGBGetBValue(color));}
        }
        else if(effect.value==Off)AppendColor(pixels,0);
        else
        {
            const size_t count=std::min<size_t>(effect.colors.size(),effect.value==Static?1:5);
            for(size_t i=0;i<count;++i){AppendColor(pixels,effect.colors[i]);if(effect.value==Breathing)AppendColor(pixels,0);}
        }
        if(pixels.empty())AppendColor(pixels,0);
        scene.data=Metadata(native,effect.value==Off?0:brightness,(effect.value==Direct||effect.value==Static||effect.value==Off)?1:period,
                            static_cast<uint8_t>(pixels.size()/(LedCount*3)-1));
        scene.data.insert(scene.data.end(),pixels.begin(),pixels.end());
    }
    worker->Submit(std::move(scene));
}
void RGBController_FlydigiApex5::DeviceUpdateMode(){DeviceUpdateLEDs();}
void RGBController_FlydigiApex5::DeviceUpdateZoneLEDs(int){DeviceUpdateLEDs();}
void RGBController_FlydigiApex5::DeviceUpdateSingleLED(int){DeviceUpdateLEDs();}
