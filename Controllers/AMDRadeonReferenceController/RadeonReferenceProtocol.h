/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <algorithm>
#include <array>
#include <cstdint>

// RX 7900 XTX reference, captured on PCI 1002:744C / 1002:0E3B.
// No USB assumptions, firmware commands, or persistent-save command (50 55).
namespace RadeonReference
{
using Message = std::array<uint8_t, 64>;

inline Message Static(uint8_t red, uint8_t green, uint8_t blue, unsigned int level)
{
    Message message;
    message.fill(0xFF);
    const uint8_t prefix[] = {0x51, 0x2C, 0x01, 0x00, 0x00, 0xFF, 0x00, 0xFF, 0xFF};
    std::copy(prefix, prefix + sizeof(prefix), message.begin());
    message[9] = static_cast<uint8_t>(std::max(1u, std::min(level, 5u)) * 0x33);
    message[10] = red;
    message[11] = green;
    message[12] = blue;
    message[13] = message[14] = message[15] = 0;
    return message;
}

inline Message Mode(bool off)
{
    Message message{};
    const uint8_t prefix[] = {0x51, 0xA0, 0x01, 0x00, 0x00, 0x03, 0x00, 0x00, 0x05, 0x06};
    std::copy(prefix, prefix + sizeof(prefix), message.begin());
    std::fill(message.begin() + 10, message.begin() + 25, off ? 0xFE : 0x00);
    return message;
}

inline Message Apply()
{
    Message message{};
    message[0] = 0x51;
    message[1] = 0x28;
    message[4] = 0xE0;
    return message;
}

inline Message Enable()
{
    Message message{};
    message[0] = 0x41;
    message[1] = 0x80;
    return message;
}

inline Message Identify()
{
    Message message{};
    message[0] = 0x12;
    message[1] = 0x02;
    return message;
}

inline Message SelectControl()
{
    Message message{};
    message[0] = 0x51;
    message[1] = 0x96;
    return message;
}

// transfer(write, buffer, size) is injected so framing and failures can be tested
// without a GPU. Stop at the first transport error or invalid acknowledgement.
template<typename Transfer, typename Delay>
bool Exchange(const Message& message, Transfer transfer, Delay delay, bool echo = true)
{
    const unsigned int sizes[] = {24, 24, 19};
    const uint8_t write_headers[] = {0x40, 0x57, 0xAE};
    const uint8_t read_headers[] = {0xFF, 0xE8, 0xD1};
    unsigned int offset = 0;
    std::array<uint8_t, 24> data{};
    for(unsigned int frame = 0; frame < 3; ++frame)
    {
        data.fill(0);
        data[0] = write_headers[frame];
        std::copy_n(message.begin() + offset, sizes[frame] - 1, data.begin() + 1);
        if(!transfer(true, data.data(), sizes[frame])) return false;
        offset += sizes[frame] - 1;
    }
    delay();
    offset = 0;
    for(unsigned int frame = 0; frame < 3; ++frame)
    {
        if(!transfer(false, data.data(), sizes[frame]) || data[0] != read_headers[frame]
           || (echo && !std::equal(data.begin() + 1, data.begin() + sizes[frame], message.begin() + offset))
           || (!echo && frame == 0 && (data[1] != 0x12 || data[2] != 0x02)))
            return false;
        offset += sizes[frame] - 1;
    }
    return true;
}
}
