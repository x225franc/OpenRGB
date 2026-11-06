/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "FlydigiBS2ProProtocol.h"
#include <array>
namespace FlydigiBS2Pro
{
using Color=std::array<uint8_t,3>;
// Values match the observed manufacturer modes, not fan or controller profiles.
enum Effect { Off=0, Flow=1, Rotate=2, Breath=3, SingleStatic=4, MultiStatic=5 };
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
