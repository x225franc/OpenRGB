# SPDX-License-Identifier: GPL-2.0-or-later
"""Compile the actual BluetoothWorker against a deterministic fake transport."""
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[2]
source_path = Path(sys.argv[2]) if len(sys.argv) > 2 else root / 'Controllers/RazerBluetoothController/RGBController_RazerBluetooth_Windows.cpp'
source = source_path.read_text(encoding='utf-8-sig')
worker = source[source.index('class BluetoothWorker'):source.index('class RGBController_RazerBluetooth')]
fixture = r'''
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
using namespace std::chrono_literals;
namespace winrt { using hstring=std::string; struct hresult_error {std::string message()const{return {};}}; inline std::string to_string(std::string s){return s;} }
#define LOG_DEBUG(...) ((void)0)
#define LOG_INFO(...) ((void)0)
#define LOG_WARNING(...) ((void)0)
#include "../Controllers/RazerController/RazerBattery.h"
bool WiredPresent(bool,bool=false){return false;}
namespace RazerBLE {
using Bytes=std::vector<uint8_t>;
struct Apartment {};
struct Fake {
 bool v4=false;
 std::atomic<int> batteryReads{0};std::atomic<bool> online{true};std::atomic<uint64_t> generation{0};
 std::mutex mutex;std::condition_variable changed;int closes=0,reads=0;std::vector<Bytes> writes;std::vector<Bytes> actual{{0},{0}};
 void reset(){std::lock_guard<std::mutex> l(mutex);writes.clear();v4=false;actual={{0},{0}};closes=0;reads=0;batteryReads=0;online=true;generation=0;}
 bool waitWrites(size_t n){std::unique_lock<std::mutex> l(mutex);return changed.wait_for(l,8s,[&]{return writes.size()>=n;});}
 bool waitClose(){std::unique_lock<std::mutex> l(mutex);return changed.wait_for(l,3s,[&]{return closes>0;});}
} fake;
struct Transport {
 bool keyboard=false,v4=false;std::vector<uint8_t> zones{1,10};std::vector<Bytes> initial{{0},{0}};
 void Open(std::string){if(!fake.online)throw std::runtime_error("offline");v4=fake.v4;if(v4){zones={4,1,10};initial={{0},{0},{99}};}}
 size_t index(uint8_t zone){return v4 ? (zone==4?0:zone==1?1:2) : (zone==1?0:1);}
 void Close(){std::lock_guard<std::mutex> l(fake.mutex);++fake.closes;fake.changed.notify_all();}
 bool IsConnected(){return fake.online;}
 uint64_t ConnectionGeneration(){return fake.generation;}
 int ReadBattery(){++fake.batteryReads;fake.changed.notify_all();return -1;}
 bool MatchesLighting(uint8_t zone,Bytes const& b){if(!fake.online)throw std::runtime_error("offline");std::lock_guard<std::mutex> l(fake.mutex);++fake.reads;fake.changed.notify_all();return fake.actual[index(zone)]==b;}
 void Set(uint8_t zone,Bytes const& b){if(!fake.online)throw std::runtime_error("offline");std::lock_guard<std::mutex> l(fake.mutex);fake.writes.push_back(b);fake.actual[index(zone)]=b;fake.changed.notify_all();}
};
}
'''
tests = r'''
void check(bool ok,char const* label){if(!ok)throw std::runtime_error(label);std::cout<<"PASS "<<label<<std::endl;}
int main(){try {
 auto& f=RazerBLE::fake;
 {
 BluetoothWorker worker;check(worker.Start("fake"),"worker starts");worker.Submit({{7},{8}});
 check(f.waitWrites(2),"initial state sent");
 check(worker.RequestBatteryRefresh(),"Bluetooth manual battery request accepted");
 {std::unique_lock<std::mutex> l(f.mutex);check(f.changed.wait_for(l,3s,[&]{return f.batteryReads>=2;}),"Bluetooth worker reads battery on explicit request");}
 f.generation+=2;
 check(f.waitWrites(4),"brief reconnect restores unchanged state without new frame");
 f.generation+=2;worker.Submit({{7},{8}});
 check(f.waitWrites(6),"identical pending frame still restores after reconnect");
 {std::unique_lock<std::mutex> l(f.mutex);const auto before=f.reads;check(f.changed.wait_for(l,8s,[&]{return f.reads>=before+2;}),"mouse state read back at low frequency");check(f.writes.size()==6,"matching hardware state does not resend colors");f.actual={{0},{0}};}
 check(f.waitWrites(8),"silent mouse lighting reset restored without disconnect event");
 f.online=false;f.generation++;worker.Submit({{9},{9}});
 check(f.waitClose(),"offline connection closes for retry");
 worker.Submit({{42},{43}});f.online=true;f.generation++;
 check(f.waitWrites(10),"retry restores latest requested scene");
 {std::lock_guard<std::mutex> l(f.mutex);check(f.writes[f.writes.size()-2]==RazerBLE::Bytes{42}&&f.writes.back()==RazerBLE::Bytes{43},"no stale scene overwrites newest request");}
 }
 f.reset();
 {BluetoothWorker worker;check(worker.Start("fake"),"unowned worker starts");f.generation+=2;std::unique_lock<std::mutex> l(f.mutex);check(!f.changed.wait_for(l,2300ms,[&]{return !f.writes.empty();}),"reconnect never claims unowned lights");}
 f.reset();
 {
 f.v4=true;f.actual={{0},{0},{99}};
 BluetoothWorker worker;check(worker.Start("v4"),"V4 three-group worker starts");
 check(worker.v4&&worker.zones==std::vector<uint8_t>({4,1,10}),"V4 preserves logo wheel and underglow group identities");
 worker.Submit({{10},{20},{0}});check(f.waitWrites(3),"V4 initial scene includes an explicitly off underglow");
 f.generation+=2;check(f.waitWrites(6),"V4 reconnect restores every group without a new frame");
 {std::lock_guard<std::mutex> l(f.mutex);check(f.actual==std::vector<RazerBLE::Bytes>({{10},{20},{0}}),"V4 reconnect preserves independent colors and disabled group");f.actual={{0},{0},{99}};}
 check(f.waitWrites(9),"V4 silent reset restores all three groups");
 {std::lock_guard<std::mutex> l(f.mutex);check(f.actual==std::vector<RazerBLE::Bytes>({{10},{20},{0}}),"V4 silent reset cannot leave underglow illuminated");}
 }
 f.reset();
 auto worker=std::make_unique<BluetoothWorker>();check(worker->Start("fake"),"shutdown worker starts");worker->Submit({{5},{5}});check(f.waitWrites(2),"shutdown baseline sent");f.online=false;f.generation++;worker->Submit({{6},{6}});check(f.waitClose(),"retry pending before shutdown");auto start=std::chrono::steady_clock::now();worker.reset();check(std::chrono::steady_clock::now()-start<1s,"shutdown interrupts retry backoff");
 return 0;
 }catch(std::exception const& e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}}
'''
out = Path(sys.argv[1]);out.parent.mkdir(parents=True,exist_ok=True)
out.write_text(fixture + worker + tests,encoding='utf-8')
