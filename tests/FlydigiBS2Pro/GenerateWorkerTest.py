# SPDX-License-Identifier: GPL-2.0-or-later
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[2]
s=(root/'Controllers/FlydigiController/RGBController_FlydigiBS2Pro.cpp').read_text()
worker=s[s.index('class FlydigiBS2Worker'):s.index('RGBController_FlydigiBS2Pro::RGBController_FlydigiBS2Pro')]
fixture=r"""
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <iostream>
#include "../Controllers/FlydigiController/FlydigiBS2ProModes.h"
using namespace FlydigiBS2Pro;
struct Scene{Bytes table;bool off=false;int preset=-1;bool operator==(const Scene& v)const{return off==v.off&&preset==v.preset&&table==v.table;}};
struct hid_device{};
struct Fake{std::mutex mutex;std::condition_variable wake;std::vector<Bytes> writes;bool enabled=false,corrupt=false;uint8_t pending=0,prefix=3;int closes=0;bool wait(size_t n){std::unique_lock<std::mutex> lock(mutex);return wake.wait_for(lock,std::chrono::seconds(4),[&]{return writes.size()>=n;});}} fake;
std::atomic<bool> wired{false};bool USBPresent(){return wired;}
int hid_write(hid_device*,const uint8_t* p,size_t n){std::lock_guard<std::mutex> lock(fake.mutex);fake.writes.emplace_back(p,p+n);fake.pending=p[3];fake.prefix=p[0]==2?1:3;if(p[3]==0x46)fake.enabled=p[5]!=0;fake.wake.notify_all();return int(n);}
int hid_read_timeout(hid_device*,uint8_t* p,size_t,int timeout){std::unique_lock<std::mutex> lock(fake.mutex);if(!timeout||!fake.pending){lock.unlock();if(timeout)std::this_thread::sleep_for(std::chrono::milliseconds(timeout));return 0;}uint8_t value=fake.pending==0x45?uint8_t(fake.enabled):1;Bytes reply{fake.prefix,90,165,fake.pending,3,value,uint8_t(fake.pending+3+value)};fake.pending=0;if(fake.corrupt)reply.back()^=1;std::copy(reply.begin(),reply.end(),p);return int(reply.size());}
namespace HidApiLifecycle{void Close(hid_device*){std::lock_guard<std::mutex> lock(fake.mutex);++fake.closes;}}
#define LOG_WARNING(...) ((void)0)
"""
tests=r"""
void check(bool v,const char* text){if(!v)throw std::runtime_error(text);std::cout<<"PASS "<<text<<std::endl;}
Scene scene(){return {NativeTable(SingleStatic,{{255,0,0}},30,5),false};}
int main(){try{
 {FlydigiBS2Worker worker(reinterpret_cast<hid_device*>(1),false);std::this_thread::sleep_for(std::chrono::milliseconds(100));check(fake.writes.empty(),"Discovery sends no RGB command");worker.Submit(scene());check(fake.wait(23),"Native mode sends query, acknowledged table and RGB enable");std::this_thread::sleep_for(std::chrono::milliseconds(100));size_t n;{std::lock_guard<std::mutex> lock(fake.mutex);n=fake.writes.size();for(const auto& p:fake.writes)check(p[3]==0x41||p[3]==0x43||p[3]==0x45||p[3]==0x46||p[3]==0x47,"Only RGB whitelist opcodes reach HID");}worker.Submit(scene());std::this_thread::sleep_for(std::chrono::milliseconds(150));{std::lock_guard<std::mutex> lock(fake.mutex);check(fake.writes.size()==n,"Identical native scene and idle time do not resend table");}for(unsigned i=0;i<100;++i){auto value=scene();value.table[5]=uint8_t(i%100);worker.Submit(value);}auto final=scene();final.table[5]=31;worker.Submit(final);std::this_thread::sleep_for(std::chrono::milliseconds(150));{std::lock_guard<std::mutex> lock(fake.mutex);check(fake.writes.size()==n,"Continuous editing does not persist intermediate slider values");}check(fake.wait(n+22),"Final edited native scene is uploaded once editing settles");std::this_thread::sleep_for(std::chrono::milliseconds(100));{std::lock_guard<std::mutex> lock(fake.mutex);unsigned commits=0;for(const auto& p:fake.writes)commits+=p[3]==0x43;check(commits==2,"One hundred edits generate only one additional persistent commit");n=fake.writes.size();}worker.Submit({{},true});check(fake.wait(n+2),"Off sends only status query and RGB switch");}
 {size_t n;{std::lock_guard<std::mutex> lock(fake.mutex);n=fake.writes.size();fake.corrupt=true;}FlydigiBS2Worker worker(reinterpret_cast<hid_device*>(1),false);worker.Submit(scene());check(fake.wait(n+1),"Invalid ACK test reaches status request");std::this_thread::sleep_for(std::chrono::milliseconds(1200));{std::lock_guard<std::mutex> lock(fake.mutex);check(fake.writes.size()==n+1,"Corrupt ACK stops before prepare and is not retried automatically");fake.corrupt=false;}}
 {size_t n;{std::lock_guard<std::mutex> lock(fake.mutex);n=fake.writes.size();}wired=true;FlydigiBS2Worker worker(reinterpret_cast<hid_device*>(1),true);worker.Submit(scene());std::this_thread::sleep_for(std::chrono::milliseconds(150));{std::lock_guard<std::mutex> lock(fake.mutex);check(fake.writes.size()==n,"Bluetooth sends nothing while USB is present");}wired=false;check(fake.wait(n+23),"Bluetooth applies pending native scene after USB data removal");}
 {
 size_t start;{std::lock_guard<std::mutex> lock(fake.mutex);start=fake.writes.size();fake.enabled=true;}
 FlydigiBS2Worker worker(reinterpret_cast<hid_device*>(1),false);Scene value;value.preset=1;worker.Submit(value);check(fake.wait(start+3),"Preset Sync initializes RGB preset mode with no table upload");std::this_thread::sleep_for(std::chrono::milliseconds(120));
 worker.Submit(value);std::this_thread::sleep_for(std::chrono::milliseconds(150));{std::lock_guard<std::mutex> lock(fake.mutex);check(fake.writes.size()==start+3,"Repeated Ambient frames in one hue band send nothing");}
 size_t expected=start+3;
 for(int preset:{2,5,4}){value.preset=preset;worker.Submit(value);expected+=2;check(fake.wait(expected),"Ambient hue change reaches a volatile firmware preset");std::this_thread::sleep_for(std::chrono::milliseconds(120));}
 {std::lock_guard<std::mutex> lock(fake.mutex);for(size_t i=start;i<fake.writes.size();++i)check(fake.writes[i][3]==0x44||fake.writes[i][3]==0x45,"Approximate Ambient never writes flash or native RGB tables while RGB is enabled");}
 value.preset=0;worker.Submit(value);std::this_thread::sleep_for(std::chrono::milliseconds(150));{std::lock_guard<std::mutex> lock(fake.mutex);check(fake.writes.size()==expected,"Dark HOLD does not send firmware preset zero or persistent Off");}
 }
 check(fake.closes==4,"Every owned HID handle closes after its worker stops");return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}}
"""
Path(sys.argv[1]).write_text(fixture+worker+tests)
