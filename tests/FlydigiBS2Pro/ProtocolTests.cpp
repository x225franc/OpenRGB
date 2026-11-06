/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "../../Controllers/FlydigiController/FlydigiBS2ProModes.h"
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
 auto usb=HidReport(RgbCommand::GetSwitch,{},Transport::USB);
 auto bt=HidReport(RgbCommand::GetSwitch,{},Transport::Bluetooth);
 Check(usb.size()==32&&usb[0]==0&&bt.size()==25&&bt[0]==2,"Observed USB and Bluetooth output report layouts");
 uint8_t value=0;Bytes reply{3,90,165,69,3,1,73};
 Check(ParseReply(reply,Transport::USB,RgbCommand::GetSwitch,value)&&value==1,"Live USB RGB status reply validates opcode and checksum");
 reply[0]=1;Check(ParseReply(reply,Transport::Bluetooth,RgbCommand::GetSwitch,value),"Live Bluetooth status reply uses input report one");
 reply[6]^=1;Check(!ParseReply(reply,Transport::Bluetooth,RgbCommand::GetSwitch,value),"Corrupted reply cannot advance a transfer");
 reply[6]^=1;Check(!ParseReply(reply,Transport::Bluetooth,RgbCommand::Finalize,value),"Unrelated opcode cannot acknowledge finalization");
 std::vector<Bytes> colors(6,Bytes(30));colors[2][0]=255;
 auto table=Configuration(colors,0,0,30);
 Check(table.size()==186&&table[1]==2&&table[4]==11&&table[5]==30&&table[66]==255,"RGB-only table preserves six positions and LED-major frames");
 Check(PlanRgbUpload(table).size()==21,"Native RGB table uses nineteen acknowledged chunks");
 rejected=false;try{Configuration(colors,10,0,30);}catch(const std::invalid_argument&){rejected=true;}Check(rejected,"Out-of-bounds animation frames rejected");
 auto breathing=NativeTable(Breath,{{0,255,0}},30,5);
 Check(breathing[3]==1&&breathing[4]==6&&breathing[6]==0&&breathing[10]==255,"Breath uses two native frames with a black and a green endpoint");
 auto flow=NativeTable(Flow,{{0,255,0}},30,5);
 Check(flow[3]==5&&flow[7]!=flow[37]&&flow[7]!=flow[10],"Flow is a spatial and temporal firmware table");
 auto multi=NativeTable(MultiStatic,{{255,0,0},{0,255,0},{0,0,255}},30,5);
 Check(multi[6]==255&&multi[37]==255&&multi[68]==255,"Multi Static assigns separate spatial RGB positions");
 return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
