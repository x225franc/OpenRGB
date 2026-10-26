/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "RadeonReferenceProtocol.h"
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>
#include <thread>

namespace RadeonReference
{
struct Frame
{
    Message message{};
    uint8_t selection = 0;
    unsigned kind = 0;
    unsigned fps = 7;
    bool operator==(const Frame& other) const
    {
        return message == other.message && selection == other.selection && kind == other.kind && fps == other.fps;
    }
};
enum class SendResult { Applied, Deferred, Failed };

// Used only by the serial output worker. Tests inject exchanges without a GPU.
class OutputSession
{
    std::function<bool(const Message&, bool)> exchange;
    bool failed = false, initialized = false;
    std::optional<Frame> previous;
    uint64_t epoch = 0;
    bool Send(const Message& message, bool echo = true)
    {
        if(!exchange(message, echo)) { failed = true; return false; }
        return true;
    }
public:
    explicit OutputSession(std::function<bool(const Message&, bool)> callback) : exchange(std::move(callback)) {}
    void Invalidate() { initialized = false; previous.reset(); }
    bool Apply(const Frame& frame, uint64_t generation)
    {
        if(failed) return false; // A new controller/rescan is required after an error.
        if(generation != epoch) { epoch = generation; Invalidate(); }
        const bool off = frame.selection == 0xFE;
        if(previous && previous->kind == frame.kind && previous->selection == frame.selection
           && (off || previous->message == frame.message)) return true;
        if(!initialized)
        {
            if(!Send(Identify(), false) || !Send(Enable()) || !Send(SelectControl()) || !Send(RadeonReference::Apply())) return false;
            initialized = true;
        }
        const bool direct = frame.kind == 2;
        if(direct)
        {
            // Exact mode-once sequence validated by the external hardware campaign.
            if(!previous || previous->kind != 2)
                if(!Send(Mode(false)) || !Send(RadeonReference::Apply())) return false;
            if(!Send(frame.message) || !Send(RadeonReference::Apply())) return false;
        }
        else
        {
            if(!off && !Send(frame.message)) return false;
            const auto selection = frame.selection == 1 || frame.selection == 2
                ? EffectSelection(frame.selection == 2) : Mode(off);
            if(!Send(selection) || !Send(RadeonReference::Apply())) return false;
        }
        previous = frame;
        return true;
    }
};

// One in-flight transaction and one replaceable desired frame. No FIFO, no busy-wait.
class OutputWorker
{
    using Clock = std::chrono::steady_clock;
    std::mutex mutex;
    std::condition_variable wake;
    bool stopping = false, paused = false, pending = false, failed = false, finish_control = false;
    uint64_t epoch = 0;
    std::optional<Frame> desired;
    std::function<SendResult(const Frame&, uint64_t)> send;
    std::function<void()> cleanup;
    std::thread thread;
    void Run()
    {
        std::unique_lock<std::mutex> lock(mutex);
        Clock::time_point next{};
        while(!stopping || finish_control)
        {
            wake.wait(lock, [this]{ return stopping || (!failed && pending && (!paused || desired->kind == 1)); });
            if(stopping && !finish_control) break;
            finish_control = false;
            const auto generation = epoch;
            const auto rate = desired->fps;
            if(!stopping && desired->kind == 2 && next > Clock::now())
            {
                if(wake.wait_until(lock, next, [this,generation,rate]{
                    return stopping || paused || epoch != generation || desired->kind != 2 || desired->fps != rate;
                })) continue;
            }
            const Frame frame = *desired;
            pending = false;
            const auto started = Clock::now();
            lock.unlock();
            SendResult result = SendResult::Failed;
            try { result = send(frame, generation); } catch(...) {}
            lock.lock();
            if(result == SendResult::Failed) { failed = true; pending = false; finish_control = false; }
            else if(result == SendResult::Deferred)
            {
                pending = desired.has_value();
                // A busy external producer does not create a tight retry loop.
                wake.wait_for(lock, std::chrono::seconds(1), [this]{ return stopping || (paused && desired->kind != 1); });
            }
            else
            {
                next = frame.kind == 2
                    ? started + std::chrono::microseconds(1000000 / std::clamp(frame.fps, 1u, 10u))
                    : Clock::time_point{};
            }
        }
        lock.unlock();
        // In particular, release a Windows mutex on the thread that owns it.
        if(cleanup) cleanup();
    }
public:
    OutputWorker(std::function<SendResult(const Frame&, uint64_t)> callback, std::function<void()> finish = {})
        : send(std::move(callback)), cleanup(std::move(finish)), thread([this]{ Run(); }) {}
    ~OutputWorker() { Shutdown(); }
    void Submit(Frame frame)
    {
        std::lock_guard<std::mutex> lock(mutex);
        if(stopping || failed) return;
        desired = std::move(frame);pending = true;wake.notify_one();
    }
    void Pause(bool value)
    {
        std::lock_guard<std::mutex> lock(mutex);
        paused = value;++epoch;
        if(!value && desired && !failed) pending = true;
        wake.notify_one();
    }
    void Shutdown()
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            // Finish a final already-submitted Static/Off/native control, never old streaming frames.
            if(!stopping) finish_control = pending && desired && desired->kind != 2 && (!paused || desired->kind == 1) && !failed;
            stopping = true;wake.notify_one();
        }
        if(thread.joinable()) thread.join();
    }
};
}
