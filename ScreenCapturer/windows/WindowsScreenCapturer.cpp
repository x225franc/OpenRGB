/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "WindowsScreenCapturer.h"
#include <QGuiApplication>
#include <QScreen>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QtGui/qscreen_platform.h>
#endif
#include <QDebug>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <Windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include "../../Effects/Ambient/PerformanceMetrics.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
using Microsoft::WRL::ComPtr;
using CaptureClock = std::chrono::steady_clock;

struct WindowsScreenCapturer::CaptureResources
{
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDXGIOutputDuplication> duplication;
    ComPtr<ID3D11Texture2D> source, target, staging[2];
    ComPtr<ID3D11ShaderResourceView> source_view;
    ComPtr<ID3D11RenderTargetView> target_view;
    ComPtr<ID3D11VertexShader> vertex_shader;
    ComPtr<ID3D11PixelShader> pixel_shader;
    ComPtr<ID3D11SamplerState> sampler;
    ComPtr<ID3D11Buffer> constants;
    bool pending[2] = {false, false};
    unsigned int write_index = 0;
    bool frame_held = false;
    uint64_t acquired = 0, timeouts = 0, readback_busy = 0;
    DXGI_MODE_ROTATION rotation = DXGI_MODE_ROTATION_IDENTITY;
    RECT rectangle{};
    QSize size;
    HDC desktop_dc = nullptr, bitmap_dc = nullptr;
    HBITMAP bitmap = nullptr;
    HGDIOBJ previous_bitmap = nullptr;
    void* pixels = nullptr;

    ~CaptureResources()
    {
        if(frame_held && duplication) duplication->ReleaseFrame();
        if(qEnvironmentVariableIsSet("OPENRGB_AMBIENT_METRICS") && duplication)
            qInfo() << "[Ambient DXGI] acquired" << acquired << "timeouts" << timeouts << "readback_busy" << readback_busy;
        if(bitmap_dc && previous_bitmap) SelectObject(bitmap_dc, previous_bitmap);
        if(bitmap) DeleteObject(bitmap);
        if(bitmap_dc) DeleteDC(bitmap_dc);
        if(desktop_dc) ReleaseDC(nullptr, desktop_dc);
    }

