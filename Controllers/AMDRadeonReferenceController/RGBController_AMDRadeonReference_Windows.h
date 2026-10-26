/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "RGBController.h"
#include "RadeonReferenceProtocol.h"
#include "SoftwareLightingEffect.h"
#include <memory>
#include <atomic>
#include <mutex>
#include <chrono>

class RadeonReferenceADL;

class RGBController_AMDRadeonReference : public RGBController
{
public:
    RGBController_AMDRadeonReference(std::shared_ptr<RadeonReferenceADL> adl, int adapter, const std::string& pci_location);
    ~RGBController_AMDRadeonReference() override;
    void SetupZones();
    void DeviceUpdateLEDs() override;
    void DeviceUpdateZoneLEDs(int) override;
    void DeviceUpdateSingleLED(int) override;
    void DeviceUpdateMode() override;

private:
    void Update();
    void ApplyState(RGBColor color,bool off,unsigned brightness);
    std::unique_ptr<SoftwareLighting::Worker> softwareEffect;
    std::atomic<unsigned> software_brightness{5};
    std::shared_ptr<RadeonReferenceADL> adl;
    int adapter;
    std::mutex io_mutex;
    bool failed = false;
    bool lighting_initialized = false;
    bool have_previous = false;
    bool previous_off = false;
    RadeonReference::Message previous{};
    std::chrono::steady_clock::time_point last_update{};
};
