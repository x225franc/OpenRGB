#include "HidApiLifecycle.h"
#include <atomic>
#include <thread>
#include <vector>
#include <iostream>
int main(int argc, char**)
{
    const bool raw = argc > 1;
    HidApiLifecycle::Init();
    std::atomic<bool> start{false}, done{false};
    std::vector<std::thread> workers;
    for(int i=0;i<4;++i) workers.emplace_back([&]{
        while(!start.load()) std::this_thread::yield();
        for(int n=0;n<150;++n) {
            auto* devices = raw ? hid_enumerate(0xffff,0xffff) : HidApiLifecycle::Enumerate(0xffff,0xffff);
            hid_free_enumeration(devices);
        }
    });
    std::thread initializer([&]{while(!start.load()) std::this_thread::yield(); while(!done.load()) {if(raw) hid_init();else HidApiLifecycle::Init();std::this_thread::yield();}});
    start=true;
    for(auto& worker:workers) worker.join();
    done=true;initializer.join();HidApiLifecycle::Exit();
    std::cout << "PASS: 600 missing-device enumerations concurrent with initialization, no hardware writes." << std::endl;
}
