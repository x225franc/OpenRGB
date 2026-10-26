/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "../../RGBController/SoftwareLightingEffect.h"
#include "../../Controllers/RazerBluetoothController/RazerBluetoothLighting.h"
#include <iostream>
#include <stdexcept>
using namespace std::chrono_literals;
void check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);std::cout<<"PASS "<<text<<std::endl;}
int main(){try{
 using namespace RazerBLELighting;const auto green=ToRGBColor(0,255,0);
 check(KeyboardState(Breathing,green,10)==std::array<uint8_t,11>{2,0,0,1,0,255,0,0,0,0,10},"Breathing matches captured native keyboard state");
 check(KeyboardState(Reactive,green,10)==std::array<uint8_t,11>{5,0,3,1,0,255,0,0,0,0,10},"Reactive matches captured native keyboard state");
 check(KeyboardState(Starlight,green,10)==std::array<uint8_t,11>{7,0,2,1,0,255,0,0,0,0,10},"Starlight matches captured native keyboard state");
 check(KeyboardState(Spectrum,green,10)==std::array<uint8_t,11>{3,0,0,0,0,0,0,0,0,0,10},"Spectrum matches native keyboard readback");
 check(KeyboardState(Wave,green,10)==std::array<uint8_t,11>{4,1,40,1,0,255,0,0,0,0,10},"Previously validated Wave packet retained");
 check(KeyboardState(Off,green,255)[10]==0&&ModeFromState(7,0)==Off,"Off remains zero brightness");
 check(ModeFromState(2,10)==Breathing&&ModeFromState(3,10)==Spectrum&&ModeFromState(5,10)==Reactive&&ModeFromState(7,10)==Starlight,"Hardware effect readback selects the correct UI mode");
 check(std::abs(SoftwareLighting::BreathingLevel(0,4)-1)<1e-9&&std::abs(SoftwareLighting::BreathingLevel(2,4))<1e-9,"Temporal breathing reaches both endpoints");
 check(SoftwareLighting::Scale(0,0.5)==0,"Black stays off during software effects");
 check(SoftwareLighting::CycleColor(0,6)==ToRGBColor(255,0,0)&&SoftwareLighting::CycleColor(2,6)==ToRGBColor(0,255,0),"Color cycle follows a stable time-based hue wheel");
 std::mutex mutex;std::condition_variable changed;size_t frames=0;bool blackRetained=true;std::vector<std::chrono::steady_clock::time_point> times;
 SoftwareLighting::Worker worker([&](std::vector<RGBColor>& frame){std::lock_guard<std::mutex> lock(mutex);++frames;blackRetained&=frame[1]==0;times.push_back(std::chrono::steady_clock::now());changed.notify_all();});
 worker.Set({green,0},8);
 {std::unique_lock<std::mutex> lock(mutex);check(changed.wait_for(lock,2s,[&]{return frames>=3;}),"Software breathing sends frames from background worker");}
 worker.Stop();
 {std::unique_lock<std::mutex> lock(mutex);const auto count=frames;check(!changed.wait_for(lock,180ms,[&]{return frames!=count;}),"Stopped animation cannot overwrite a subsequent native mode");check(blackRetained,"Unselected black zone stays black on every frame");}
 worker.Shutdown();
 size_t slowFrames=0;std::chrono::steady_clock::time_point first,last;
 SoftwareLighting::Worker slow([&](std::vector<RGBColor>&){std::lock_guard<std::mutex> lock(mutex);if(!slowFrames)first=std::chrono::steady_clock::now();last=std::chrono::steady_clock::now();++slowFrames;changed.notify_all();},250ms);
 slow.Set({green},3,true);
 {std::unique_lock<std::mutex> lock(mutex);check(changed.wait_for(lock,2s,[&]{return slowFrames>=2;}),"GPU software cadence produces updates");check(last-first>=240ms,"GPU cadence never exceeds four frame starts per second");}
 slow.Shutdown();return 0;
}catch(std::exception const& e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}}
