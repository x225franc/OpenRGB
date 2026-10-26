/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "../../Controllers/RazerBluetoothController/RazerBluetoothLighting.h"
#include "../../Controllers/AMDRadeonReferenceController/RadeonReferenceProfile.h"
#include <iostream>
#include <stdexcept>
void check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);std::cout<<"PASS "<<text<<std::endl;}
int main(){try{
 using namespace RazerBLELighting;const auto green=ToRGBColor(0,255,0);
 check(KeyboardState(Breathing,green,10)==std::array<uint8_t,11>{2,0,0,1,0,255,0,0,0,0,10},"Breathing matches captured native keyboard state");
 check(KeyboardState(Reactive,green,10)==std::array<uint8_t,11>{5,0,3,1,0,255,0,0,0,0,10},"Reactive matches captured native keyboard state");
 check(KeyboardState(Starlight,green,10)==std::array<uint8_t,11>{7,0,2,1,0,255,0,0,0,0,10},"Starlight matches captured native keyboard state");
 check(KeyboardState(Spectrum,green,10)==std::array<uint8_t,11>{3,0,0,0,0,0,0,0,0,0,10},"Spectrum matches native keyboard readback");
 check(KeyboardState(Wave,green,10)==std::array<uint8_t,11>{4,1,40,1,0,255,0,0,0,0,10},"Previously validated Wave packet retained");
 check(KeyboardState(Off,green,255)[10]==0&&ModeFromState(7,0)==Off,"Off remains zero brightness");
 check(ModeFromState(2,10)==Breathing&&ModeFromState(3,10)==Spectrum&&ModeFromState(5,10)==Reactive&&ModeFromState(7,10)==Starlight,"Hardware effect readback selects the correct UI mode");
 std::vector<mode> saved(7),current(5);
 for(auto* list:{&saved,&current}){(*list)[0].name="Static";}
 saved[5].name=current[3].name="Breathing (hardware)";
 saved[5].flags=current[3].flags=MODE_FLAG_HAS_MODE_SPECIFIC_COLOR|MODE_FLAG_HAS_BRIGHTNESS|MODE_FLAG_HAS_SPEED;
 saved[5].speed_min=1;saved[5].speed_max=3;saved[5].speed=1;saved[5].brightness=2;saved[5].color_mode=MODE_COLORS_MODE_SPECIFIC;saved[5].colors={green};
 current[3].speed_min=3;current[3].speed_max=1;current[3].brightness_min=1;current[3].brightness_max=3;current[3].colors_min=current[3].colors_max=1;
 check(RadeonReference::RestoreModes(current,saved,5)==3&&current[3].colors[0]==green&&current[3].brightness==2,"GPU profile restores native mode by name after removal of software entries");
 check(current[3].speed_min==3&&current[3].speed_max==1&&current[3].speed==1,"GPU profile preserves new reversed speed metadata and saved wire preset");
 saved[3].name="Breathing (software)";
 check(RadeonReference::RestoreModes(current,saved,3)==0,"Removed software selection falls back to Static instead of another indexed mode");
 return 0;
}catch(std::exception const& e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}}
