/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <QApplication>
#include <QPainter>
#include <QScreen>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <atomic>
#include <chrono>
#include <cmath>
#include <thread>
#include <Windows.h>
#include <iostream>
#include <algorithm>
#include <Ambient.h>
#include "MockController.h"
using BenchClock = std::chrono::steady_clock;
static std::vector<double> capture_times;
void RecordCapture(double ms) { capture_times.push_back(ms); }
static QJsonObject Summarize(std::vector<double> samples)
{
    if(samples.empty()) return {};
    double sum = 0;
    for(double sample : samples) sum += sample;
    std::sort(samples.begin(),samples.end());
    return {{"samples",int(samples.size())},{"mean_ms",sum/samples.size()},
            {"p95_ms",samples[(samples.size()-1)*95/100]},{"max_ms",samples.back()}};
}
static uint64_t CpuTime()
{
    FILETIME creation,exit,kernel,user;
    GetProcessTimes(GetCurrentProcess(), &creation,&exit,&kernel,&user);
    ULARGE_INTEGER k{},u{};
    k.LowPart=kernel.dwLowDateTime; k.HighPart=kernel.dwHighDateTime;
    u.LowPart=user.dwLowDateTime; u.HighPart=user.dwHighDateTime;
    return k.QuadPart+u.QuadPart;
}
class AmbientTests
{
public:
    static ScreenCapturer* Capturer(Ambient& effect) { return effect.capturer; }
};
class TestWindow : public QWidget
{
public:
    int tick = 0, paint_count = 0;
    bool was_visible = false;
    void paintEvent(QPaintEvent*) override
    {
        ++paint_count;
        was_visible |= bool(IsWindowVisible(reinterpret_cast<HWND>(winId())));
        QPainter p(this);
        p.fillRect(rect(),QColor(18,18,25));
        for(int y=0;y<height();y+=48) for(int x=0;x<width();x+=48)
        {
            p.fillRect(x,y,46,46,QColor::fromHsv((x+y+tick*3)%360,200,180));
        }
        p.setPen(Qt::white);
        p.drawText(20,30,"Ambient benchmark - test window (no physical devices)");
    }
};
int main(int argc,char** argv)
{
    QApplication app(argc,argv);
    const QString scenario = argc>1 ? argv[1] : "idle";
    const int capture_fps = argc>2 ? std::atoi(argv[2]) : 20;
    const int duration = argc>3 ? std::atoi(argv[3]) : 12;
    const QString effect_mode = argc>4 ? argv[4] : "average";
    Ambient effect;
    effect.SetFPS(capture_fps);
    json settings{{"width",1},{"height",1},{"framerate",capture_fps},{"smoothness",80}};
#ifndef BENCH_BASELINE
    const bool gdi = scenario.startsWith("gdi-");
    settings["performance"] = {{"mode",gdi ? 0 : 3},{"capture_fps",capture_fps}, {"analysis_fps",capture_fps},
                               {"rgb_fps",capture_fps},{"analysis_width",320},{"smoothing",80}, {"threshold",2},
                               {"preview_enabled",false},{"preview_fps",10}};
#endif
    settings["mode"] = effect_mode == "copy" ? 1 : 0;
    effect.LoadCustomSettings(settings);
    MockController controller;
    controller.type=ZONE_TYPE_MATRIX;
    controller.count=256; controller.matrix_width=16; controller.matrix_height=16;
    controller.mapping.resize(256);
    for(unsigned int i=0;i<256;++i) controller.mapping[i]=i;
    ControllerZone zone(&controller,0,false,100,true,false);
    std::atomic<unsigned int> frames{0};
    QObject::connect(AmbientTests::Capturer(effect), &ScreenCapturer::OnImage,
                     [&frames](const QImage&){ ++frames; });
    TestWindow window;
    window.setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus);
    window.setScreen(QGuiApplication::screens()[0]);
    window.setWindowTitle("Ambient performance benchmark");
    window.resize(640,420);
    const QRect screen=QGuiApplication::screens()[0]->geometry();
    window.move(screen.topLeft()+QPoint(60,70));
    window.show();
    window.hide();
    window.show();
    QApplication::processEvents();
    QTimer animation;
    std::vector<double> ui_ticks;
    auto previous_tick=BenchClock::now();
    QObject::connect(&animation, &QTimer::timeout, [&]()
    {
        const auto now=BenchClock::now();
        ui_ticks.push_back(std::chrono::duration<double,std::milli>(now-previous_tick).count());
        previous_tick=now;
        ++window.tick;
        if(scenario.contains("move")) window.move(screen.topLeft()+QPoint(100+int(70*std::sin(window.tick*.1)),100+int(50*std::cos(window.tick*.1))));
        if(scenario.contains("resize")) window.resize(700+int(130*std::sin(window.tick*.1)),450+int(80*std::cos(window.tick*.1)));
        if(scenario != "idle" && scenario != "gdi-idle") window.update();
    });
    animation.setTimerType(Qt::PreciseTimer);
    animation.start(16);
    std::atomic<bool> running{true};
    std::vector<double> render_times;
    const auto start=BenchClock::now();
    const uint64_t cpu_start=CpuTime();
    effect.EffectState(true);
    std::thread output([&]()
    {
        auto deadline=BenchClock::now();
        while(running)
        {
            const auto begin=BenchClock::now();
            effect.StepEffect({&zone});
#ifndef BENCH_BASELINE
            if(effect.NeedsControllerUpdate(&controller)) controller.UpdateLEDs();
#else
            controller.UpdateLEDs();
#endif
            render_times.push_back(std::chrono::duration<double,std::milli>(BenchClock::now()-begin).count());
            deadline+=std::chrono::microseconds(1000000/capture_fps);
            if(deadline<BenchClock::now()) deadline=BenchClock::now()+std::chrono::microseconds(1000000/capture_fps);
            std::this_thread::sleep_until(deadline);
        }
    });
    QTimer::singleShot(duration*1000,&app,&QApplication::quit);
    app.exec();
    running=false; output.join();
    effect.EffectState(false);
    const double seconds=std::chrono::duration<double>(BenchClock::now()-start).count();
    SYSTEM_INFO info{}; GetSystemInfo(&info);
    const double core_percent=(CpuTime()-cpu_start)/10000000.0/seconds*100;
    MONITORINFOEXW window_monitor{};
    window_monitor.cbSize = sizeof(window_monitor);
    GetMonitorInfoW(MonitorFromWindow(reinterpret_cast<HWND>(window.winId()), MONITOR_DEFAULTTONEAREST), &window_monitor);
    QJsonObject result{{"scenario",scenario}, {"effect_mode",effect_mode},
                      {"window_monitor",QString::fromWCharArray(window_monitor.szDevice)},{"requested_fps",capture_fps},{"duration_s",seconds},
                      {"monitor",QGuiApplication::screens()[0]->name()},
                      {"refresh_rate",QGuiApplication::screens()[0]->refreshRate()},
                      {"test_window_visible",window.was_visible},
                      {"test_window_paints",window.paint_count},
                      {"logical_screen_width",screen.width()},{"logical_screen_height",screen.height()},
                      {"cpu_percent_total_machine",core_percent/info.dwNumberOfProcessors},
                      {"cpu_percent_one_core",core_percent}, {"delivered_frames",int(frames.load())},
                      {"delivered_fps",frames/seconds},{"capture_or_poll_wall_time",Summarize(capture_times)},
                      {"render_wall_time",Summarize(render_times)},{"qt_gui_timer_intervals",Summarize(ui_ticks)},
                      {"rgb_updates_simulated",int(controller.transmissions)},
                      {"gpu_usage_measured",false},{"dwm_present_latency_measured",false}};
#ifdef BENCH_BASELINE
    result["build"]="baseline";
#else
    result["build"]="optimized";
#endif
    std::cout<<QJsonDocument(result).toJson(QJsonDocument::Indented).constData();
}
