/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "../../Controllers/FlydigiController/FlydigiApex5Protocol.h"
#include <iostream>
#include <stdexcept>
using namespace FlydigiApex5;
void Check(bool value,const char* text){if(!value)throw std::runtime_error(text);std::cout<<"PASS "<<text<<'\n';}
int main(){try{
 auto mode=Packet(0xA8,{0,1,2,20});
 Check(mode[0]==3&&mode[3]==0xA8&&mode[4]==6&&mode[9]==0xC5,"Captured direct-transfer header and checksum match");
 Bytes blue;for(unsigned int i=0;i<12;++i)blue.insert(blue.end(),{0,0,255});
 auto reports=Transfer(1,blue);
 Check(reports.size()==3&&reports[1][5]==0&&reports[2][5]==1,"Twelve RGB values span two twenty-byte pages");
 Check(reports[1][26]==0xBA&&reports[2][26]==0xBB,"Blue packet checksums match Space Station captures");
 Bytes restored;for(size_t i=1;i<reports.size();++i)restored.insert(restored.end(),reports[i].begin()+6,reports[i].begin()+26);
 Check(std::equal(blue.begin(),blue.end(),restored.begin()),"Page boundaries preserve split RGB components");
 Check(std::all_of(restored.begin()+36,restored.end(),[](uint8_t b){return b==0;}),"Unused final-page bytes are zero padded");
 auto meta=Metadata(5,30,1,0);auto encoded=Transfer(0,meta);
 Check(meta.size()==20&&meta[6]==30&&meta[7]==12&&meta[8]==5&&encoded.size()==2,"Metadata preserves twelve LEDs, percent brightness and native Static");
 auto off=Metadata(6,0,1,0);Check(off[6]==0&&off[8]==6,"Close maps to native Off");
 bool rejected=false;try{Packet(0xA6,{0,0});}catch(const std::invalid_argument&){rejected=true;}
 Check(rejected,"Profile/save command cannot be constructed through lighting API");
 rejected=false;try{Packet(0xA9,Bytes(22));}catch(const std::invalid_argument&){rejected=true;}
 Check(rejected,"Oversized HID payload rejected before any write");
 return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
