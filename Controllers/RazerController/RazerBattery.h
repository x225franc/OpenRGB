/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

// Core-only optional interface. The RGBController/plugin API layout is unchanged.
struct RazerBatteryStatus
{
    bool supported = false, reachable = false;
    int percent = -1, charging = -1;
    std::string transport;
    std::chrono::steady_clock::time_point updated{};
};
class RazerBatteryCache
{
    mutable std::mutex mutex;
    RazerBatteryStatus status;
public:
    void Enable(const std::string& transport)
    {
        std::lock_guard<std::mutex> lock(mutex);
        status.supported = true;
        status.transport = transport;
    }
    void Publish(RazerBatteryStatus value)
    {
        std::lock_guard<std::mutex> lock(mutex);
        if(value.percent >= 0 && value.percent <= 100 && value.reachable)
        {
            value.updated = std::chrono::steady_clock::now();
            status = std::move(value);
        }
        else
        {
            status.reachable = false;
            status.charging = -1;
            if(!value.transport.empty()) status.transport = value.transport;
        }
    }
    RazerBatteryStatus Snapshot() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return status;
    }
};
class RazerBatteryProvider
{
public:
    virtual ~RazerBatteryProvider() = default;
    virtual RazerBatteryStatus GetBatteryStatus() const = 0;
};
class RazerBatteryMonitor
{
    std::mutex mutex;
    std::condition_variable changed;
    std::thread thread;
    bool stop = false;
public:
    RazerBatteryCache cache;
    ~RazerBatteryMonitor() { Stop(); }
    void Start(std::function<RazerBatteryStatus()> read)
    {
        cache.Enable("USB / HyperSpeed");
        thread = std::thread([this, read]
        {
            std::unique_lock<std::mutex> lock(mutex);
            while(!stop)
            {
                lock.unlock();
                try { cache.Publish(read()); }
                catch(...) { cache.Publish({}); }
                lock.lock();
                changed.wait_for(lock, std::chrono::seconds(30), [this]{ return stop; });
            }
        });
    }
    void Stop()
    {
        { std::lock_guard<std::mutex> lock(mutex); stop = true; }
        changed.notify_all();
        if(thread.joinable()) thread.join();
    }
};
