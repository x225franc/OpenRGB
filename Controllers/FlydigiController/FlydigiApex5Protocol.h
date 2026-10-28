/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace FlydigiApex5
{
using Bytes = std::vector<uint8_t>;
using Report = std::array<uint8_t, 32>;
constexpr unsigned int LedCount = 12;
constexpr unsigned int ChunkSize = 20;

// Observed Space Station output reports on 37D7:2501, interface 2, FFA0:0001.
// There is deliberately no API for A6/save, input mappings, triggers or profiles.
inline Report Packet(uint8_t command, const Bytes& payload)
{
    if((command != 0xA8 && command != 0xA9) || payload.size() > 21)
        throw std::invalid_argument("Not a supported Flydigi lighting packet");
    Report result{};
    result[0]=3;result[1]=0x5A;result[2]=0xA5;result[3]=command;
    result[4]=static_cast<uint8_t>(payload.size()+2);
    std::copy(payload.begin(),payload.end(),result.begin()+5);
    uint8_t checksum=0;
    for(size_t i=3;i<5+payload.size();++i) checksum+=result[i];
    result[5+payload.size()]=checksum;
    return result;
}
inline std::vector<Report> Transfer(uint16_t start, const Bytes& data)
{
    const size_t count=(data.size()+ChunkSize-1)/ChunkSize;
    if(data.empty() || count>255) throw std::invalid_argument("Invalid lighting transfer size");
    std::vector<Report> result;
    result.push_back(Packet(0xA8,{uint8_t(start>>8),uint8_t(start),uint8_t(count),ChunkSize}));
    for(size_t page=0;page<count;++page)
    {
        Bytes payload(ChunkSize+1,0);payload[0]=static_cast<uint8_t>(page);
        const size_t offset=page*ChunkSize;
        std::copy_n(data.begin()+offset,std::min<size_t>(ChunkSize,data.size()-offset),payload.begin()+1);
        result.push_back(Packet(0xA9,payload));
    }
    return result;
}
inline Bytes Metadata(uint8_t effect, uint8_t brightness, uint8_t period, uint8_t last_frame)
{
    Bytes data{0,3,0,0,last_frame,period,uint8_t(std::min<unsigned int>(100,brightness)),LedCount,effect,0,
               255,255,255,255,255,255,255,255,255,255};
    return data;
}
}
