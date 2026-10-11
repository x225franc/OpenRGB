/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <array>
#include <algorithm>
#include <chrono>
#include <QDebug>

// Opt-in bounded diagnostics, no file IO or logging on the hot path.
// One instance per worker; no cross-thread access.
class AmbientPerformanceMetrics
{
public:
    using Clock = std::chrono::steady_clock;
    explicit AmbientPerformanceMetrics(const char* stage) : stage(stage), enabled(qEnvironmentVariableIsSet("OPENRGB_AMBIENT_METRICS")) {}
    void Add(Clock::time_point start)
    {
        if(!enabled) return;
        const double ms = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
        samples[count % samples.size()] = ms;
        ++count; total += ms; maximum = (std::max)(maximum, ms);
    }
    void Report()
    {
        if(!enabled || !count) return;
        const size_t n = (std::min)(count, samples.size());
        std::sort(samples.begin(), samples.begin() + n);
        qInfo() << "[Ambient metrics]" << stage << "samples" << count << "mean_ms" << total/count
                << "p95_last2048_ms" << samples[(n-1)*95/100] << "max_ms" << maximum;
    }
private:
    const char* stage;
    bool enabled;
    std::array<double, 2048> samples{};
    size_t count = 0;
    double total = 0, maximum = 0;
};
