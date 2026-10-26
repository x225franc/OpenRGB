/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "RGBController.h"
#include "RadeonReferenceProtocol.h"
#include "RadeonReferenceOutput.h"
#include <memory>
#include <chrono>

class RadeonReferenceADL;
class RadeonReferenceDeviceLock;
class RadeonReferencePowerEvents;

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
    RadeonReference::SendResult SendFrame(const RadeonReference::Frame& frame, uint64_t epoch);
    std::shared_ptr<RadeonReferenceADL> adl;
    int adapter;
    std::unique_ptr<RadeonReference::OutputSession> output_session;
    std::unique_ptr<RadeonReference::OutputWorker> output;
    std::unique_ptr<RadeonReferenceDeviceLock> ownership;
    std::unique_ptr<RadeonReferencePowerEvents> power_events;
    uint64_t last_activity = 0;
    std::chrono::steady_clock::time_point next_warning{};
};
