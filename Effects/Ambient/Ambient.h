/*---------------------------------------------------------*\
| Ambient.h                                                 |
|                                                           |
|   OpenRGB Effects Plugin Ambient Effect                   |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <mutex>
#include <atomic>
#include <array>
#include <chrono>
#include <map>
#include "PerformanceMetrics.h"
#include <QWidget>
#include <QTimer>
#include <QMouseEvent>
#include "ui_Ambient.h"
#include "RGBEffect.h"
#include "EffectRegisterer.h"
#include "RectangleSelector.h"
#include "ScreenCapturer.h"

namespace Ui {
class Ambient;
}

enum AmbientMode{
    SCALED_AVERAGE = 0,
    SCREEN_COPY = 1
};

class Ambient : public RGBEffect
{
    Q_OBJECT

public:
    explicit Ambient(QWidget *parent = nullptr);
    ~Ambient();

    EFFECT_REGISTERER(ClassName(), UI_Name(), CAT_SPECIAL, [](){return new Ambient;});

    static std::string const ClassName() {return "Ambient";}
    static std::string const UI_Name() { return QT_TR_NOOP("Ambient"); }

    void StepEffect(std::vector<ControllerZone*>) override;
    void LoadCustomSettings(json) override;
    json SaveCustomSettings() override;
    void EffectState(bool) override;
    unsigned int GetFPS() override;
    void SetFPS(unsigned int value) override { effect_fps = std::max(1u, value); }
    void SetBrightness(unsigned int value) override { ambient_brightness = value; }
    unsigned int GetBrightness() override { return ambient_brightness; }
    void SetTemperature(int value) override { ambient_temperature = value; }
    int GetTemperature() override { return ambient_temperature; }
    void SetTint(int value) override { ambient_tint = value; }
    int GetTint() override { return ambient_tint; }
    void OnControllerZonesListChanged(std::vector<ControllerZone*>) override;
    bool NeedsControllerUpdate(RGBControllerInterface*);
    bool PreparePreview();
    void StepPreview(std::vector<ControllerZone*>);

private slots:
    void changeEvent(QEvent *event) override;
    void on_select_screen_clicked();
    void on_select_rectangle_clicked();
    void on_left_valueChanged(int);
    void on_top_valueChanged(int);
    void on_width_valueChanged(int);
    void on_height_valueChanged(int);
    void on_mode_currentIndexChanged(int);
    void on_screen_currentIndexChanged(int);
    void on_smoothness_valueChanged(int);
    void on_framerate_valueChanged(int);
    void on_crop_stream_stateChanged(int);

private:
    friend class AmbientTests;
    Ui::Ambient *ui;
    QTimer* preview_timer = nullptr;
    ScreenCapturer* capturer = nullptr;
    RectangleSelectorOverlay* rectangle_selector_overlay = nullptr;

    int screen_index = -1;
    std::atomic<AmbientMode> mode{SCALED_AVERAGE};

    void UpdateSelection();
    void SetDynamicStrings();
    void ApplyPerformancePreset(int);
    void SyncPerformanceSettings();
    void AcceptFrame(const QImage&);
    void RenderFrame(const std::vector<ControllerZone*>&, bool preview);
    using Clock = std::chrono::steady_clock;
    struct Frame { QImage pixels; RGBColor average = 0; Clock::time_point captured; };
    struct ZoneState
    {
        std::vector<std::array<double, 3>> smoothed;
        std::vector<RGBColor> previous_targets;
        Clock::time_point updated{}, transmitted{};
    };
    Frame latest_frame;
    std::map<ControllerZone*, ZoneState> zone_states;
    std::map<RGBControllerInterface*, bool> dirty_controllers;
    std::map<RGBControllerInterface*, Clock::time_point> last_transmissions;
    std::map<RGBControllerInterface*, Clock::time_point> next_transmissions;
    Clock::time_point last_analysis{}, last_preview{};
    AmbientPerformanceMetrics processing_metrics{"processing"};
    AmbientPerformanceMetrics render_metrics{"render"};
    bool syncing_performance = false;
    std::atomic<unsigned int> effect_fps{60}, ambient_brightness{100};
    std::atomic<int> ambient_temperature{0}, ambient_tint{0};
    std::atomic<unsigned int> rgb_fps{60}, analysis_fps{60}, analysis_width{320};
    std::atomic<unsigned int> color_threshold{2}, preview_fps{10};
    std::atomic<bool> preview_enabled{true}, preview_visible{false};
    std::atomic<int> performance_mode{1};

    std::atomic<unsigned int> left{0};
    std::atomic<unsigned int> top{0};
    std::atomic<unsigned int> width{1};
    std::atomic<unsigned int> height{1};
    std::atomic<unsigned int> smoothness{80};
    std::atomic<unsigned int> framerate{60};
    std::atomic<bool> crop_stream{false};
    QString restore_token;

    std::mutex lock;
};
