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

#include "RGBController_FlydigiBS2Pro.h"
DetectedControllers DetectFlydigiBS2(hid_device_info* info,const std::string&)
{
    DetectedControllers result;
    const bool bluetooth=info->bus_type==HID_API_BUS_BLUETOOTH;
    if((bluetooth&&info->usage!=0xFF)||(!bluetooth&&(info->bus_type!=HID_API_BUS_USB||info->interface_number!=0||info->usage!=1)))return result;
    auto device=HidApiLifecycle::OpenPath(info->path);
    if(device)result.push_back(new RGBController_FlydigiBS2Pro(device,info->path,bluetooth));
    return result;
}
REGISTER_HID_DETECTOR_PU("Flydigi BS2 Pro (USB)",DetectFlydigiBS2,0x37D7,0x1002,0xFFA0,1);
REGISTER_HID_DETECTOR_PU("Flydigi BS2 Pro (Bluetooth)",DetectFlydigiBS2,0x37D7,0x1002,0xFFA0,0xFF);
