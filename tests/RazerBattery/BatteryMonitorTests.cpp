#include "../../Controllers/RazerController/RazerBattery.h"
#include <atomic>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace std::chrono_literals;
void check(bool b,const char* text){if(!b)throw std::runtime_error(text);std::cout<<"PASS "<<text<<std::endl;}
template<class F> void await(F f){auto end=std::chrono::steady_clock::now()+2s;while(!f()){if(std::chrono::steady_clock::now()>end)throw std::runtime_error("Test wait timed out");std::this_thread::sleep_for(2ms);}}
int main(){try{
 static_assert(RazerBatteryPollInterval==5min,"Automatic polling must be five minutes");
 check(RazerBatteryPollInterval==5min,"automatic interval is five minutes");
 std::mutex mutex;std::condition_variable changed;int started=0,allow=0;std::atomic<bool> uiThreadUsed{false};auto ui=std::this_thread::get_id();
 RazerBatteryMonitor monitor;
 monitor.Start([&]{std::unique_lock<std::mutex> l(mutex);const int n=++started;changed.notify_all();changed.wait(l,[&]{return allow>=n;});if(std::this_thread::get_id()==ui)uiThreadUsed=true;RazerBatteryStatus s;s.supported=true;s.reachable=n<3;s.percent=n<3?90-n:-1;s.transport="HyperSpeed";return s;});
 auto waitStarted=[&](int n){std::unique_lock<std::mutex> l(mutex);check(changed.wait_for(l,2s,[&]{return started>=n;}),"background read starts");};
 auto release=[&](int n){std::lock_guard<std::mutex> l(mutex);allow=n;changed.notify_all();};
 waitStarted(1);check(monitor.cache.Snapshot().refreshing,"initial read reports busy");check(!monitor.RequestRefresh(),"manual requests during active read are coalesced");release(1);await([&]{return !monitor.cache.Snapshot().refreshing;});
 check(monitor.cache.Snapshot().percent==89,"initial result is cached");
 std::atomic<int> accepted{0};std::vector<std::thread> callers;for(int i=0;i<8;i++)callers.emplace_back([&]{if(monitor.RequestRefresh())++accepted;});for(auto& t:callers)t.join();waitStarted(2);check(accepted==1,"eight concurrent clicks schedule one read");release(2);await([&]{return !monitor.cache.Snapshot().refreshing;});
 auto previous=monitor.cache.Snapshot();check(previous.percent==88&&!uiThreadUsed,"manual read publishes new value outside caller thread");
 check(monitor.RequestRefresh(),"fresh manual request accepted");waitStarted(3);release(3);await([&]{return !monitor.cache.Snapshot().refreshing;});auto stale=monitor.cache.Snapshot();check(stale.percent==88&&stale.updated==previous.updated&&!stale.reachable,"sleeping device retains last valid value and timestamp");
 std::this_thread::sleep_for(100ms);{std::lock_guard<std::mutex> l(mutex);check(started==3,"idle worker performs no extra reads");}
 auto time=std::chrono::steady_clock::now();monitor.Stop();check(std::chrono::steady_clock::now()-time<1s,"shutdown interrupts five-minute wait");return 0;
 }catch(std::exception const& e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}}
