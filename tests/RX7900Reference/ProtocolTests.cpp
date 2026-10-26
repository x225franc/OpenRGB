/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "../../Controllers/AMDRadeonReferenceController/RadeonReferenceProtocol.h"
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>

void Require(bool value, const char* message)
{
    if(!value) throw std::runtime_error(message);
}

int main(int argc, char** argv)
{
    try
    {
        Require(argc == 3, "Pass static and firmware captured fixture paths");
        std::map<std::string, RadeonReference::Message> fixtures;
        for(int file = 1; file < argc; ++file)
        {
        std::ifstream input(argv[file]);
        Require(input.good(), "Cannot open captured fixtures");
        std::string line;
        while(std::getline(input, line))
        {
            std::istringstream row(line);
            std::string name;
            row >> name;
            RadeonReference::Message message{};
            for(auto& byte : message)
            {
                unsigned int value;
                Require(static_cast<bool>(row >> std::hex >> value) && value <= 255, "Invalid captured fixture");
                byte = static_cast<uint8_t>(value);
            }
            fixtures[name] = message;
        }
        }
        Require(fixtures.at("red") == RadeonReference::Static(255, 0, 0, 5), "Red differs from official capture");
        Require(fixtures.at("green") == RadeonReference::Static(0, 255, 0, 5), "Green differs from official capture");
        Require(fixtures.at("blue") == RadeonReference::Static(0, 0, 255, 5), "Blue differs from official capture");
        Require(fixtures.at("red_low") == RadeonReference::Static(255, 0, 0, 1), "Low brightness differs from capture");
        Require(fixtures.at("on") == RadeonReference::Mode(false), "On mode differs from capture");
        Require(fixtures.at("off") == RadeonReference::Mode(true), "Off mode differs from capture");
        Require(fixtures.at("apply") == RadeonReference::Apply(), "Apply differs from capture");
        Require(fixtures.at("enable") == RadeonReference::Enable(), "Enable differs from capture");
        Require(fixtures.at("select") == RadeonReference::SelectControl(), "Control selection differs from capture");
        Require(fixtures.at("identify") == RadeonReference::Identify(), "Identify differs from startup capture");
        Require(RadeonReference::Static(1, 2, 3, 0)[9] == 0x33, "Brightness lower bound");
        Require(RadeonReference::Static(1, 2, 3, 999)[9] == 0xFF, "Brightness upper bound");

        unsigned int firmware_cases = 0;
        for(const auto& fixture : fixtures)
        {
            std::string name = fixture.first;
            if(name.rfind("fw_", 0) != 0) continue;
            if(name == "fw_select_cycle") Require(fixture.second == RadeonReference::EffectSelection(true), "Cycle selection differs from capture");
            else if(name == "fw_select_breathing") Require(fixture.second == RadeonReference::EffectSelection(false), "Breathing selection differs from capture");
            else
            {
                std::replace(name.begin(), name.end(), '_', ' ');
                std::istringstream values(name);
                std::string prefix, effect;values >> prefix >> effect;
                unsigned int red=0, green=0, blue=0, brightness=0, speed=0, random=0;
                if(effect == "breathing")
                {
                    Require(bool(values >> red >> green >> blue >> brightness >> speed >> random), "Bad breathing fixture name");
                    Require(fixture.second == RadeonReference::Breathing(uint8_t(red), uint8_t(green), uint8_t(blue), brightness, speed, random != 0), "Native breathing differs from full 64-byte capture");
                }
                else
                {
                    Require(effect == "cycle" && bool(values >> brightness >> speed), "Bad cycle fixture name");
                    Require(fixture.second == RadeonReference::ColorCycle(brightness, speed), "Native color cycle differs from full 64-byte capture");
                }
            }
            ++firmware_cases;
        }
        Require(firmware_cases >= 12, "Missing native firmware fixtures");
        Require(RadeonReference::ColorCycle(999,999)[9] == 0x7F, "Cycle brightness must not reach FF");
        Require(RadeonReference::Breathing(255,0,0,0,0,false)[5] == 0x26, "Breathing clamps to captured low speed");
        std::cout << "PASS " << firmware_cases << " native firmware messages match complete captured packets" << std::endl;

        // Each failure must stop immediately, including invalid reply markers
        // and payloads; a partial write must never be retried automatically.
        for(int failure = -1; failure < 6; ++failure)
        {
            unsigned int calls = 0;
            unsigned int delays = 0;
            auto transfer = [&](bool write, uint8_t* data, unsigned int size)
            {
                const unsigned int call = calls++;
                const unsigned int frame = call % 3;
                const unsigned int sizes[] = {24, 24, 19};
                const unsigned int offsets[] = {0, 23, 46};
                const uint8_t headers[] = {0x40, 0x57, 0xAE, 0xFF, 0xE8, 0xD1};
                Require(write == (call < 3) && size == sizes[frame], "Frame order or size");
                if(static_cast<int>(call) == failure) return false;
                if(write)
                {
                    Require(data[0] == headers[call], "Write frame marker");
                    Require(std::equal(data + 1, data + size, fixtures.at("red").begin() + offsets[frame]), "Write frame content");
                }
                else
                {
                    Require(data[0] == (call == 3 ? 0xAE : headers[call - 1]), "Read buffer continuity differs from official API use");
                    data[0] = headers[call];
                    std::copy_n(fixtures.at("red").begin() + offsets[frame], size - 1, data + 1);
                }
                return true;
            };
            const bool ok = RadeonReference::Exchange(fixtures.at("red"), transfer, [&] { ++delays; });
            Require(ok == (failure == -1), "Transport failure not propagated");
            Require(calls == (failure == -1 ? 6u : static_cast<unsigned int>(failure + 1)), "Transfer continued after failure");
            Require(delays == (failure >= 0 && failure < 3 ? 0u : 1u), "Read turnaround timing");
        }
        for(bool valid_identity : {false, true})
        {
            unsigned int call = 0;
            const bool ok = RadeonReference::Exchange(RadeonReference::Identify(), [&](bool write, uint8_t* data, unsigned int size)
            {
                const unsigned int frame = call++ % 3;
                if(!write)
                {
                    std::fill_n(data, size, 0);
                    const uint8_t markers[] = {0xFF, 0xE8, 0xD1};
                    data[0] = markers[frame];
                    if(frame == 0)
                    {
                        data[1] = valid_identity ? 0x12 : 0xFF;
                        data[2] = 0x02;
                        data[3] = 0x42; // Device identity data is not an echo.
                    }
                }
                return true;
            }, [] {}, false);
            Require(ok == valid_identity, "Invalid identity response accepted or valid payload rejected");
        }
        for(unsigned int corrupt = 0; corrupt < 6; ++corrupt)
        {
            unsigned int calls = 0;
            const bool ok = RadeonReference::Exchange(fixtures.at("red"), [&](bool write, uint8_t* data, unsigned int size)
            {
                const unsigned int call = calls++;
                if(!write)
                {
                    const unsigned int frame = call - 3;
                    const unsigned int offsets[] = {0, 23, 46};
                    const uint8_t headers[] = {0xFF, 0xE8, 0xD1};
                    data[0] = headers[frame];
                    std::copy_n(fixtures.at("red").begin() + offsets[frame], size - 1, data + 1);
                    if(frame == corrupt / 2) data[corrupt % 2] ^= 1;
                }
                return true;
            }, [] {});
            Require(!ok && calls == 4 + corrupt / 2, "Corrupt acknowledgement accepted or reads continued");
        }
        std::cout << "PASS: captured colors, brightness, off/on, apply, framing, all transfer failures and corrupt acknowledgements\n";
        return 0;
    }
    catch(const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