    bool Init(const QString& name, unsigned int analysis_width, bool modern)
    {
        DEVMODEW mode{};
        mode.dmSize = sizeof(mode);
        const std::wstring display = name.toStdWString();
        if(!EnumDisplaySettingsW(display.c_str(), ENUM_CURRENT_SETTINGS, &mode)) return false;
        rectangle = {mode.dmPosition.x, mode.dmPosition.y,
                     mode.dmPosition.x + LONG(mode.dmPelsWidth), mode.dmPosition.y + LONG(mode.dmPelsHeight)};
        const unsigned int w = analysis_width ? (std::min)(analysis_width, unsigned(mode.dmPelsWidth)) : mode.dmPelsWidth;
        const unsigned int h = (std::max)(1u, unsigned(mode.dmPelsHeight * double(w) / mode.dmPelsWidth));
        size = QSize(w, h);
        if(modern && InitDXGI(display)) return true;
        // Compatible fallback: reusable top-down DIB, no QPixmap or full-size CPU copy.
        duplication.Reset();
        context.Reset();
        device.Reset();
        desktop_dc = GetDC(nullptr);
        bitmap_dc = CreateCompatibleDC(desktop_dc);
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = w;
        info.bmiHeader.biHeight = -LONG(h);
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        bitmap = CreateDIBSection(desktop_dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
        if(!desktop_dc || !bitmap_dc || !bitmap || !pixels) return false;
        previous_bitmap = SelectObject(bitmap_dc, bitmap);
        SetStretchBltMode(bitmap_dc, COLORONCOLOR);
        qInfo() << "[Ambient] GDI fallback," << size;
        return true;
    }

    bool InitDXGI(const std::wstring& display)
    {
        ComPtr<IDXGIFactory1> factory;
        if(FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) return false;
        ComPtr<IDXGIAdapter1> adapter;
        ComPtr<IDXGIOutput> output;
        bool found = false;
        for(UINT a = 0; !found && factory->EnumAdapters1(a, &adapter) == S_OK; ++a)
        {
            for(UINT o = 0; adapter->EnumOutputs(o, &output) == S_OK; ++o)
            {
                DXGI_OUTPUT_DESC desc{};
                output->GetDesc(&desc);
                if(desc.AttachedToDesktop && display == desc.DeviceName)
                {
                    found = true;
                    rotation = desc.Rotation;
                    break;
                }
                output.Reset();
            }
            if(!found) adapter.Reset();
        }
        if(!found) return false;
        D3D_FEATURE_LEVEL level;
        if(FAILED(D3D11CreateDevice(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr,
                                   D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION,
                                   &device, &level, &context))) return false;
        ComPtr<IDXGIOutput1> output1;
        if(FAILED(output.As(&output1)) || FAILED(output1->DuplicateOutput(device.Get(), &duplication))) return false;

        // Four bilinear taps per analysis pixel; reduction happens entirely on the GPU.
        static const char shader[] = R"(
            Texture2D desktopImage : register(t0);
            SamplerState linearSampler : register(s0);
            cbuffer Params : register(b0) { float2 stepUV; float rotation; float padding; };
            struct V { float4 position : SV_Position; float2 uv : TEXCOORD0; };
            V VS(uint id : SV_VertexID) {
                V v; v.uv = float2((id << 1) & 2, id & 2);
                v.position = float4(v.uv * float2(2,-2) + float2(-1,1),0,1); return v;
            }
            float2 orient(float2 uv) {
                if(rotation == 2) return float2(uv.y, 1-uv.x);
                if(rotation == 3) return 1-uv;
                if(rotation == 4) return float2(1-uv.y, uv.x);
                return uv;
            }
            float4 PS(V v) : SV_Target {
                float2 d = stepUV * 0.25;
                return (desktopImage.Sample(linearSampler, orient(v.uv + float2(-d.x,-d.y))) +
                        desktopImage.Sample(linearSampler, orient(v.uv + float2( d.x,-d.y))) +
                        desktopImage.Sample(linearSampler, orient(v.uv + float2(-d.x, d.y))) +
                        desktopImage.Sample(linearSampler, orient(v.uv + float2( d.x, d.y)))) * 0.25;
            }
        )";
        ComPtr<ID3DBlob> vs, ps;
        if(FAILED(D3DCompile(shader, sizeof(shader), nullptr, nullptr, nullptr, "VS", "vs_4_0",
                            D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &vs, nullptr)) ||
           FAILED(D3DCompile(shader, sizeof(shader), nullptr, nullptr, nullptr, "PS", "ps_4_0",
                            D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &ps, nullptr)) ||
           FAILED(device->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr, &vertex_shader)) ||
           FAILED(device->CreatePixelShader(ps->GetBufferPointer(), ps->GetBufferSize(), nullptr, &pixel_shader))) return false;
        D3D11_SAMPLER_DESC sd{};
        sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.MaxLOD = D3D11_FLOAT32_MAX;
        if(FAILED(device->CreateSamplerState(&sd, &sampler))) return false;
        const float params[] = {1.0f / size.width(), 1.0f / size.height(), float(rotation), 0};
        D3D11_BUFFER_DESC bd{};
        bd.ByteWidth = sizeof(params);
        bd.Usage = D3D11_USAGE_IMMUTABLE;
        bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        D3D11_SUBRESOURCE_DATA initial{};
        initial.pSysMem = params;
        if(FAILED(device->CreateBuffer(&bd, &initial, &constants))) return false;
        D3D11_TEXTURE2D_DESC td{};
        td.Width = size.width(); td.Height = size.height();
        td.MipLevels = td.ArraySize = 1;
        td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_RENDER_TARGET;
        if(FAILED(device->CreateTexture2D(&td, nullptr, &target)) ||
           FAILED(device->CreateRenderTargetView(target.Get(), nullptr, &target_view))) return false;
        td.Usage = D3D11_USAGE_STAGING;
        td.BindFlags = 0;
        td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        for(auto& buffer : staging)
            if(FAILED(device->CreateTexture2D(&td, nullptr, &buffer))) return false;
        qInfo() << "[Ambient] DXGI GPU downsampling," << size;
        return true;
    }

    HRESULT Grab(QImage& image)
    {
        if(!duplication)
        {
            if(!StretchBlt(bitmap_dc, 0, 0, size.width(), size.height(), desktop_dc,
                           rectangle.left, rectangle.top, rectangle.right - rectangle.left,
                           rectangle.bottom - rectangle.top, SRCCOPY)) return E_FAIL;
            GdiFlush();
            image = QImage(static_cast<uchar*>(pixels), size.width(), size.height(), QImage::Format_RGB32).copy();
            return S_OK;
        }
        // Try reading the previous GPU result without waiting for the GPU.
        const unsigned int read_index = (write_index + 1) % 2;
        // Read older then newer; publish only the newest ready frame and never wait.
        for(const auto slot : {write_index, read_index})
        {
            if(!pending[slot]) continue;
            D3D11_MAPPED_SUBRESOURCE mapped{};
            HRESULT hr = context->Map(staging[slot].Get(), 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &mapped);
            if(SUCCEEDED(hr))
            {
                image = QImage(static_cast<const uchar*>(mapped.pData), size.width(), size.height(),
                               mapped.RowPitch, QImage::Format_RGB32).copy();
                context->Unmap(staging[slot].Get(), 0);
                pending[slot] = false;
            }
            else if(hr != DXGI_ERROR_WAS_STILL_DRAWING) return hr;
            else ++readback_busy;
        }
        if(pending[write_index]) return S_FALSE;
        // Defensive cleanup after a failed capture; successful frames are released below.
        if(frame_held) { duplication->ReleaseFrame(); frame_held = false; }
        DXGI_OUTDUPL_FRAME_INFO info{};
        ComPtr<IDXGIResource> resource;
        // A bounded worker-only wait lets DXGI finish its fresh desktop copy. A zero
        // timeout after ReleaseFrame can halve the delivery rate on some drivers.
        HRESULT hr = duplication->AcquireNextFrame(2, &info, &resource);
        if(hr == DXGI_ERROR_WAIT_TIMEOUT) { ++timeouts; return S_FALSE; }
        if(FAILED(hr)) return hr;
        frame_held = true;
        ++acquired;
        ComPtr<ID3D11Texture2D> desktop;
        hr = resource.As(&desktop);
        if(SUCCEEDED(hr))
        {
            D3D11_TEXTURE2D_DESC td{};
            desktop->GetDesc(&td);
            if(!source)
            {
                td.Usage = D3D11_USAGE_DEFAULT; td.CPUAccessFlags = 0; td.MiscFlags = 0;
                td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                hr = device->CreateTexture2D(&td, nullptr, &source);
                if(SUCCEEDED(hr)) hr = device->CreateShaderResourceView(source.Get(), nullptr, &source_view);
            }
            if(SUCCEEDED(hr))
            {
                context->CopyResource(source.Get(), desktop.Get());
                ID3D11RenderTargetView* rt = target_view.Get();
                ID3D11ShaderResourceView* srv = source_view.Get();
                ID3D11SamplerState* ss = sampler.Get();
                ID3D11Buffer* cb = constants.Get();
                D3D11_VIEWPORT viewport{0, 0, float(size.width()), float(size.height()), 0, 1};
                context->OMSetRenderTargets(1, &rt, nullptr);
                context->RSSetViewports(1, &viewport);
                context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                context->VSSetShader(vertex_shader.Get(), nullptr, 0);
                context->PSSetShader(pixel_shader.Get(), nullptr, 0);
                context->PSSetShaderResources(0, 1, &srv);
                context->PSSetSamplers(0, 1, &ss);
                context->PSSetConstantBuffers(0, 1, &cb);
                context->Draw(3, 0);
                srv = nullptr;
                context->PSSetShaderResources(0, 1, &srv);
                context->CopyResource(staging[write_index].Get(), target.Get());
                context->Flush();
                pending[write_index] = true;
                write_index = read_index;
            }
        }
        // Release as soon as the GPU commands have been submitted, as in LightSync.
        // Holding the frame until the next tick reduced delivery to about 40 FPS
        // on the tested Windows 11/NVIDIA system; immediate release reached 58 FPS.
        duplication->ReleaseFrame();
        frame_held = false;
        return hr;
    }
};

