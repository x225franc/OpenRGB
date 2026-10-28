/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <vector>

// INACTIVE PREPARATION: derived from local Space Station source, not hardware captures.
// No HID/GATT I/O, registration, fan commands, RGB schema or ACK parser is implemented.
// This is the inner manufacturer frame. BLE wrapping/UUIDs remain unverified.
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
    // A future executor must validate a real, matching success reply BEFORE advancing.
    // Static source checks response[4]==1, but framing/CRC/opcode must be confirmed on hardware.
    bool requires_verified_success_response=true;
};
inline std::vector<PlannedRequest> PlanRgbUpload(const Bytes& rgb_configuration)
{
    // The meaning of configuration bytes is intentionally left to future hardware work.
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
}
