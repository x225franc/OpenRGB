/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "../../Controllers/FlydigiController/FlydigiBS2ProProtocol.h"
#include <iostream>
using namespace FlydigiBS2Pro;
void Check(bool value,const char* text){if(!value)throw std::runtime_error(text);std::cout<<"PASS "<<text<<'\n';}
int main(){try{
 auto get=Encode(RgbCommand::GetSwitch);Check(get.size()==32&&get[2]==0x45&&get[3]==2&&get[4]==0x47,"Static-source RGB query format");
 auto on=Encode(RgbCommand::SetSwitch,{1});auto off=Encode(RgbCommand::SetSwitch,{0});
 Check(on.size()==32&&on[5]==0x4A&&off[5]==0x49,"Static-source RGB switch checksum");
 Bytes config;for(uint8_t i=1;i<=21;++i)config.push_back(i);
 auto plan=PlanRgbUpload(config);Check(plan.size()==5,"Preparation plus three chunks plus finalization");
 Check(plan.front().frame.size()==24&&plan.front().frame[4]==0x43&&plan.back().frame[4]==0x45,"Static-source transfer control format");
 Check(plan[1].frame[4]==0&&plan[2].frame[4]==1&&plan[3].frame[4]==2,"Chunk sequence never loses its index");
 Check(plan[1].frame[3]==13&&plan[1].frame[15]==0x8B&&plan[3].frame[3]==4&&plan[3].frame[5]==21,"Ten-byte chunk boundary and final partial chunk");
 Check(std::all_of(plan.begin(),plan.end(),[](const auto& v){return v.requires_verified_success_response;}),"Every step requires a verified device response; this is not an executor");
 bool rejected=false;try{Encode(static_cast<RgbCommand>(0x20));}catch(const std::invalid_argument&){rejected=true;}Check(rejected,"Non-RGB opcode rejected");
 rejected=false;try{Encode(RgbCommand::SetSwitch,{2});}catch(const std::invalid_argument&){rejected=true;}Check(rejected,"Invalid RGB switch value rejected");
 rejected=false;try{PlanRgbUpload(Bytes(2561));}catch(const std::invalid_argument&){rejected=true;}Check(rejected,"Oversized plan cannot wrap sequence counter");
 rejected=false;try{PlanRgbUpload({});}catch(const std::invalid_argument&){rejected=true;}Check(rejected,"Empty configuration is not submitted accidentally");
 return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
