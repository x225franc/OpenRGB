/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <chrono>
#include <algorithm>
#include <cmath>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>
#include "RGBControllerInterface.h"
namespace SoftwareLighting
{
inline double BreathingLevel(double elapsed, double period)
{
    constexpr double pi = 3.14159265358979323846;
    return 0.5 + 0.5 * std::cos(2.0 * pi * elapsed / std::max(0.5, period));
}
inline RGBColor Scale(RGBColor color, double amount)
{
    return ToRGBColor(int(std::round(RGBGetRValue(color)*amount)),
        int(std::round(RGBGetGValue(color)*amount)),int(std::round(RGBGetBValue(color)*amount)));
}
inline RGBColor CycleColor(double elapsed,double period)
{
    const double phase=std::fmod(std::max(0.0,elapsed)/std::max(0.5,period),1.0)*6;
    const int index=int(phase);const int up=int(std::round((phase-index)*255));const int down=255-up;
    switch(index){case 0:return ToRGBColor(255,up,0);case 1:return ToRGBColor(down,255,0);case 2:return ToRGBColor(0,255,up);case 3:return ToRGBColor(0,down,255);case 4:return ToRGBColor(up,0,255);default:return ToRGBColor(255,0,down);}
}
// Explicit software mode only. Stop() waits for the last send before a native
// mode is applied; an old animation can never overwrite a subsequent Off/Static.
class Worker
{
    std::mutex mutex;
    std::condition_variable changed;
    bool stopping = false, active = false;
    unsigned revision = 0;
    std::vector<RGBColor> colors;
    double period = 3;
    bool cycle = false;
    std::chrono::milliseconds cadence;
    std::chrono::steady_clock::time_point started;
    std::function<void(std::vector<RGBColor>&)> send;
    std::thread thread;
public:
    explicit Worker(std::function<void(std::vector<RGBColor>&)> callback, std::chrono::milliseconds interval=std::chrono::milliseconds(50)) : cadence(interval),send(std::move(callback)),thread([this]{
        std::unique_lock<std::mutex> lock(mutex);
        while(!stopping)
        {
            changed.wait(lock,[this]{return stopping||active;});
            if(stopping)break;
            const auto version=revision;
            auto frame=colors;
            const double time=std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count();
            for(auto& color:frame)if(color)color=cycle?Scale(CycleColor(time,period),std::max({RGBGetRValue(color),RGBGetGValue(color),RGBGetBValue(color)})/255.0):Scale(color,BreathingLevel(time,period));
            try {send(frame);} catch(...) {active=false;}
            changed.wait_for(lock,cadence,[this,version]{return stopping||!active||revision!=version;});
        }
    }) {}
    ~Worker(){Shutdown();}
    void Set(std::vector<RGBColor> value,int speed,bool colorCycle=false)
    {
        std::lock_guard<std::mutex> lock(mutex);colors=std::move(value);
        cycle=colorCycle;period=(11-std::clamp(speed,1,10))*0.5;started=std::chrono::steady_clock::now();
        active=true;++revision;changed.notify_all();
    }
    void Stop(){std::lock_guard<std::mutex> lock(mutex);active=false;++revision;changed.notify_all();}
    void Shutdown()
    {
        {std::lock_guard<std::mutex> lock(mutex);stopping=true;active=false;changed.notify_all();}
        if(thread.joinable())thread.join();
    }
};
}
