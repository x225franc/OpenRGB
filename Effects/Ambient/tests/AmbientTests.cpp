/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <QApplication>
#include <QImage>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QPluginLoader>
#include <QJsonDocument>
#include <iostream>
#include <stdexcept>
#include <thread>
#include "Ambient.h"
#include "MockController.h"
#include "OpenRGBPluginInterface.h"

class AmbientTests
{
    static void Check(bool value, const char* label)
    {
        if(!value) throw std::runtime_error(label);
        std::cout << "PASS " << label << '\n';
    }
    static json Settings(int mode = 0, unsigned int threshold = 2, unsigned int smoothing = 0)
    {
        return {{"width", 4}, {"height", 2}, {"mode", mode},
                {"performance", {{"mode", 3}, {"capture_fps", 20}, {"analysis_fps", 20}, {"rgb_fps", 20},
                                 {"analysis_width", 320}, {"smoothing", smoothing}, {"threshold", threshold},
                                 {"preview_enabled", true}, {"preview_fps", 10}}}};
    }
    static void Publish(Ambient& ambient, const QImage& frame)
    {
        ambient.last_analysis = {};
        ambient.AcceptFrame(frame);
    }
public:
    static void Run()
    {
        Ambient ambient;
        MockController controller;
        ControllerZone zone(&controller, 0, false, 100, true, false);
        const std::vector<ControllerZone*> zones{&zone};
        Check(ambient.GetFPS() == 60, "new instances default to balanced 60 FPS");
        ambient.LoadCustomSettings({{"framerate", 37}, {"smoothness", 23}, {"width", 4}, {"height", 2}});
        const auto legacy = ambient.SaveCustomSettings();
        Check(legacy["performance"]["mode"] == 0 && legacy["framerate"] == 37 && legacy["smoothness"] == 23,
              "legacy profiles retain capture rate, smoothing and native analysis");
        Check(legacy["performance"]["analysis_width"] == 0 && legacy["performance"]["threshold"] == 0,
              "legacy defaults have no downsampling or color threshold");
        Check(ambient.NeedsControllerUpdate(&controller) && ambient.NeedsControllerUpdate(&controller),
              "zero threshold retains unconditional legacy transmissions");
        ambient.LoadCustomSettings(Settings());
        const auto saved = ambient.SaveCustomSettings();
        Ambient roundtrip;
        roundtrip.LoadCustomSettings(saved);
        Check(roundtrip.SaveCustomSettings() == saved, "all new settings round-trip");
        ambient.SetFPS(7);
        Check(ambient.GetFPS() == 7, "global FPS upper bound is respected");
        ambient.SetFPS(60);

        QImage solid(4,2,QImage::Format_RGB32);
        solid.fill(qRgb(120,60,30));
        Publish(ambient, solid);
        ambient.StepEffect(zones);
        Check(controller.colors[7] == ToRGBColor(120,60,30) && controller.colors[0] == 0,
              "average mode uses zone-relative LED offsets");
        Check(ambient.NeedsControllerUpdate(&controller), "changed colors request a hardware update");
        auto saturation_settings = Settings();
        saturation_settings["saturation"] = 0;
        ambient.LoadCustomSettings(saturation_settings);
        ambient.StepEffect(zones);
        Check(controller.colors[7] == ToRGBColor(120,120,120), "zero saturation produces grayscale without lowering HSV value");
        saturation_settings["saturation"] = 200;
        ambient.LoadCustomSettings(saturation_settings);
        ambient.StepEffect(zones);
        Check(RGBGetRValue(controller.colors[7]) == 120 && RGBGetBValue(controller.colors[7]) == 0,
              "boosted saturation clamps safely while preserving HSV value");
        Ambient saturation_roundtrip;
        saturation_roundtrip.LoadCustomSettings(ambient.SaveCustomSettings());
        Check(saturation_roundtrip.SaveCustomSettings()["saturation"] == 200, "saturation survives profile roundtrip");
        ambient.LoadCustomSettings(Settings());
        ambient.StepEffect(zones);
        Check(controller.colors[7] == ToRGBColor(120,60,30), "legacy profiles restore neutral saturation exactly");
        ambient.NeedsControllerUpdate(&controller);

        ambient.StepEffect(zones);
        Check(!ambient.NeedsControllerUpdate(&controller), "identical colors suppress hardware updates");
        ambient.SetBrightness(50);
        ambient.StepEffect(zones);
        Check(controller.colors[7] == ColorUtils::apply_adjustments(ToRGBColor(120,60,30), .5f, 0, 0),
              "brightness is applied once and participates in dirty detection");
        std::this_thread::sleep_for(std::chrono::milliseconds(55));
        Check(ambient.NeedsControllerUpdate(&controller), "pending brightness change survives the RGB frame-rate limiter");
        ambient.StepEffect(zones);
        std::this_thread::sleep_for(std::chrono::milliseconds(55));
        Check(!ambient.NeedsControllerUpdate(&controller), "brightness-adjusted stable colors suppress updates");
        ambient.dirty_controllers[&controller] = true;
        ambient.next_transmissions[&controller] = Ambient::Clock::now() + std::chrono::milliseconds(50);
        Check(!ambient.NeedsControllerUpdate(&controller), "RGB deadline prevents scheduler over-transmission");
        ambient.StepEffect(zones);
        std::this_thread::sleep_for(std::chrono::milliseconds(60));
        Check(ambient.NeedsControllerUpdate(&controller), "unsent changes survive an intervening effect step");
        ambient.SetBrightness(100);

        QImage spatial(4,2,QImage::Format_RGB32);
        for(int y=0;y<2;++y) for(int x=0;x<4;++x) spatial.setPixel(x,y,qRgb(10+x*30,y*100,0));
        ambient.LoadCustomSettings(Settings(1));
        Publish(ambient, spatial);
        ambient.StepEffect(zones);
        Check(controller.colors[7] == ToRGBColor(10,100,0) && controller.colors[10] == ToRGBColor(100,100,0),
              "linear screen-copy mapping preserves nearest-neighbour positions");
        zone.reverse = true;
        ambient.StepEffect(zones);
        Check(controller.colors[7] == ToRGBColor(100,100,0), "linear reverse mapping");
        zone.reverse = false;
        controller.type = ZONE_TYPE_MATRIX;
        controller.mapping = {0, 0xffffffff, 2, 3};
        ambient.StepEffect(zones);
        Check(controller.colors[7] == ToRGBColor(40,0,0) && controller.colors[9] == ToRGBColor(40,100,0),
              "matrix mapping skips holes before reading/writing LED indices");
        controller.invalidate_after_snapshot = true;
        ambient.StepEffect(zones);
        Check(controller.raw_map_reads == 0 && controller.colors[9] == ToRGBColor(40,100,0),
              "matrix rendering owns its snapshot when Visual Map replaces the live map");
        controller.invalidate_after_snapshot = false;
        controller.matrix_width = controller.matrix_height = 2;
        controller.mapping = {0};
        const auto before_incomplete = controller.colors;
        ambient.StepEffect(zones);
        Check(controller.colors == before_incomplete, "incomplete matrix snapshot is skipped without an out-of-bounds read");
        controller.mapping = {0, 0xffffffff, 2, 3};
        controller.type = ZONE_TYPE_LINEAR;
        ControllerZone segment(&controller, 0, false, 100, true, true, 0);
        ambient.StepEffect({&segment});
        Check(controller.colors[10] == ToRGBColor(10,100,0), "segment offsets are preserved");

        ambient.LoadCustomSettings(Settings(0, 20, 80));
        solid.fill(qRgb(220,120,40));
        Publish(ambient, solid);
        ambient.OnControllerZonesListChanged(zones);
        for(int i=0;i<25;++i)
        {
            ambient.StepEffect(zones);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        Check(controller.colors[7] == ToRGBColor(220,120,40), "smoothing converges to exact target despite threshold");
        Check(ambient.zone_states[&zone].smoothed[0][0] == 220, "fractional smoothing state avoids rounding stalls");
        solid.fill(qRgb(100,30,20)); Publish(ambient, solid);
        solid.fill(qRgb(150,60,30)); Publish(ambient, solid);
        Check(ambient.latest_frame.average == ToRGBColor(150,60,30), "latest frame replaces stale input without a queue");
        ambient.preview_visible = true;
        Check(ambient.PreparePreview() && !ambient.PreparePreview(), "preview has its own frame-rate limiter");
        ambient.preview_visible = false;
        std::this_thread::sleep_for(std::chrono::milliseconds(110));
        Check(!ambient.PreparePreview(), "hidden preview skips rendering");
        ambient.preview_visible = true;
        ambient.preview_enabled = false;
        Check(!ambient.PreparePreview(), "preview toggle disables rendering");

        ambient.preview_enabled = true;
        ambient.preview_fps = 60;
        ambient.last_preview = {};
        Check(ambient.PreparePreview(), "preview supports 60 FPS independently of capture");

        // Start/stop twice while switching output and resolution. No device IO is involved.
        std::atomic<unsigned int> captured_frames{0};
        QObject::connect(ambient.capturer, &ScreenCapturer::OnImage, [&captured_frames](const QImage&){ ++captured_frames; });
        ambient.resize(760, 560);
        if(!qEnvironmentVariableIsSet("AMBIENT_TEST_HEADLESS"))
        {
            ambient.show();
            QApplication::processEvents();
            ambient.grab().save(QString::fromLocal8Bit(qgetenv("AMBIENT_TEST_SCREENSHOT")));
        }
        for(int i=0;i<2;++i)
        {
            ambient.EffectState(true);
            for(int tick=0; tick<50; ++tick)
            {
                QApplication::processEvents();
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
            ambient.capturer->SetAnalysisWidth(160);
            ambient.capturer->SetScreen(0);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            const auto start = Ambient::Clock::now();
            ambient.EffectState(false);
            Check(Ambient::Clock::now()-start < std::chrono::seconds(2), "capture worker stops and can restart cleanly");
        }
        Check(captured_frames > 0, "Windows backend captures actual desktop frames");
    }
};
int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    try
    {
        if(argc > 1)
        {
            QPluginLoader loader(QString::fromLocal8Bit(argv[1]));
            auto* plugin = qobject_cast<OpenRGBPluginInterface*>(loader.instance());
            if(!plugin) throw std::runtime_error(loader.errorString().toStdString());
            if(plugin->GetPluginAPIVersion() != OPENRGB_PLUGIN_API_VERSION) throw std::runtime_error("plugin API mismatch");
            std::cout << "PASS plugin DLL loads with Qt " << qVersion() << " and API " << plugin->GetPluginAPIVersion() << '\n';
            if(argc > 2 && plugin->GetPluginInfo().Commit != std::string(argv[2]))
                throw std::runtime_error("built DLL commit does not match the reviewed source commit");
            std::cout << "INFO built DLL commit " << plugin->GetPluginInfo().Commit << '\n';
            loader.unload();
        }
        AmbientTests::Run();
        return 0;
    }
    catch(const std::exception& error) { std::cerr << "FAIL " << error.what() << '\n'; return 1; }
}
