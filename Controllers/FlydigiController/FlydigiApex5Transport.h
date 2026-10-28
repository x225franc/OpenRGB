/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "HidApiLifecycle.h"
#include <cwchar>
namespace FlydigiApex5
{
// Both transports share a PID. Product strings distinguish the observed USB
// collection from the receiver; enumeration never sends a report to the pad.
inline bool WiredPresent()
{
    bool present=false;
    hid_device_info* devices=HidApiLifecycle::Enumerate(0x37D7,0x2501);
    for(auto item=devices;item;item=item->next)
        if(item->interface_number==2&&item->usage_page==0xFFA0&&item->usage==1&&item->product_string
           &&std::wcsstr(item->product_string,L"APEX5")&&!std::wcsstr(item->product_string,L"Wireless"))present=true;
    hid_free_enumeration(devices);return present;
}
}
