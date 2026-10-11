/*---------------------------------------------------------*\
| VirtualController.h                                       |
|                                                           |
|   Virtual controller for visual map plugin                |
|                                                           |
|   This file is part of the OpenRGB Visual Map Plugin      |
|   project                                                 |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#define NA 0xFFFFFFFF

#include <functional>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <QImage>
#include "ControllerZone.h"
#include "LedRouting.h"
#include "RGBControllerInterface.h"

class VirtualController
{
public:
    static std::string VIRTUAL_CONTROLLER_SERIAL;

    VirtualController();
    ~VirtualController();

    /*-----------------------------------------------------*\
    | Virtual RGBController Functions                       |
    \*-----------------------------------------------------*/
    void                            DeviceUpdateLEDs();

    /*-----------------------------------------------------*\
    | Internals                                             |
    \*-----------------------------------------------------*/
    void                            Add(ControllerZone*);
    void                            ApplyImage(const QImage&);
    void                            ApplyToDevice(const QImage&);
    void                            ApplyToZone(ControllerZone*, const QImage&);
    void                            Clear();
    std::string                     GetName();
    unsigned int                    GetTotalLeds();
    std::vector<ControllerZone*>    GetZones();
    bool                            HasZone(ControllerZone*);
    bool                            IsEmpty();
    void                            Register(bool, bool);
    void                            Remove(ControllerZone*);
    void                            SetName(std::string name);
    void                            SetPostUpdateCallBack(std::function<void(const QImage&)>);
    void                            UpdateSize(int,int);
    void                            UpdateVirtualZone();

    /*-----------------------------------------------------*\
    | Static lifecycle management                           |
    \*-----------------------------------------------------*/
    static void                     UnregisterAll();

private:
    RGBControllerInterface*         virtual_controller;
    unsigned int                    width;
    unsigned int                    height;
    bool                            registered = false;
    bool                            members_hidden = false;
    std::function<void(QImage)>     callback;
    std::vector<ControllerZone*>    added_zones;
    std::unordered_map<ControllerZone*, std::vector<LedRouting::LedRoute>> led_routes;
    std::recursive_mutex            added_zones_mutex;
    RGBController_Setup             setup;

    void                            ForceDirectMode();

    static void                     DeviceUpdateLEDs_func(void* object_ptr);

    /*-----------------------------------------------------*\
    | Static instance tracking                              |
    \*-----------------------------------------------------*/
    static std::vector<VirtualController*> instances;
    static std::mutex                    instances_mutex;
};
