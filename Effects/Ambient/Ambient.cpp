/*---------------------------------------------------------*\
| Ambient.cpp                                               |
|                                                           |
|   OpenRGB Effects Plugin Ambient Effect                   |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include "Ambient.h"
#include <QDebug>
#include <QSignalBlocker>
#include <QTimer>
#include <cmath>
#include <algorithm>
#include "LivePreviewController.h"
#include "ColorUtils.h"
#include "SpatialBlending.h"
#include "OpenRGBEffectSettings.h"
#include "QtScreenCapturer.h"

#ifdef __linux__
#include "WaylandScreenCapturer.h"
#elif _WIN32
#include "WindowsScreenCapturer.h"
#endif

REGISTER_EFFECT(Ambient);

Ambient::Ambient(QWidget *parent) :
    RGBEffect(parent),
    ui(new Ui::Ambient)
{
    ui->setupUi(this);
    connect(ui->saturation, &QSlider::valueChanged, this, [this](int value) {
        ambient_saturation = value;
        ui->saturation_value->setValue(value);
    });
    connect(ui->black_boost, &QSlider::valueChanged, this, [this](int value)
    {
        ambient_black_boost = value;
        ui->black_boost_value->setValue(value);
    });
    connect(ui->blending, &QSlider::valueChanged, this, [this](int value)
    {
        ambient_blending = value;
        ui->blending_value->setValue(value);
    });
    connect(ui->blending_value, QOverload<int>::of(&QSpinBox::valueChanged), ui->blending, &QSlider::setValue);
    connect(ui->black_boost_value, QOverload<int>::of(&QSpinBox::valueChanged), ui->black_boost, &QSlider::setValue);
    connect(ui->saturation_value, QOverload<int>::of(&QSpinBox::valueChanged),
            ui->saturation, &QSlider::setValue);

    connect(ui->real_ambilight, &QCheckBox::toggled, this, [this](bool enabled) { real_ambilight = enabled; });
    connect(ui->ultrawide_mode, &QCheckBox::toggled, this, [this](bool enabled) { ultrawide_mode = enabled; });
    SetDynamicStrings();
    EffectDetails.EffectClassName   = ClassName();
    EffectDetails.HasCustomSettings = true;
    EffectDetails.SupportsRandom = false;

    ui->select_screen->hide();
    ui->crop_frame->hide();

#ifdef __linux__
    bool isWayland = qgetenv("XDG_SESSION_TYPE") == "wayland";

    if(isWayland)
    {
        capturer = new WaylandScreenCapturer();

        ui->select_rectangle->hide();
        ui->screen_label->hide();
        ui->screen->hide();        
        ui->select_screen->show();
    }
#elif _WIN32
    capturer = new WindowsScreenCapturer();
#endif

    if(capturer == nullptr)
    {
        capturer = new QtScreenCapturer();
    }

    connect(ui->keep_during_screensaver, &QCheckBox::toggled, this, [this](bool value) {
        capturer->SetKeepEffectDuringScreensaver(value);
    });
#ifndef _WIN32
    ui->keep_during_screensaver->hide();
#endif
    ui->framerate->setValue(OpenRGBEffectSettings::globalSettings.fpscapture);

    rectangle_selector_overlay = new RectangleSelectorOverlay(this);

    connect(rectangle_selector_overlay, &RectangleSelectorOverlay::SelectionUpdated, [=](QRect rect){
        ui->left->setValue(rect.left());
        ui->top->setValue(rect.top());
        ui->width->setValue(rect.width());
        ui->height->setValue(rect.height());
    });

    QList<QScreen*> screens = QGuiApplication::screens();

    for(QScreen* screen: screens)
    {
        ui->screen->addItem(screen->name());
    }

    // Direct bounded publication: no queued images accumulate on the Qt GUI thread.
    connect(capturer, &ScreenCapturer::OnImage, this, &Ambient::AcceptFrame, Qt::DirectConnection);

    connect(capturer, &ScreenCapturer::OnError, [&](const ScreenCapturerError& err, const QString& message){
        qDebug() << "ScreenCapturer::OnError" << err << message;
    });

    capturer->SetScreen(0);

    connect(capturer, &ScreenCapturer::OnRestoreTokenProvided,[&](const QString token){
        qDebug() << "[Ambient] capturer provided a restore token";
        restore_token = token;
    });
    ui->analysis_width->addItem(tr("Native"), 0);
    for(const int value : {160, 320, 640, 1280}) ui->analysis_width->addItem(QString::number(value), value);
    connect(ui->performance_mode, qOverload<int>(&QComboBox::currentIndexChanged), this, &Ambient::ApplyPerformancePreset);
    auto custom = [this]()
    {
        if(syncing_performance) return;
        const QSignalBlocker blocker(ui->performance_mode);
        ui->performance_mode->setCurrentIndex(3);
        SyncPerformanceSettings();
    };
    for(auto* spin : {ui->capture_fps, ui->analysis_fps, ui->rgb_fps, ui->temporal_smoothing, ui->color_threshold, ui->preview_fps})
        connect(spin, qOverload<int>(&QSpinBox::valueChanged), this, custom);
    connect(ui->analysis_width, qOverload<int>(&QComboBox::currentIndexChanged), this, custom);
    connect(ui->preview_enabled, &QCheckBox::toggled, this, custom);
    ApplyPerformancePreset(1);

    // Visibility is queried on the GUI thread, never by an effect/capture worker.
    preview_timer = new QTimer(this);
    connect(preview_timer, &QTimer::timeout, this, [this]()
    {
        QWidget* page = parentWidget();
        while(page && page->metaObject()->className() != QString("OpenRGBEffectPage")) page = page->parentWidget();
        bool visible = false;
        if(page)
            for(auto* preview : page->findChildren<LivePreviewController*>())
                visible |= preview->isVisible() && !preview->window()->isMinimized();
        preview_visible = visible;
    });


}

Ambient::~Ambient()
{
    capturer->Stop();
    processing_metrics.Report();
    render_metrics.Report();
    delete capturer;
    delete rectangle_selector_overlay;
    delete ui;
}

void Ambient::changeEvent(QEvent *event)
{
    if(event->type() == QEvent::LanguageChange)
    {
        ui->retranslateUi(this);
        SetDynamicStrings();
    }
}

void Ambient::SetDynamicStrings()
{
    EffectDetails.EffectName        = tr(UI_Name().c_str()).toStdString();
    EffectDetails.EffectDescription = tr("Takes a portion of the screen and reflect it to your devices").toStdString();
    const QSignalBlocker mode_blocker(ui->mode);
    const int selected_mode = ui->mode->currentIndex();
    ui->mode->clear();
    ui->mode->addItems({
                           tr("Scaled average"),
                           tr("Screen copy"),
                       });
    ui->mode->setCurrentIndex(std::max(0, selected_mode));
    const QSignalBlocker preset_blocker(ui->performance_mode);
    const int selected_preset = ui->performance_mode->currentIndex();
    ui->performance_mode->clear();
    ui->performance_mode->addItems({tr("Standard"), tr("Équilibré"), tr("Performance"), tr("Personnalisé")});
    ui->performance_mode->setCurrentIndex(std::max(0, selected_preset));
}

void Ambient::EffectState(const bool state)
{
    EffectEnabled = state;

    if(state)
    {
        { std::lock_guard<std::mutex> guard(lock); latest_frame = Frame{}; }
        last_analysis = Clock::time_point{};
        qDebug() << "[Ambient] Start capturer";
        capturer->Start();
        QMetaObject::invokeMethod(preview_timer, "start", Qt::AutoConnection, Q_ARG(int, 250));
    }
    else
    {
        QMetaObject::invokeMethod(preview_timer, "stop", Qt::AutoConnection);
        preview_visible = false;
        capturer->Stop();
    }
}

void Ambient::ApplyPerformancePreset(int preset)
{
    syncing_performance = true;
    const bool standard = preset == 0;
    if(preset != 3)
    {
        ui->capture_fps->setValue(standard ? ui->framerate->value() : preset == 2 ? 30 : 60);
        ui->analysis_fps->setValue(ui->capture_fps->value());
        ui->rgb_fps->setValue(standard ? int(effect_fps.load()) : preset == 2 ? 30 : 60);
        ui->analysis_width->setCurrentIndex(standard ? 0 : preset == 2 ? 1 : 2);
        ui->color_threshold->setValue(standard ? 0 : preset == 2 ? 3 : 2);
        ui->temporal_smoothing->setValue(ui->smoothness->value());
        ui->preview_fps->setValue(10);
        ui->preview_enabled->setChecked(true);
    }
    { const QSignalBlocker blocker(ui->performance_mode); ui->performance_mode->setCurrentIndex(preset); }
    syncing_performance = false;
    SyncPerformanceSettings();
}

void Ambient::SyncPerformanceSettings()
{
    performance_mode = ui->performance_mode->currentIndex();
    rgb_fps = ui->rgb_fps->value();
    analysis_fps = ui->analysis_fps->value();
    analysis_width = ui->analysis_width->currentData().toUInt();
    smoothness = ui->temporal_smoothing->value();
    color_threshold = ui->color_threshold->value();
    preview_fps = ui->preview_fps->value();
    preview_enabled = ui->preview_enabled->isChecked();
    {
        const QSignalBlocker smoothing_blocker(ui->smoothness), capture_blocker(ui->framerate);
        ui->smoothness->setValue(smoothness);
        ui->framerate->setValue(ui->capture_fps->value());
    }
    framerate = ui->capture_fps->value();
    capturer->SetFrameRate(framerate);
    capturer->SetAnalysisWidth(analysis_width);
    capturer->SetModernCapture(performance_mode != 0);
}

unsigned int Ambient::GetFPS()
{
    // Existing global FPS remains an upper bound; the Ambient RGB limiter is independent of capture.
    return std::max(1u, std::min(effect_fps.load(), rgb_fps.load()));
}

void Ambient::AcceptFrame(const QImage& input)
{
    if(input.isNull()) return;
    const auto now = Clock::now();
    const unsigned int rate = std::max(1u, analysis_fps.load());
    if(rate < framerate.load())
    {
        if(now < last_analysis) return;
        const auto period = std::chrono::microseconds(1000000 / rate);
        last_analysis = last_analysis == Clock::time_point{} ? now + period : last_analysis + period;
        if(last_analysis < now) last_analysis = now + period;
    }
    if(capturer->IsPaused()) return;
    const auto start = Clock::now();
    Frame frame;
    frame.captured = now;
    frame.pixels = input; // Implicitly shared immutable image, no full-screen copy.
    if(crop_stream)
    {
        const QSize source = capturer->SourceSize().isEmpty() ? input.size() : capturer->SourceSize();
        const double sx = double(input.width()) / source.width(), sy = double(input.height()) / source.height();
        const QRect region(int(left * sx), int(top * sy), int(std::ceil(width * sx)), int(std::ceil(height * sy)));
        const QRect clipped = region.intersected(input.rect());
        frame.pixels = clipped.isEmpty() ? QImage() : input.copy(clipped);
    }
    if(ultrawide_mode && !frame.pixels.isNull())
    {
        const int center_width = std::min(frame.pixels.width(), frame.pixels.height() * 16 / 9);
        if(center_width > 0 && center_width < frame.pixels.width())
            frame.pixels = frame.pixels.copy((frame.pixels.width()-center_width)/2, 0, center_width, frame.pixels.height());
    }
    // Qt/Linux capturers have no early reduction; use bounded analysis there as well.
    const int limit = int(analysis_width.load());
    if(limit > 0 && frame.pixels.width() > limit) frame.pixels = frame.pixels.scaledToWidth(limit, Qt::FastTransformation);
    if(!frame.pixels.isNull())
    {
        if(frame.pixels.format() != QImage::Format_RGB32 && frame.pixels.format() != QImage::Format_ARGB32)
            frame.pixels = frame.pixels.convertToFormat(QImage::Format_RGB32);
        uint64_t r = 0, g = 0, b = 0;
        // Average is evaluated once per frame on the capture worker, not once per zone or preview.
        for(int y = 0; y < frame.pixels.height(); ++y)
        {
            const auto* row = reinterpret_cast<const QRgb*>(frame.pixels.constScanLine(y));
            for(int x = 0; x < frame.pixels.width(); ++x) { r += qRed(row[x]); g += qGreen(row[x]); b += qBlue(row[x]); }
        }
        const uint64_t count = uint64_t(frame.pixels.width()) * frame.pixels.height();
        frame.average = ToRGBColor(r/count, g/count, b/count);
        if(ambient_blending && mode == SCREEN_COPY)
            frame.neighborhood = AmbientSpatialNeighborhood(frame.pixels);
    }
    {
        std::lock_guard<std::mutex> guard(lock);
        latest_frame = std::move(frame); // A single latest slot, never a queue of stale frames.
    }
    processing_metrics.Add(start);
}

void Ambient::OnControllerZonesListChanged(std::vector<ControllerZone*>)
{
    zone_states.clear();
    dirty_controllers.clear();
    last_transmissions.clear();
    next_transmissions.clear();
}

bool Ambient::NeedsControllerUpdate(RGBControllerInterface* controller)
{
    const auto now = Clock::now();
    const auto dirty = dirty_controllers.find(controller);
    const bool changed = dirty != dirty_controllers.end() && dirty->second;
    auto& previous = last_transmissions[controller];
    auto& deadline = next_transmissions[controller];
    const bool limited = performance_mode != 0;
    if(limited && now < deadline) return false;
    // Zero disables suppression for legacy/device compatibility. A 1 Hz keepalive
    // preserves direct-mode streams on devices which cannot hold a color indefinitely.
    if(color_threshold == 0 || changed || now - previous >= std::chrono::seconds(1))
    {
        previous = now;
        dirty_controllers[controller] = false;
        if(limited)
        {
            const auto period = std::chrono::microseconds(1000000 / GetFPS());
            deadline = deadline == Clock::time_point{} ? now + period : deadline + period;
            if(deadline < now) deadline = now + period;
        }
        return true;
    }
    return false;
}

bool Ambient::PreparePreview()
{
    if(!preview_enabled || !preview_visible) return false;
    const auto now = Clock::now();
    if(now < last_preview) return false;
    const auto period = std::chrono::microseconds(1000000 / std::max(1u, preview_fps.load()));
    last_preview = last_preview == Clock::time_point{} ? now + period : last_preview + period;
    if(last_preview < now) last_preview = now + period;
    return true;
}

void Ambient::StepPreview(std::vector<ControllerZone*> zones) { RenderFrame(zones, true); }
void Ambient::StepEffect(std::vector<ControllerZone*> zones)
{
    // Keep unsent changes until the independent RGB deadline permits transmission.
    RenderFrame(zones, false);
}

void Ambient::RenderFrame(const std::vector<ControllerZone*>& zones, bool preview)
{
    if(capturer->IsPaused()) return;
    const auto start = Clock::now();
    Frame frame;
    { std::lock_guard<std::mutex> guard(lock); frame = latest_frame; }
    const bool black = width == 0 || height == 0 || (crop_stream && frame.pixels.isNull());
    if(frame.pixels.isNull() && !black) return;
    const auto effect_mode = mode.load();
    const unsigned int smoothing = smoothness.load();
    const int brightness = ambient_brightness.load(), temperature = ambient_temperature.load(), tint = ambient_tint.load();
    const unsigned int saturation = ambient_saturation.load();
    const unsigned int black_boost = ambient_black_boost.load();
    const unsigned int blending = ambient_blending.load();
    const unsigned int threshold = preview ? 0 : color_threshold.load();
    // Equivalent to the old coefficient at 60 FPS, but based on actual elapsed time.
    const double tau = smoothing ? -1.0 / (60.0 * std::log(smoothing * 0.01)) : 0;
    for(auto* zone : zones)
    {
        const unsigned int count = zone->leds_count();
        if(!count) continue;
        auto& state = zone_states[zone];
        if(state.smoothed.size() != count)
        {
            state.smoothed.assign(count, {0, 0, 0});
            state.previous_targets.assign(count, 0);
            state.updated = Clock::time_point{};
        }
        const double dt = state.updated == Clock::time_point{} ? 1.0 / GetFPS() : std::chrono::duration<double>(start - state.updated).count();
        const double alpha = tau ? 1.0 - std::exp(-dt / tau) : 1.0;
        state.updated = start;
        const bool flush_small = start - state.transmitted >= std::chrono::milliseconds(250);
        bool changed = false;
        auto set = [&](unsigned int index, RGBColor target)
        {
            if(index == 0xffffffff || index >= count) return;
            const bool target_stable = state.previous_targets[index] == target;
            state.previous_targets[index] = target;
            auto& values = state.smoothed[index];
            const int channels[] = {RGBGetRValue(target), RGBGetGValue(target), RGBGetBValue(target)};
            bool settled = true;
            for(int c = 0; c < 3; ++c)
            {
                values[c] += alpha * (channels[c] - values[c]);
                if(std::abs(values[c] - channels[c]) < 0.5) values[c] = channels[c];
                else settled = false;
            }
            RGBColor color = ToRGBColor(int(std::round(values[0])), int(std::round(values[1])), int(std::round(values[2])));
            if(saturation != 100)
            {
                QColor hsv(RGBGetRValue(color), RGBGetGValue(color), RGBGetBValue(color));
                hsv.setHsv(hsv.hsvHue(), std::min(255, int(std::lround(hsv.hsvSaturation() * saturation / 100.0))), hsv.value());
                color = ToRGBColor(hsv.red(), hsv.green(), hsv.blue());
            }
            // Lift the black point before brightness. White stays white, and an
            // unavailable capture must remain off rather than becoming gray.
            if(black_boost && !black)
            {
                const int floor = int(std::lround(255.0 * black_boost / 100.0));
                auto lift = [floor](int channel) { return floor + (channel * (255 - floor) + 127) / 255; };
                color = ToRGBColor(lift(RGBGetRValue(color)), lift(RGBGetGValue(color)), lift(RGBGetBValue(color)));
            }
            const RGBColor adjusted = ColorUtils::apply_adjustments(color, (zone->self_brightness / 100.f) * (brightness / 100.f), temperature, tint);
            const RGBColor previous = zone->GetLED(index); // Includes zone/segment offset.
            const int delta = std::max({std::abs(int(RGBGetRValue(previous)) - int(RGBGetRValue(adjusted))),
                                        std::abs(int(RGBGetGValue(previous)) - int(RGBGetGValue(adjusted))),
                                        std::abs(int(RGBGetBValue(previous)) - int(RGBGetBValue(adjusted)))});
            if(delta && (delta >= int(threshold) || (settled && target_stable) || flush_small))
            {
                zone->SetLED(index, color, brightness, temperature, tint);
                changed = true;
            }
        };
        auto sample = [&](unsigned int x, unsigned int y, unsigned int w, unsigned int h)
        {
            if(black) return RGBColor(0);
            if(real_ambilight) y = h - 1;
            // Same nearest-neighbour spatial mapping as Qt's old fast scaling; no per-zone image allocations.
            const int sx = std::min(frame.pixels.width()-1, int((uint64_t(2*x+1) * frame.pixels.width()) / (2*w)));
            const int sy = std::min(frame.pixels.height()-1, int((uint64_t(2*y+1) * frame.pixels.height()) / (2*h)));
            const QRgb pixel = reinterpret_cast<const QRgb*>(frame.pixels.constScanLine(sy))[sx];
            if(blending && !frame.neighborhood.isNull())
            {
                const int bx = std::min(frame.neighborhood.width()-1, int((uint64_t(2*x+1)*frame.neighborhood.width())/(2*w)));
                const int by = std::min(frame.neighborhood.height()-1, int((uint64_t(2*y+1)*frame.neighborhood.height())/(2*h)));
                const QRgb nearby = reinterpret_cast<const QRgb*>(frame.neighborhood.constScanLine(by))[bx];
                auto mix = [blending](int original, int average) { return (original*(100-blending)+average*blending+50)/100; };
                return ToRGBColor(mix(qRed(pixel),qRed(nearby)), mix(qGreen(pixel),qGreen(nearby)), mix(qBlue(pixel),qBlue(nearby)));
            }
            return ToRGBColor(qRed(pixel), qGreen(pixel), qBlue(pixel));
        };
        if(effect_mode == SCALED_AVERAGE)
            for(unsigned int i = 0; i < count; ++i) set(i, black ? 0 : frame.average);
        else if(zone->type() == ZONE_TYPE_SINGLE || zone->type() == ZONE_TYPE_LINEAR || zone->type() == ZONE_TYPE_LINEAR_LOOP)
            for(unsigned int i = 0; i < count; ++i) set(i, sample(zone->reverse ? count-i-1 : i, 0, count, 1));
        else
        {
            // Visual Map can replace its matrix while an effect frame is rendering.
            // The SDK copies this value under the controller's access lock. A raw
            // map pointer outlives that lock and can point to a freed vector.
            const matrix_map_type mapping = zone->is_segment
                ? zone->controller->GetZoneSegmentMatrixMap(zone->zone_idx, zone->segment_idx)
                : zone->controller->GetZoneMatrixMap(zone->zone_idx);
            const unsigned int w = mapping.width, h = mapping.height;
            if(w && h && uint64_t(w) * h <= mapping.map.size())
                for(unsigned int y = 0; y < h; ++y)
                    for(unsigned int x = 0; x < w; ++x)
                        set(mapping.map[size_t(y)*w+x], sample(zone->reverse ? w-x-1 : x, y, w, h));
        }
        if(changed)
        {
            if(!preview) dirty_controllers[zone->controller] = true;
            state.transmitted = start;
        }
    }
    render_metrics.Add(start);
}

void Ambient::LoadCustomSettings(json settings)
{
    ui->real_ambilight->setChecked(settings.value("real_ambilight", false));
    ui->ultrawide_mode->setChecked(settings.value("ultrawide_mode", false));
    ui->saturation->setValue(settings.value("saturation", 100));
    ui->black_boost->setValue(settings.value("black_boost", 0));
    ui->blending->setValue(settings.value("blending", 0));
    ui->keep_during_screensaver->setChecked(settings.value("keep_effect_during_screensaver", false));
    if(settings.contains("left"))           ui->left->setValue(settings["left"]);
    if(settings.contains("top"))            ui->top->setValue(settings["top"]);
    if(settings.contains("width"))          ui->width->setValue(settings["width"]);
    if(settings.contains("height"))         ui->height->setValue(settings["height"]);
    if(settings.contains("mode"))           ui->mode->setCurrentIndex(settings["mode"]);
    if(settings.contains("screen_index"))   ui->screen->setCurrentIndex(settings["screen_index"]);
    if(settings.contains("smoothness"))     ui->smoothness->setValue(settings["smoothness"]);
    if(settings.contains("framerate"))      ui->framerate->setValue(settings["framerate"]);
    if(settings.contains("crop_stream"))    ui->crop_stream->setChecked(settings["crop_stream"]);

    if(settings.contains("performance"))
    {
        const auto& perf = settings["performance"];
        syncing_performance = true;
        ui->capture_fps->setValue(perf.value("capture_fps", 60));
        ui->analysis_fps->setValue(perf.value("analysis_fps", 60));
        ui->rgb_fps->setValue(perf.value("rgb_fps", 60));
        const int index = ui->analysis_width->findData(perf.value("analysis_width", 320));
        ui->analysis_width->setCurrentIndex(index >= 0 ? index : 2);
        ui->temporal_smoothing->setValue(perf.value("smoothing", 80));
        ui->color_threshold->setValue(perf.value("threshold", 2));
        ui->preview_enabled->setChecked(perf.value("preview_enabled", true));
        ui->preview_fps->setValue(perf.value("preview_fps", 10));
        { const QSignalBlocker blocker(ui->performance_mode); ui->performance_mode->setCurrentIndex(std::clamp(perf.value("mode", 1), 0, 3)); }
        syncing_performance = false;
        SyncPerformanceSettings();
    }
    else
    {
        // Legacy profiles retain native analysis, original capture FPS and zero threshold.
        ApplyPerformancePreset(0);
    }

    if(settings.contains("restore_token"))
    {
        restore_token = QString::fromStdString(settings["restore_token"]);
        capturer->SetToken(restore_token);
    }
}

json Ambient::SaveCustomSettings()
{
    json settings;

    settings["left"]            = left.load();
    settings["top"]             = top.load();
    settings["height"]          = height.load();
    settings["width"]           = width.load();
    settings["real_ambilight"] = real_ambilight.load();
    settings["ultrawide_mode"] = ultrawide_mode.load();
    settings["mode"]            = mode.load();
    settings["screen_index"]    = screen_index;
    settings["smoothness"]      = smoothness.load();
    settings["framerate"]       = framerate.load();
    settings["restore_token"]   = restore_token.toStdString();
    settings["crop_stream"]     = crop_stream.load();
    settings["saturation"]      = ambient_saturation.load();
    settings["black_boost"]     = ambient_black_boost.load();
    settings["blending"]        = ambient_blending.load();
    settings["keep_effect_during_screensaver"] = ui->keep_during_screensaver->isChecked();

    settings["performance"] = {
        {"mode", performance_mode.load()}, {"capture_fps", framerate.load()},
        {"analysis_fps", analysis_fps.load()}, {"rgb_fps", rgb_fps.load()},
        {"analysis_width", analysis_width.load()}, {"smoothing", smoothness.load()},
        {"threshold", color_threshold.load()}, {"preview_enabled", preview_enabled.load()},
        {"preview_fps", preview_fps.load()}
    };
    return settings;
}

void Ambient::on_left_valueChanged(int value)
{
    left = value;   
}

void Ambient::on_top_valueChanged(int value)
{
    top = value;
}

void Ambient::on_width_valueChanged(int value)
{
    width = value;
}

void Ambient::on_height_valueChanged(int value)
{
    height = value;
}

void Ambient::on_mode_currentIndexChanged(int value)
{
    mode = static_cast<AmbientMode>(value);
}

void Ambient::on_screen_currentIndexChanged(int value)
{
    screen_index = value;
    capturer->SetScreen(screen_index);
}

void Ambient::on_select_rectangle_clicked()
{
    rectangle_selector_overlay->StartSelection(screen_index);
}

void Ambient::on_select_screen_clicked()
{
    capturer->Init("", EffectEnabled);
}

void Ambient::on_smoothness_valueChanged(int value)
{
    smoothness = value;
    if(!syncing_performance) ui->temporal_smoothing->setValue(value);
}

void Ambient::on_framerate_valueChanged(int value)
{
    framerate = value;
    capturer->SetFrameRate(framerate);
    if(!syncing_performance) ui->capture_fps->setValue(value);
}

void Ambient::on_crop_stream_stateChanged(int value)
{
    crop_stream = value;
    ui->crop_frame->setVisible(value);
}
