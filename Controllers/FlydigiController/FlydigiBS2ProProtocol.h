/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <vector>

// RGB-only BS2 Pro protocol. USB and Bluetooth HID framing validated on 2026-11-06.
// Configuration finalization persists to flash; never use this path as a Direct stream.
// The enum intentionally cannot construct fan, thermal, firmware or reset commands.
namespace FlydigiBS2Pro
{
using Bytes=std::vector<uint8_t>;
enum class RgbCommand : uint8_t
{
    Prepare=0x41, Finalize=0x43, GetSwitch=0x45, SetSwitch=0x46, Chunk=0x47
};
inline Bytes Encode(RgbCommand command,const Bytes& payload={})
{
    size_t size=24;
    switch(command)
    {
    case RgbCommand::GetSwitch:
        size=32;if(!payload.empty())throw std::invalid_argument("RGB query payload must be empty");break;
    case RgbCommand::SetSwitch:
        size=32;if(payload.size()!=1||payload[0]>1)throw std::invalid_argument("RGB switch must be zero or one");break;
    case RgbCommand::Prepare:
    case RgbCommand::Finalize:
        if(!payload.empty())throw std::invalid_argument("RGB transfer control payload must be empty");break;
    case RgbCommand::Chunk:
        if(payload.size()<2||payload.size()>11)throw std::invalid_argument("RGB chunk needs an index and one to ten bytes");break;
    default:throw std::invalid_argument("Only observed BS2 RGB opcodes are allowed");
    }
    Bytes result(size,0);
    result[0]=0x5A;result[1]=0xA5;result[2]=static_cast<uint8_t>(command);
    result[3]=static_cast<uint8_t>(payload.size()+2);
    std::copy(payload.begin(),payload.end(),result.begin()+4);
    uint8_t checksum=0;
    for(size_t i=2;i<4+payload.size();++i)checksum+=result[i];
    result[4+payload.size()]=checksum;
    return result;
}
struct PlannedRequest
{
    RgbCommand command;
    Bytes frame;
    // The executor validates the matching opcode, checksum and success byte before advancing.
    bool requires_verified_success_response=true;
};
inline std::vector<PlannedRequest> PlanRgbUpload(const Bytes& rgb_configuration)
{
    // RGB-only native table, never a global fan/system configuration.
    if(rgb_configuration.empty()||rgb_configuration.size()>2560)
        throw std::invalid_argument("RGB configuration must fit at most 256 indexed ten-byte chunks");
    std::vector<PlannedRequest> plan;
    plan.push_back({RgbCommand::Prepare,Encode(RgbCommand::Prepare),true});
    for(size_t offset=0,index=0;offset<rgb_configuration.size();offset+=10,++index)
    {
        const size_t length=std::min<size_t>(10,rgb_configuration.size()-offset);
        Bytes payload{static_cast<uint8_t>(index)};
        payload.insert(payload.end(),rgb_configuration.begin()+offset,rgb_configuration.begin()+offset+length);
        plan.push_back({RgbCommand::Chunk,Encode(RgbCommand::Chunk,payload),true});
    }
    plan.push_back({RgbCommand::Finalize,Encode(RgbCommand::Finalize),true});
    return plan;
}
enum class Transport { USB, Bluetooth };
// Verified HID descriptors: USB has a zero report prefix and 31 payload bytes;
// Bluetooth uses output report 2 with 24 payload bytes.
inline Bytes HidReport(RgbCommand command,const Bytes& payload,Transport transport)
{
    const auto inner=Encode(command,payload);
    Bytes report(transport==Transport::USB?32:25,0);
    report[0]=transport==Transport::USB?0:2;
    std::copy_n(inner.begin(),std::min(inner.size(),report.size()-1),report.begin()+1);
    return report;
}
inline bool ParseReply(const Bytes& report,Transport transport,RgbCommand command,uint8_t& value)
{
    const uint8_t prefix=transport==Transport::USB?3:1;
    if(report.size()<7||report[0]!=prefix||report[1]!=0x5A||report[2]!=0xA5
       ||report[3]!=static_cast<uint8_t>(command)||report[4]!=3)return false;
    if(uint8_t(report[3]+report[4]+report[5])!=report[6])return false;
    value=report[5];return value<=1;
}
// RGB-only native animation table: six spatial positions, ten frames per position.
// Layout is LED-major, never the controller-wide fan/system configuration.
inline Bytes Configuration(const std::vector<Bytes>& colors,uint8_t last_frame,uint8_t speed,uint8_t brightness)
{
    if(colors.size()!=6||last_frame>9||speed>10||brightness>100)
        throw std::invalid_argument("Invalid BS2 RGB table");
    Bytes result{0,2,0,last_frame,uint8_t(11-speed),brightness};
    for(const auto& led:colors)
    {
        if(led.size()!=30)throw std::invalid_argument("Each BS2 RGB position needs ten RGB frames");
        result.insert(result.end(),led.begin(),led.end());
    }
    return result;
}

}
