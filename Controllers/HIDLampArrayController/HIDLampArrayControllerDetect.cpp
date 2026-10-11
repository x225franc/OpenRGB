/*---------------------------------------------------------*\
| HIDLampArrayControllerDetect.cpp                          |
|                                                           |
|   Detector for HID LampArray Devices                      |
|                                                           |
|   Adam Honse (calcprogrammer1@gmail.com)      26 Mar 2024 |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#include "DetectionManager.h"
#include "ResourceManager.h"
#include "SettingsManager.h"
#include "HIDLampArrayController.h"
#include "RGBController.h"
#include "RGBController_HIDLampArray.h"
#include <vector>
#include <hidapi.h>

DetectedControllers DetectHIDLampArrayControllers(hid_device_info* info, const std::string& /*name*/)
{
    DetectedControllers detected_controllers;
    hid_device*         dev;

    /* Leave the user's Lenovo keyboard to Windows Dynamic Lighting.
     * This opt-in exclusion is deliberately checked before opening the HID.
     */
    const json settings = ResourceManager::get()->GetSettingsManager()->GetSettings("HIDLampArray");
    if(info->vendor_id == 0x048D && info->product_id == 0xC193
       && settings.is_object() && settings.value("ExcludeLenovoLighting", false))
    {
        return detected_controllers;
    }

    dev = hid_open_path(info->path);

    if(dev)
    {
        HIDLampArrayController* controller = new HIDLampArrayController(dev, info->path);

        /*-------------------------------------------------*\
        | Only create the RGBController if there are lamps  |
        | detected in the controller.  Otherwise, delete    |
        | the controller.                                   |
        \*-------------------------------------------------*/
        if(controller->GetLampCount() > 0)
        {
            RGBController_HIDLampArray* rgb_controller = new RGBController_HIDLampArray(controller);

            detected_controllers.push_back(rgb_controller);
        }
        else
        {
            delete controller;
        }
    }

    return(detected_controllers);
}

REGISTER_HID_DETECTOR_PU_ONLY("HID LampArray Device", DetectHIDLampArrayControllers, 0x59, 0x01);
