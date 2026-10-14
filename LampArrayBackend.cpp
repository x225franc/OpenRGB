#include "LampArrayBackend.h"
#include "LampArrayRecovery.h"
#include <map>
#include <algorithm>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <deque>
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Devices.Lights.h>
#include <winrt/Windows.UI.h>
using namespace winrt;
using namespace Windows::Devices::Lights;
struct LampArrayBackend::Impl
{
    using Clock=std::chrono::steady_clock;
    struct Frame { std::vector<RGBColor> colors; int brightness=100; QString status="Waiting for first frame"; };
    struct Opened {
        LampArray array{nullptr}; Device device; LampArrayRecovery recovery;
        std::vector<Windows::UI::Color> colors;std::vector<int32_t> indices;
        std::chrono::nanoseconds interval{16666667};Clock::time_point next=Clock::now(),measured=Clock::now();
        int frames=0;double fps=0;
    };
    std::mutex mutex;std::condition_variable wake;bool stopping=false, firstFramePending=false;
    std::map<QString,Frame> latest;
    std::map<QString,Opened> arrays; // Only accessed by the dedicated Windows lighting thread.
    std::deque<std::function<void()>> tasks;
    Clock::time_point recoveryStarted=Clock::now();
    std::thread worker;
    Impl():worker([this]{run();}){}
    ~Impl(){ {std::lock_guard<std::mutex> lock(mutex);stopping=true;}wake.notify_one();worker.join(); }
    template<class F> auto execute(F function)
    {
        using Result=decltype(function());
        auto task=std::make_shared<std::packaged_task<Result()>>(std::move(function));auto result=task->get_future();
        {std::lock_guard<std::mutex> lock(mutex);tasks.emplace_back([task]{(*task)();});}wake.notify_one();return result.get();
    }
    void clear()
    {
        for(auto& entry:arrays)try{entry.second.array.IsEnabled(false);}catch(...){}
        arrays.clear();std::lock_guard<std::mutex> lock(mutex);latest.clear();
    }
    void reopen(QString id, Opened& opened)
    {
        try{if(opened.array)opened.array.IsEnabled(false);}catch(...){}
        opened.array=nullptr;
        auto operation=LampArray::FromIdAsync(hstring(id.toStdWString()));
        if(operation.wait_for(std::chrono::seconds(5))!=Windows::Foundation::AsyncStatus::Completed)
        {
            operation.Cancel();throw std::runtime_error("Windows lighting reopen timed out");
        }
        auto array=operation.GetResults();
        if(!array)throw std::runtime_error("Windows lighting device is not ready");
        if(array.LampCount()!=int(opened.indices.size()))throw std::runtime_error("Lamp count changed; restart OpenRGB");
        array.IsEnabled(true);opened.array=array;
        opened.frames=0;opened.fps=0;opened.measured=Clock::now();
        opened.interval=std::max(std::chrono::nanoseconds(16666667),std::chrono::nanoseconds(array.MinUpdateInterval().count()*100));
        opened.recovery.Reopened(Clock::now());
    }
    void run()
    {
        init_apartment(apartment_type::multi_threaded);
        for(;;)
        {
            std::deque<std::function<void()>> pending;
            {std::lock_guard<std::mutex> lock(mutex);if(stopping)break;firstFramePending=false;pending.swap(tasks);}
            for(auto& task:pending)task();
            auto now=Clock::now();auto deadline=now+std::chrono::seconds(1);
            for(auto& entry:arrays)
            {
                auto& opened=entry.second;Frame frame;bool ready=false;
                {std::lock_guard<std::mutex> lock(mutex);auto it=latest.find(entry.first);if(it!=latest.end()&&!it->second.colors.empty()){frame=it->second;ready=true;}}
                if(!ready)continue;
                if(now>=opened.next)
                {
                    QString state;
                    try
                    {
                        if(opened.recovery.invalid && now<opened.recovery.retry)
                        {
                            opened.next=opened.recovery.retry;deadline=std::min(deadline,opened.next);continue;
                        }
                        bool available=opened.array && opened.array.IsAvailable();
                        if(opened.recovery.NeedsReopen(available,now))
                        {
                            reopen(entry.first,opened);
                            available=opened.array.IsAvailable();
                        }
                        // Like Toolkit, keep feeding the most recent image even while Windows arbitrates ownership.
                        // Never turn an availability check into a stopped producer.
                        if(frame.colors.size()!=opened.indices.size())state="Lamp count changed; restart OpenRGB";
                        else
                        {
                            double gain=std::clamp(frame.brightness/100.0,0.0,1.0);
                            for(size_t i=0;i<frame.colors.size();i++)
                            {
                                auto c=frame.colors[i];opened.colors[i]={255,uint8_t(std::round(RGBGetRValue(c)*gain)),uint8_t(std::round(RGBGetGValue(c)*gain)),uint8_t(std::round(RGBGetBValue(c)*gain))};
                            }
                            opened.array.SetColorsForIndices(opened.colors,opened.indices);
                            ++opened.frames;auto elapsed=std::chrono::duration<double>(now-opened.measured).count();
                            if(elapsed>=1){opened.fps=opened.frames/elapsed;opened.frames=0;opened.measured=now;}
                            state=(available?QString("Controlled by Windows Dynamic Lighting"):QString("Waiting for Windows background priority â€” place OpenRGB LightSync first"));
                            if(available)state+=QString(" | %1 FPS (max %2)").arg(opened.fps,0,'f',0).arg(1e9/opened.interval.count(),0,'f',0);
                        }
                    }
                    catch(hresult_error const& e){opened.recovery.Failed(Clock::now());state="Reconnecting Windows Dynamic Lighting: "+QString::fromWCharArray(e.message().c_str());}
                    catch(std::exception const& e){opened.recovery.Failed(Clock::now());state="Reconnecting Windows Dynamic Lighting: "+QString::fromUtf8(e.what());}
                    catch(...){opened.recovery.Failed(Clock::now());state="Reconnecting Windows Dynamic Lighting";}
                    {std::lock_guard<std::mutex> lock(mutex);latest[entry.first].status=state;}
                    opened.next+=opened.interval;
                    // Drop missed deadlines: no catch-up burst and no stale frame queue.
                    auto finished=Clock::now();if(opened.next<=finished)opened.next=finished+opened.interval;
                }
                deadline=std::min(deadline,opened.next);
            }
            std::unique_lock<std::mutex> lock(mutex);wake.wait_until(lock,deadline,[this]{return stopping||!tasks.empty()||firstFramePending;});
        }
        clear();uninit_apartment();
    }
};
LampArrayBackend::LampArrayBackend():impl(new Impl){}
LampArrayBackend::~LampArrayBackend()=default;
std::vector<LampArrayBackend::Device> LampArrayBackend::discover()
{
    return impl->execute([this]{
        std::vector<Device> result;std::vector<QString> present;
        auto found=Windows::Devices::Enumeration::DeviceInformation::FindAllAsync(LampArray::GetDeviceSelector()).get();
        for(auto info:found)
        {
            QString id=QString::fromWCharArray(info.Id().c_str());auto existing=impl->arrays.find(id);
            if(existing!=impl->arrays.end()){present.push_back(id);result.push_back(existing->second.device);continue;}
            auto array=LampArray::FromIdAsync(info.Id()).get();
            if(!array||array.LampArrayKind()!=LampArrayKind::Keyboard||array.LampCount()<=0)continue;
            if(array.HardwareVendorId()!=0x17EF&&array.HardwareVendorId()!=0x048D)continue;
            Impl::Opened opened;opened.recovery.Begin(impl->recoveryStarted);opened.array=array;opened.device.id=id;opened.device.name="Lenovo Lighting";
            float minX=1e9f,minY=1e9f,maxX=-1e9f,maxY=-1e9f;
            for(int i=0;i<array.LampCount();i++)
            {
                auto p=array.GetLampInfo(i).Position();opened.device.positions.emplace_back(p.x,p.y);
                minX=std::min(minX,p.x);maxX=std::max(maxX,p.x);minY=std::min(minY,p.y);maxY=std::max(maxY,p.y);
                opened.indices.push_back(i);
            }
            for(auto& p:opened.device.positions)p=QPointF((p.x()-minX)/std::max(0.0001f,maxX-minX),(p.y()-minY)/std::max(0.0001f,maxY-minY));
            opened.colors.resize(opened.indices.size());
            auto hardware=std::chrono::nanoseconds(array.MinUpdateInterval().count()*100);
            opened.interval=std::max(std::chrono::nanoseconds(16666667),hardware);
            array.IsEnabled(true);present.push_back(id);result.push_back(opened.device);impl->arrays.emplace(id,std::move(opened));
        }
        for(auto it=impl->arrays.begin();it!=impl->arrays.end();){if(std::find(present.begin(),present.end(),it->first)==present.end())it=impl->arrays.erase(it);else ++it;}
        return result;
    });
}
QString LampArrayBackend::send(QString id,const std::vector<RGBColor>& rgb,int brightness)
{
    QString state;
    {std::lock_guard<std::mutex> lock(impl->mutex);auto& frame=impl->latest[id];if(frame.colors.empty())impl->firstFramePending=true;frame.colors=rgb;frame.brightness=brightness;state=frame.status;}
    impl->wake.notify_one();return state;
}
void LampArrayBackend::release(){impl->execute([this]{impl->clear();});}
void LampArrayBackend::resume(){impl->execute([this]{impl->clear();impl->recoveryStarted=Impl::Clock::now();});}
