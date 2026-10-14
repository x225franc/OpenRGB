/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <chrono>
// Same bounded ownership recovery as LightSync standalone and Toolkit.
struct LampArrayRecovery
{
    using Clock = std::chrono::steady_clock;
    Clock::time_point until{}, unavailable{}, retry{};
    bool invalid = false, wasUnavailable = false;
    void Begin(Clock::time_point now) { until=now+std::chrono::minutes(3); unavailable=now; wasUnavailable=true; }
    bool NeedsReopen(bool available, Clock::time_point now)
    {
        if(invalid) return now>=retry;
        if(available) { wasUnavailable=false; return false; }
        if(!wasUnavailable) { unavailable=now; wasUnavailable=true; }
        if(now>=until || now-unavailable<std::chrono::seconds(45)) return false;
        unavailable=now;
        return true;
    }
    void Failed(Clock::time_point now) { invalid=true; retry=now+std::chrono::seconds(2); }
    void Reopened(Clock::time_point now) { invalid=false; unavailable=now; wasUnavailable=true; }
};
