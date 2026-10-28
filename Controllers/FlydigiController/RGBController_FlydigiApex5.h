/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "RGBController.h"
#include <hidapi.h>
#include <memory>

class FlydigiApex5Worker;
class RGBController_FlydigiApex5 : public RGBController
{
public:
    RGBController_FlydigiApex5(hid_device* device, const char* path, bool wireless);
    ~RGBController_FlydigiApex5() override;
    void SetupZones();
    void DeviceUpdateLEDs() override;
    void DeviceUpdateMode() override;
    void DeviceUpdateZoneLEDs(int) override;
    void DeviceUpdateSingleLED(int) override;
private:
    std::unique_ptr<FlydigiApex5Worker> worker;
};
