/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "ModernWindowsAccent.h"
#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#include <roapi.h>
#include <winrt/Windows.UI.ViewManagement.h>
namespace
{
struct WindowsApartment
{
    HRESULT result = RoInitialize(RO_INIT_SINGLETHREADED);
    WindowsApartment()
    {
        if(FAILED(result) && result != RPC_E_CHANGED_MODE)
            winrt::check_hresult(result);
    }
    ~WindowsApartment()
    {
        if(SUCCEEDED(result))
            RoUninitialize();
    }
};
}
struct ModernWindowsAccentSubscription::State
{
    WindowsApartment apartment;
    winrt::Windows::UI::ViewManagement::UISettings settings { nullptr };
    winrt::event_token token { };
    explicit State(HWND window)
    {
        settings = winrt::Windows::UI::ViewManagement::UISettings();
        // The event can run off-thread. Post only a native message, never access Qt here.
        token = settings.ColorValuesChanged(
            [window](auto const&, auto const&) { PostMessageW(window, ModernAccentChangedMessage, 0, 0); });
    }
    ~State()
    {
        try
        {
            if(settings)
                settings.ColorValuesChanged(token);
        }
        catch(...)
        {
        }
    }
};
QColor modernWindowsAccent()
{
    try
    {
        WindowsApartment apartment;
        winrt::Windows::UI::ViewManagement::UISettings settings;
        // Light2 is the Windows accent variant used by controls on dark surfaces.
        const auto color
            = settings.GetColorValue(winrt::Windows::UI::ViewManagement::UIColorType::AccentLight2);
        return QColor(color.R, color.G, color.B);
    }
    catch(...)
    {
        DWORD color = 0;
        BOOL opaque = FALSE;
        if(SUCCEEDED(DwmGetColorizationColor(&color, &opaque)))
            return QColor((color >> 16) & 255, (color >> 8) & 255, color & 255);
        return QColor("#ed96cc");
    }
}
ModernWindowsAccentSubscription::ModernWindowsAccentSubscription(quintptr window)
    : state(std::make_unique<State>(reinterpret_cast<HWND>(window)))
{
}
#else
struct ModernWindowsAccentSubscription::State
{
};
QColor modernWindowsAccent()
{
    return QColor("#ed96cc");
}
ModernWindowsAccentSubscription::ModernWindowsAccentSubscription(quintptr) { }
#endif
ModernWindowsAccentSubscription::~ModernWindowsAccentSubscription() = default;
