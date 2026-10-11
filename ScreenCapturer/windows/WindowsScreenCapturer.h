/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "ScreenCapturer.h"
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

class WindowsScreenCapturer : public ScreenCapturer
{
public:
    WindowsScreenCapturer();
    ~WindowsScreenCapturer() override;
    void Start() override;
    void Stop() override;
    void SetScreen(int) override;
    void SetFrameRate(unsigned int) override;
    void SetAnalysisWidth(unsigned int) override;
    void SetModernCapture(bool) override;
    QSize SourceSize() const override;
private:
    struct CaptureResources;
    void CaptureThreadFunction();
    std::thread capture_thread;
    std::mutex lifecycle_mutex;
    std::atomic<bool> running{false};
    mutable std::mutex settings_mutex;
    std::condition_variable wake;
    QString display_name;
    unsigned int analysis_width = 320;
    bool modern_capture = true;
    unsigned int generation = 0;
    QSize source_size;
};
