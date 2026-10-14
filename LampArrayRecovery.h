/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <chrono>
struct LampArrayRecovery
{
    using Clock = std::chrono::steady_clock;
    Clock::time_point unavailable{}, retry{};
    bool invalid = false;
    bool NeedsReopen(bool available, Clock::time_point now)
    {
        if(invalid) return now >= retry;
        if(available) { unavailable = {}; return false; }
        if(unavailable == Clock::time_point{}) unavailable = now;
        return now - unavailable >= std::chrono::seconds(10) && now >= retry;
    }
    void Failed(Clock::time_point now) { invalid = true; retry = now + std::chrono::seconds(2); }
    void Reopened(Clock::time_point now) { invalid = false; unavailable = now; retry = now + std::chrono::seconds(10); }
};
