/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "DetectionManager.h"
#include "HidApiLifecycle.h"
#include "RGBController_FlydigiApex5.h"
#include <cwchar>

DetectedControllers DetectFlydigiApex5(hid_device_info* info,const std::string&)
{
    DetectedControllers result;
    hid_device* device=HidApiLifecycle::OpenPath(info->path);
    if(device)result.push_back(new RGBController_FlydigiApex5(device,info->path,info->product_string&&std::wcsstr(info->product_string,L"Wireless")));
    return result;
}
// Exact observed RGB collection. Never open keyboard, pointer or XInput interfaces.
REGISTER_HID_DETECTOR_IPU("Flydigi APEX 5 (USB / 2.4 GHz)",DetectFlydigiApex5,0x37D7,0x2501,2,0xFFA0,1);
