/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "RGBController.h"
#include <hidapi.h>
#include <memory>
#include <atomic>
class FlydigiBS2Worker;
class RGBController_FlydigiBS2Pro : public RGBController
{
public:
    RGBController_FlydigiBS2Pro(hid_device* device,const char* path,bool bluetooth);
    ~RGBController_FlydigiBS2Pro() override;
    void DeviceUpdateLEDs() override;
    void DeviceUpdateMode() override;
    void SetCustomMode() override;
    void DeviceUpdateZoneLEDs(int) override;
    void DeviceUpdateSingleLED(int) override;
private:
    std::unique_ptr<FlydigiBS2Worker> worker;
    std::atomic<uint8_t> previous_preset{0};
};
