/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

inline constexpr auto RazerBatteryPollInterval = std::chrono::minutes(5);

// Core-only optional interface. The RGBController/plugin API layout is unchanged.
struct RazerBatteryStatus
{
    bool supported = false, reachable = false, refreshing = false;
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
            const bool refreshing = status.refreshing;
            status = std::move(value);
            status.refreshing = refreshing;
        }
        else
        {
            status.reachable = false;
            status.charging = -1;
            if(!value.transport.empty()) status.transport = value.transport;
        }
    }
    void SetRefreshing(bool value)
    {
        std::lock_guard<std::mutex> lock(mutex);
        status.refreshing = value;
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
    virtual bool RequestBatteryRefresh() = 0;
};
class RazerBatteryMonitor
{
    std::mutex mutex;
    std::condition_variable changed;
    std::thread thread;
    bool stop = false, requested = false, reading = false;
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
                reading = true;requested = false;cache.SetRefreshing(true);
                lock.unlock();
                try { cache.Publish(read()); }
                catch(...) { cache.Publish({}); }
                lock.lock();
                reading = false;cache.SetRefreshing(false);
                changed.wait_for(lock, RazerBatteryPollInterval, [this]{ return stop || requested; });
            }
        });
    }
    bool RequestRefresh()
    {
        std::lock_guard<std::mutex> lock(mutex);
        if(stop || reading || requested) return false;
        requested = true;cache.SetRefreshing(true);changed.notify_all();return true;
    }
    void Stop()
    {
        { std::lock_guard<std::mutex> lock(mutex); stop = true; }
        changed.notify_all();
        if(thread.joinable()) thread.join();
    }
};
