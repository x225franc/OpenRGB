/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "../../Controllers/AMDRadeonReferenceController/RadeonReferenceOutput.h"
#include "../../Controllers/AMDRadeonReferenceController/RadeonReferenceProfile.h"
#include <atomic>
#include <future>
#include <iostream>
#include <stdexcept>
using namespace RadeonReference;
using namespace std::chrono_literals;
void Check(bool condition, const char* message)
{
    if(!condition) throw std::runtime_error(message);
    std::cout << "PASS " << message << std::endl;
}
Frame Direct(uint8_t red, unsigned fps = 7) { return {Static(red,0,0,5),0,2,fps}; }
int main()
{
    try
    {
        std::vector<Message> sent;
        OutputSession session([&](const Message& m,bool echo){sent.push_back(m);return echo || m==Identify();});
        const auto red = Direct(255), green = Frame{Static(0,255,0,5),0,2,7};
        Check(session.Apply(red,0), "Direct initial session succeeds");
        const std::vector<Message> expected{Identify(),Enable(),SelectControl(),Apply(),Mode(false),Apply(),red.message,Apply()};
        Check(sent==expected,"First Direct frame matches hardware-tested initialization and mode-once sequence");
        sent.clear();Check(session.Apply(green,0)&&sent==std::vector<Message>{green.message,Apply()},"Subsequent Direct frame sends only color and Apply");
        sent.clear();Check(session.Apply(green,0)&&sent.empty(),"Unchanged Direct color sends no hardware commands");
        Frame breathing{Breathing(0,255,0,3,2,false),1,5,7};
        Check(session.Apply(breathing,0)&&sent==std::vector<Message>{breathing.message,EffectSelection(false),Apply()},"Native mode uses full hardware selection");
        sent.clear();Check(session.Apply(red,0)&&sent==std::vector<Message>{Mode(false),Apply(),red.message,Apply()},"Returning from native mode reselects Static");
        Frame off{Static(0,0,0,5),0xFE,1,7};sent.clear();
        Check(session.Apply(off,0)&&sent==std::vector<Message>{Mode(true),Apply()},"Off keeps its full command sequence");
        sent.clear();Check(session.Apply(red,0)&&sent.size()==4&&sent[0]==Mode(false),"Returning from Off reselects Static");
        sent.clear();Check(session.Apply(red,1)&&sent==expected,"Resume generation reinitializes even an identical color");
        session.Invalidate();sent.clear();Check(session.Apply(red,1)&&sent==expected,"External ownership or idle gap invalidates mode-once cache");
        for(unsigned failure=0;failure<8;++failure)
        {
            unsigned calls=0;
            OutputSession failing([&](const Message&,bool){return calls++ != failure;});
            Check(!failing.Apply(red,0)&&calls==failure+1,"Transaction failure stops the frame immediately");
            Check(!failing.Apply(green,1)&&calls==failure+1,"Failure remains latched across later frames and resume");
        }
        {
            unsigned calls=0;bool fail=false;
            OutputSession failing([&](const Message&,bool){++calls;return !fail;});
            failing.Apply(red,0);fail=true;calls=0;
            Check(!failing.Apply(green,0)&&calls==1,"Short Direct sequence also stops before Apply on failure");
        }
        {
            std::mutex mutex;std::condition_variable wake;bool release=false;std::vector<uint8_t> frames;
            OutputWorker worker([&](const Frame& frame,uint64_t){
                std::unique_lock<std::mutex> lock(mutex);frames.push_back(frame.message[10]);wake.notify_all();
                if(frames.size()==1)wake.wait(lock,[&]{return release;});
                return SendResult::Applied;
            });
            worker.Submit(Direct(1,10));
            {std::unique_lock<std::mutex> lock(mutex);Check(wake.wait_for(lock,1s,[&]{return frames.size()==1;}),"Output worker starts a frame");}
            auto submit=std::async(std::launch::async,[&]{for(unsigned n=2;n<=100;++n)worker.Submit(Direct(uint8_t(n),10));});
            const bool nonblocking=submit.wait_for(100ms)==std::future_status::ready;
            {std::lock_guard<std::mutex> lock(mutex);release=true;wake.notify_all();}
            submit.get();
            Check(nonblocking,"Submitting frames never waits for an in-flight GPU exchange");
            {std::unique_lock<std::mutex> lock(mutex);Check(wake.wait_for(lock,1s,[&]{return frames.size()>=2;}),"Latest pending frame follows the in-flight frame");Check(frames.size()==2&&frames[1]==100,"Old frames are replaced, not queued");}
            worker.Shutdown();
        }
        {
            std::mutex mutex;std::condition_variable wake;std::vector<std::chrono::steady_clock::time_point> times;std::vector<unsigned> kinds;std::vector<uint64_t> epochs;
            OutputWorker worker([&](const Frame& frame,uint64_t epoch){std::lock_guard<std::mutex> lock(mutex);times.push_back(std::chrono::steady_clock::now());kinds.push_back(frame.kind);epochs.push_back(epoch);wake.notify_all();return SendResult::Applied;});
            worker.Submit(Direct(1,1));
            {std::unique_lock<std::mutex> lock(mutex);Check(wake.wait_for(lock,1s,[&]{return times.size()==1;}),"Slow-rate baseline starts");}
            worker.Submit(Direct(2,1));worker.Submit(off);
            {std::unique_lock<std::mutex> lock(mutex);Check(wake.wait_for(lock,300ms,[&]{return kinds.size()==2;}),"Off bypasses the Direct cadence wait");Check(kinds.back()==1,"Off replaces a queued Direct frame");}
            worker.Pause(true);worker.Submit(red);
            {std::unique_lock<std::mutex> lock(mutex);Check(!wake.wait_for(lock,100ms,[&]{return times.size()>2;}),"Suspend pauses output");}
            worker.Pause(false);
            {std::unique_lock<std::mutex> lock(mutex);Check(wake.wait_for(lock,1s,[&]{return times.size()==3;}),"Resume resubmits the desired scene without a new Ambient frame");Check(epochs.back()==2,"Resume passes a fresh session generation");}
            worker.Shutdown();
        }
        {
            std::mutex mutex;std::condition_variable wake;unsigned controls=0;
            OutputWorker worker([&](const Frame& frame,uint64_t){std::lock_guard<std::mutex> lock(mutex);controls+=frame.kind==1;wake.notify_all();return SendResult::Applied;});
            worker.Pause(true);worker.Submit(off);
            {std::unique_lock<std::mutex> lock(mutex);Check(wake.wait_for(lock,1s,[&]{return controls==1;}),"An explicit Off remains eligible during the suspend transition");}
            worker.Shutdown();
        }
        {
            std::atomic<unsigned> calls{0},finished{0};std::promise<void> first;auto ready=first.get_future();
            OutputWorker worker([&](const Frame&,uint64_t){++calls;first.set_value();return SendResult::Failed;},[&]{++finished;});
            worker.Submit(red);Check(ready.wait_for(1s)==std::future_status::ready,"Failure test reaches the transport callback");
            worker.Submit(green);worker.Pause(false);worker.Shutdown();
            Check(calls==1,"Failed output worker never retries automatically");Check(finished==1,"Worker cleanup runs exactly once before shutdown returns");
        }
        {
            std::mutex mutex;std::condition_variable wake;std::vector<std::chrono::steady_clock::time_point> starts;
            OutputWorker worker([&](const Frame&,uint64_t){std::lock_guard<std::mutex> lock(mutex);starts.push_back(std::chrono::steady_clock::now());wake.notify_all();return SendResult::Applied;});
            worker.Submit(Direct(1,20));
            {std::unique_lock<std::mutex> lock(mutex);wake.wait_for(lock,1s,[&]{return starts.size()==1;});}
            worker.Submit(Direct(2,20));
            {std::unique_lock<std::mutex> lock(mutex);Check(wake.wait_for(lock,1s,[&]{return starts.size()==2;}),"Twenty-per-second target produces the next frame");Check(starts[1]-starts[0]<95ms,"Twenty FPS is not silently clamped to ten FPS");Check(starts[1]-starts[0]>=45ms,"Direct limiter prevents catch-up bursts above twenty starts per second");}
            worker.Submit(Direct(3,1));
            const auto before=std::chrono::steady_clock::now();worker.Shutdown();
            Check(std::chrono::steady_clock::now()-before<300ms,"Shutdown cancels the cadence wait");
        }
        {
            std::mutex mutex;std::condition_variable wake;bool release=false;std::vector<unsigned> kinds;
            OutputWorker worker([&](const Frame& frame,uint64_t){
                std::unique_lock<std::mutex> lock(mutex);kinds.push_back(frame.kind);wake.notify_all();
                if(kinds.size()==1)wake.wait(lock,[&]{return release;});
                return SendResult::Applied;
            });
            worker.Submit(red);
            {std::unique_lock<std::mutex> lock(mutex);wake.wait_for(lock,1s,[&]{return kinds.size()==1;});}
            worker.Submit(off);
            auto close=std::async(std::launch::async,[&]{worker.Shutdown();});
            {std::lock_guard<std::mutex> lock(mutex);release=true;wake.notify_all();}
            close.get();
            Check(kinds==std::vector<unsigned>{2,1},"Shutdown finishes the final queued Off control before releasing ownership");
        }
        {
            mode saved,current;
            saved.name=current.name="Direct";saved.flags=MODE_FLAG_HAS_PER_LED_COLOR|MODE_FLAG_HAS_BRIGHTNESS;
            current.flags=saved.flags|MODE_FLAG_HAS_SPEED;current.speed_min=1;current.speed_max=10;current.speed=7;
            current.brightness_min=1;current.brightness_max=5;saved.brightness=3;
            std::vector<mode> destination{current};
            Check(RestoreModes(destination,{saved},0)==0&&destination[0].speed==7&&destination[0].brightness==3,"Legacy Direct profiles retain brightness and receive recommended default cadence");
        }
        return 0;
    }
    catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
}
