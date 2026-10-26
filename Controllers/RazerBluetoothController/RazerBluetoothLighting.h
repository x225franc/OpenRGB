/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <array>
#include <cstdint>
#include "RGBControllerInterface.h"
namespace RazerBLELighting
{
enum Mode {Static=0,Off=1,Wave=2,Breathing=3,Spectrum=4,Reactive=5,Starlight=6};
// Captured from BlackWidow V3 Mini Bluetooth Fn+Ctrl presets, 2026-10-26.
inline std::array<uint8_t,11> KeyboardState(int mode,RGBColor color,uint8_t brightness)
{
    std::array<uint8_t,11> state{1,0,0,1,uint8_t(RGBGetRValue(color)),uint8_t(RGBGetGValue(color)),uint8_t(RGBGetBValue(color)),0,0,0,brightness};
    switch(mode)
    {
        case Off:state={1,0,0,1,0,0,0,0,0,0,0};break;
        case Wave:state={4,1,40,1,0,255,0,0,0,0,brightness};break;
        case Breathing:state[0]=2;break;
        case Spectrum:state={3,0,0,0,0,0,0,0,0,0,brightness};break;
        case Reactive:state[0]=5;state[2]=3;break;
        case Starlight:state[0]=7;state[2]=2;break;
    }
    return state;
}
inline int ModeFromState(uint8_t effect,uint8_t brightness)
{
    if(!brightness)return Off;
    switch(effect){case 2:return Breathing;case 3:return Spectrum;case 4:return Wave;case 5:return Reactive;case 7:return Starlight;default:return Static;}
}
}