WindowsScreenCapturer::WindowsScreenCapturer() = default;
WindowsScreenCapturer::~WindowsScreenCapturer() { Stop(); }
void WindowsScreenCapturer::SetScreen(int index)
{
    const auto screens = QGuiApplication::screens(); // GUI thread only.
    QString name;
    if(index >= 0 && index < screens.size())
    {
        HMONITOR monitor = nullptr;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        if(auto* native = screens[index]->nativeInterface<QNativeInterface::QWindowsScreen>()) monitor = native->handle();
#else
        const QPoint center = screens[index]->geometry().center();
        monitor = MonitorFromPoint(POINT{center.x(), center.y()}, MONITOR_DEFAULTTONEAREST);
#endif
        MONITORINFOEXW info{};
        info.cbSize = sizeof(info);
        if(monitor && GetMonitorInfoW(monitor, &info)) name = QString::fromWCharArray(info.szDevice);
    }
    std::lock_guard<std::mutex> guard(settings_mutex);
    display_name = name; // Native \\.\DISPLAYn, not the monitor's friendly/product name.
    ++generation;
    wake.notify_all();
}
void WindowsScreenCapturer::SetFrameRate(unsigned int value)
{
    ScreenCapturer::SetFrameRate(value);
    wake.notify_all();
}
void WindowsScreenCapturer::SetAnalysisWidth(unsigned int value)
{
    std::lock_guard<std::mutex> guard(settings_mutex);
    if(analysis_width != value) { analysis_width = value; ++generation; wake.notify_all(); }
}
void WindowsScreenCapturer::SetModernCapture(bool value)
{
    std::lock_guard<std::mutex> guard(settings_mutex);
    if(modern_capture != value) { modern_capture = value; ++generation; wake.notify_all(); }
}
QSize WindowsScreenCapturer::SourceSize() const
{
    std::lock_guard<std::mutex> guard(settings_mutex);
    return source_size;
}
void WindowsScreenCapturer::Start()
{
    std::lock_guard<std::mutex> guard(lifecycle_mutex);
    if(running.exchange(true)) return;
    capture_thread = std::thread(&WindowsScreenCapturer::CaptureThreadFunction, this);
}
void WindowsScreenCapturer::Stop()
{
    std::lock_guard<std::mutex> guard(lifecycle_mutex);
    running = false;
    wake.notify_all();
    if(capture_thread.joinable()) capture_thread.join();
}
void WindowsScreenCapturer::CaptureThreadFunction()
{
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
    std::unique_ptr<CaptureResources> resources;
    unsigned int active_generation = ~0u;
    auto retry_at = CaptureClock::time_point::min();
    auto deadline = CaptureClock::now();
    AmbientPerformanceMetrics metrics("capture");
    while(running)
    {
        QString name;
        unsigned int requested_width, current_generation;
        bool requested_modern;
        {
            std::lock_guard<std::mutex> guard(settings_mutex);
            name = display_name; requested_width = analysis_width;
            requested_modern = modern_capture; current_generation = generation;
        }
        if(active_generation != current_generation || (!resources && CaptureClock::now() >= retry_at))
        {
            resources.reset();
            auto candidate = std::make_unique<CaptureResources>();
            if(!name.isEmpty() && candidate->Init(name, requested_width, requested_modern))
            {
                std::lock_guard<std::mutex> guard(settings_mutex);
                source_size = QSize(candidate->rectangle.right - candidate->rectangle.left,
                                    candidate->rectangle.bottom - candidate->rectangle.top);
                resources = std::move(candidate);
            }
            retry_at = CaptureClock::now() + std::chrono::seconds(2);
            active_generation = current_generation;
        }
        if(resources)
        {
            const auto start = CaptureClock::now();
            QImage frame;
            const HRESULT hr = resources->Grab(frame);
            metrics.Add(start);
            if(FAILED(hr))
            {
                resources.reset();
                // Access loss (lock, hibernate, monitor change): retry rather than retaining a dead duplication.
                retry_at = CaptureClock::now() + std::chrono::milliseconds(500);
            }
            else if(!frame.isNull()) emit OnImage(frame);
        }
        const unsigned int active_rate = (std::max)(1u, framerate.load());
        const auto period = std::chrono::microseconds(1000000 / active_rate);
        deadline += period;
        if(deadline < CaptureClock::now()) deadline = CaptureClock::now() + period;
        std::unique_lock<std::mutex> guard(settings_mutex);
        wake.wait_until(guard, deadline, [&]{ return !running || generation != active_generation || framerate != active_rate; });
    }
    metrics.Report(); // One summary when stopped, never per-frame logging.
}
