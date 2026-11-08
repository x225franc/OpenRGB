/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "FlydigiBS2ProProtocol.h"
#include <algorithm>
#include <array>
#include <cmath>
namespace FlydigiBS2Pro
{
using Color=std::array<uint8_t,3>;
// Values match the observed manufacturer modes, not fan or controller profiles.
enum Effect { Off=0, Flow=1, Rotate=2, Breath=3, SingleStatic=4, MultiStatic=5, PresetSync=6 };
// Whole-cooler approximation. Values 1/2/5 are animated green/yellow/blue
// on the tested device; 4 is fixed red. Zero means HOLD, never hardware Off.
// Direct speed controls the stable-color follow delay, not the fixed firmware animation.
inline unsigned PresetIntervalMs(unsigned speed)
{
    return 100u+(10u-std::clamp(speed,1u,10u))*200u;
}
inline uint8_t PresetForColor(const Color& c,uint8_t previous=0)
{
    const int r=c[0],g=c[1],b=c[2],mx=std::max({r,g,b}),mn=std::min({r,g,b});
    if(mx<24||(mx-mn)*100<mx*20)return 0;
    const double d=mx-mn;
    double hue=mx==r?60.0*((g-b)/d):mx==g?60.0*(2+(b-r)/d):60.0*(4+(r-g)/d);
    if(hue<0)hue+=360;
    const auto distance=[hue](double center){const double delta=std::abs(hue-center);return std::min(delta,360.0-delta);};
    struct Choice{uint8_t preset;double center;};
    constexpr std::array<Choice,4> choices{{{4,0},{2,60},{1,120},{5,240}}};
    uint8_t best=4;double nearest=361,old_distance=361;
    for(const auto& choice:choices)
    {
        const double value=distance(choice.center);
        if(value<nearest){nearest=value;best=choice.preset;}
        if(choice.preset==previous)old_distance=value;
    }
    // Small hue movements around a boundary should not flip the native effect.
    return old_distance<=nearest+12?previous:best;
}
inline Bytes NativeTable(unsigned effect,const std::vector<Color>& palette,uint8_t brightness,uint8_t speed)
{
    if(effect>MultiStatic||palette.empty()||palette.size()>6)
        throw std::invalid_argument("Invalid BS2 native RGB mode/palette");
    const Color black{0,0,0};
    const std::array<Color,6> rainbow{{{0,0,255},{0,127,127},{0,255,0},{127,127,0},{255,0,0},{127,0,127}}};
    uint8_t last=effect==Breath?uint8_t(std::min<size_t>(5,palette.size())*2-1)
                  :effect==Flow||effect==Rotate?5:0;
    std::vector<Bytes> leds(6,Bytes(30));
    for(unsigned led=0;led<6;++led)
        for(unsigned frame=0;frame<10;++frame)
        {
            Color color=black;
            if(effect==SingleStatic)color=palette[0];
            else if(effect==MultiStatic&&frame==0)color=palette[led%palette.size()];
            else if(effect==Breath&&frame<=last&&frame%2)color=palette[(frame/2)%palette.size()];
            else if(effect==Flow&&frame<6)color=rainbow[(frame+led)%6];
            else if(effect==Rotate&&frame<6)
                for(unsigned i=0;i<palette.size();++i)
                    if((6+i-led)%6==frame){color=palette[i];break;}
            std::copy(color.begin(),color.end(),leds[led].begin()+frame*3);
        }
    return Configuration(leds,last,last?std::clamp<uint8_t>(speed,1,10):0,brightness);
}
}
