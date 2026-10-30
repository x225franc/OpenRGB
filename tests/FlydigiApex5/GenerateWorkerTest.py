# SPDX-License-Identifier: GPL-2.0-or-later
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[2]
s=(root/'Controllers/FlydigiController/RGBController_FlydigiApex5.cpp').read_text()
worker=s[s.index('class FlydigiApex5Worker'):s.index('RGBController_FlydigiApex5::RGBController_FlydigiApex5')]
fixture=r"""
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <string>
#include <map>
#include <iostream>
#include <stdexcept>
#include "../Controllers/FlydigiController/FlydigiApex5Protocol.h"
using namespace FlydigiApex5;
struct Scene { Bytes data;bool direct=false;bool operator==(const Scene& v)const{return direct==v.direct&&data==v.data;} };
struct hid_device{};
struct Fake {
 std::mutex mutex;std::condition_variable changed;bool fail=false;std::atomic<bool> wired{false};
 bool query=false;uint8_t profile=2;int opens=1,closes=0;std::vector<Report> writes;
 bool wait(size_t count){std::unique_lock<std::mutex> l(mutex);return changed.wait_for(l,std::chrono::seconds(5),[&]{return writes.size()>=count;});}
} fake;
int hid_write(hid_device*,const uint8_t* bytes,size_t size){std::lock_guard<std::mutex> l(fake.mutex);if(fake.fail)return -1;if(size!=32)throw std::runtime_error("wrong size");Report b;std::copy_n(bytes,32,b.begin());if(b[3]==0xA1){fake.query=true;return 32;}fake.writes.push_back(b);fake.changed.notify_all();return 32;}
int hid_read_timeout(hid_device*,uint8_t* bytes,size_t,int timeout){std::lock_guard<std::mutex> l(fake.mutex);if(!timeout||!fake.query)return 0;fake.query=false;std::fill_n(bytes,32,uint8_t(0));bytes[0]=4;bytes[1]=0x5A;bytes[2]=0xA5;bytes[3]=0xA1;bytes[6]=fake.profile;return 32;}
namespace HidApiLifecycle { hid_device* OpenPath(const char*){std::lock_guard<std::mutex> l(fake.mutex);++fake.opens;return reinterpret_cast<hid_device*>(1);} void Close(hid_device*){std::lock_guard<std::mutex> l(fake.mutex);++fake.closes;} }
namespace FlydigiApex5 {bool WiredPresent(){return fake.wired;}}
#define LOG_WARNING(...) ((void)0)
#define LOG_DEBUG(...) ((void)0)
"""
tests=r"""
void check(bool value,const char* text){if(!value)throw std::runtime_error(text);std::cout<<"PASS "<<text<<std::endl;}
Scene scene(uint8_t r,uint8_t g,uint8_t b){Scene s;s.direct=true;s.data=Metadata(5,30,1,0);for(int i=0;i<12;++i)s.data.insert(s.data.end(),{r,g,b});return s;}
int main(){try{
 {
 FlydigiApex5Worker worker(reinterpret_cast<hid_device*>(1),"fake");
 std::this_thread::sleep_for(std::chrono::milliseconds(150));check(fake.writes.empty(),"Discovery does not claim lighting before an explicit scene");
 worker.Submit(scene(255,0,0));check(fake.wait(5),"Initial Direct scene sends metadata and two RGB pages");
 {std::lock_guard<std::mutex> l(fake.mutex);check(fake.writes[0][5]==2,"Initial RGB targets verified active profile 3, not profile 1");}
 std::this_thread::sleep_for(std::chrono::milliseconds(100));
 size_t before;{std::lock_guard<std::mutex> l(fake.mutex);before=fake.writes.size();}
 worker.Submit(scene(255,0,0));std::this_thread::sleep_for(std::chrono::milliseconds(150));
 {std::lock_guard<std::mutex> l(fake.mutex);check(fake.writes.size()==before,"Identical scene is not resent immediately");}
 for(int i=0;i<100;++i)worker.Submit(scene(i,0,0));worker.Submit(scene(0,0,255));
 check(fake.wait(before+3),"Latest burst scene reaches HID worker");std::this_thread::sleep_for(std::chrono::milliseconds(350));
 {std::lock_guard<std::mutex> l(fake.mutex);auto b=fake.writes[fake.writes.size()-2];check(b[3]==0xA9&&b[5]==0&&b[6]==0&&b[7]==0&&b[8]==255,"Burst preserves latest blue scene, not queued stale frames");check(fake.writes.size()<before+20,"One hundred UI submissions do not create a one hundred-frame backlog");before=fake.writes.size();}
 std::this_thread::sleep_for(std::chrono::milliseconds(2200));
 {std::lock_guard<std::mutex> l(fake.mutex);check(fake.writes.size()>=before+5,"Owned stationary Direct scene refreshes metadata and colors without new submissions");before=fake.writes.size();for(const auto& b:fake.writes)check(b[3]==0xA8||b[3]==0xA9,"Only lighting opcodes reach HID");fake.fail=true;}
 worker.Submit(scene(0,255,0));std::this_thread::sleep_for(std::chrono::milliseconds(200));
 worker.Submit(scene(80,0,80));{std::lock_guard<std::mutex> l(fake.mutex);fake.fail=false;}
 check(fake.wait(before+5),"Failed transfer retries the latest scene with full metadata");
 }
 check(fake.opens==fake.closes,"Every owned handle closes exactly once");
 {
 size_t before;{std::lock_guard<std::mutex> l(fake.mutex);before=fake.writes.size();fake.profile=2;++fake.opens;}
 FlydigiApex5Worker worker(reinterpret_cast<hid_device*>(1),"sleep-recovery");worker.Submit(scene(20,30,40));check(fake.wait(before+5),"Sleep test owns an initial Direct scene");
 {std::lock_guard<std::mutex> l(fake.mutex);before=fake.writes.size();fake.profile=255;}
 std::this_thread::sleep_for(std::chrono::milliseconds(1500));
 {std::lock_guard<std::mutex> l(fake.mutex);check(fake.writes.size()==before,"Sleeping pad receives no RGB when active profile cannot be verified");fake.profile=1;}
 check(fake.wait(before+5),"Wake restores stationary Direct without a new Submit");
 {std::lock_guard<std::mutex> l(fake.mutex);check(fake.writes[before][5]==1,"Wake restores RGB on the newly verified onboard profile");}
 }
 {
 size_t before;{std::lock_guard<std::mutex> l(fake.mutex);before=fake.writes.size();++fake.opens;}
 FlydigiApex5Worker worker(reinterpret_cast<hid_device*>(1),"native-idle");auto native=scene(1,2,3);native.direct=false;worker.Submit(native);check(fake.wait(before+4),"Native scene is applied once");
 {std::lock_guard<std::mutex> l(fake.mutex);before=fake.writes.size();}
 std::this_thread::sleep_for(std::chrono::milliseconds(2300));
 {std::lock_guard<std::mutex> l(fake.mutex);check(fake.writes.size()==before,"Native effects remain autonomous without periodic writes");}
 }

 {
 size_t before;{std::lock_guard<std::mutex> l(fake.mutex);before=fake.writes.size();fake.profile=255;++fake.opens;}
 FlydigiApex5Worker worker(reinterpret_cast<hid_device*>(1),"profile-test");worker.Submit(scene(1,2,3));
 std::this_thread::sleep_for(std::chrono::milliseconds(300));
 {std::lock_guard<std::mutex> l(fake.mutex);check(fake.writes.size()==before,"Unknown profile suspends every RGB write");fake.profile=1;}
 check(fake.wait(before+5),"Verified profile resumes latest requested scene");
 {std::lock_guard<std::mutex> l(fake.mutex);check(fake.writes[before][5]==1,"Resume targets profile 2 without falling back to profile 1");before=fake.writes.size();fake.profile=3;}
 worker.Submit(scene(4,5,6));check(fake.wait(before+5),"Profile change invalidates cached RGB metadata");
 {std::lock_guard<std::mutex> l(fake.mutex);check(fake.writes[before][5]==3,"Changed scene targets newly verified profile 4");}
 }

 {
 fake.wired=true;size_t before;{std::lock_guard<std::mutex> l(fake.mutex);before=fake.writes.size();++fake.opens;}
 FlydigiApex5Worker worker(reinterpret_cast<hid_device*>(1),"receiver",true);worker.Submit(scene(255,0,0));
 std::this_thread::sleep_for(std::chrono::milliseconds(250));
 {std::lock_guard<std::mutex> l(fake.mutex);check(fake.writes.size()==before,"Receiver sends no lighting while USB is connected");}
 worker.Submit(scene(0,0,255));fake.wired=false;check(fake.wait(before+5),"USB removal resumes latest receiver scene with metadata");
 }
 check(fake.opens==fake.closes,"Transport-priority worker closes its handle");
 {
 size_t before;{std::lock_guard<std::mutex> l(fake.mutex);before=fake.writes.size();++fake.opens;}
 {FlydigiApex5Worker worker(reinterpret_cast<hid_device*>(1),"hotplug");worker.Submit(scene(9,8,7));check(fake.wait(before+5),"Hotplug test owns a Direct scene");}
 {std::lock_guard<std::mutex> l(fake.mutex);before=fake.writes.size();++fake.opens;}
 {FlydigiApex5Worker worker(reinterpret_cast<hid_device*>(1),"hotplug");std::this_thread::sleep_for(std::chrono::milliseconds(200));{std::lock_guard<std::mutex> l(fake.mutex);check(fake.writes.size()==before,"Recreated controller does not upload RGB during initial reconnect settling");}check(fake.wait(before+5),"Recreated controller restores its session Direct scene without Submit");
 auto native=scene(1,1,1);native.direct=false;worker.Submit(native);check(fake.wait(before+9),"Explicit native mode clears Direct resume ownership");}
 {std::lock_guard<std::mutex> l(fake.mutex);before=fake.writes.size();++fake.opens;}
 {FlydigiApex5Worker worker(reinterpret_cast<hid_device*>(1),"hotplug");std::this_thread::sleep_for(std::chrono::milliseconds(200));std::lock_guard<std::mutex> l(fake.mutex);check(fake.writes.size()==before,"Native selection does not resurrect an old Direct scene after reconnect");}
 }

 return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}}
"""
Path(sys.argv[1]).write_text(fixture+worker+tests)
