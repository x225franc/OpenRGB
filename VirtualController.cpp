/*---------------------------------------------------------*\
| VirtualController.cpp                                     |
|                                                           |
|   Virtual controller for visual map plugin                |
|                                                           |
|   This file is part of the OpenRGB Visual Map Plugin      |
|   project                                                 |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include <algorithm>
#include <set>
#include "OpenRGBVisualMapPlugin.h"
#include "RGBControllerInterface.h"
#include "VirtualController.h"

std::string VirtualController::VIRTUAL_CONTROLLER_SERIAL = "VISUAL_MAP_VISUAL_CONTROLLER_SERIAL";

std::vector<VirtualController*> VirtualController::instances;
std::mutex                      VirtualController::instances_mutex;

VirtualController::VirtualController()
{
    width                                               = 1;
    height                                              = 1;

    /*-----------------------------------------------------*\
    | Setup controller details                              |
    \*-----------------------------------------------------*/
    setup.object_ptr                                    = this;
    setup.name                                          = "Visual Map Controller";
    setup.vendor                                        = "OpenRGB Visual Map Plugin";
    setup.description                                   = "Virtual controller provided by the OpenRGB Visual Map Plugin";
    setup.version                                       = VERSION_STRING;
    setup.serial                                        = VIRTUAL_CONTROLLER_SERIAL;
    setup.location                                      = "Somewhere over the rainbow";
    setup.type                                          = DEVICE_TYPE_VIRTUAL;
    setup.active_mode                                   = 0;

    /*-----------------------------------------------------*\
    | Setup zone                                            |
    \*-----------------------------------------------------*/
    setup.zones.resize(1);

    setup.zones[0].name                                 = "Virtual Zone";
    setup.zones[0].type                                 = ZONE_TYPE_MATRIX;

    /*-----------------------------------------------------*\
    | Setup mode details                                    |
    \*-----------------------------------------------------*/
    setup.modes.resize(1);

    setup.modes[0].name                                 = "Direct";
    setup.modes[0].value                                = 0;
    setup.modes[0].flags                                = MODE_FLAG_HAS_PER_LED_COLOR | MODE_FLAG_HAS_BRIGHTNESS;
    setup.modes[0].brightness                           = 100;
    setup.modes[0].brightness_max                       = 100;
    setup.modes[0].brightness_min                       = 0;
    setup.modes[0].color_mode                           = MODE_COLORS_PER_LED;

    /*-----------------------------------------------------*\
    | Setup function pointers                               |
    \*-----------------------------------------------------*/
    setup.DeviceConfigureZone                           = nullptr;
    setup.DeviceUpdateLEDs                              = DeviceUpdateLEDs_func;
    setup.DeviceUpdateZoneLEDs                          = nullptr;
    setup.DeviceUpdateSingleLED                         = nullptr;
    setup.DeviceUpdateMode                              = nullptr;
    setup.DeviceSaveMode                                = nullptr;
    setup.DeviceUpdateZoneMode                          = nullptr;
    setup.DeviceUpdateDeviceSpecificConfiguration       = nullptr;
    setup.DeviceUpdateDeviceSpecificZoneConfiguration   = nullptr;

    virtual_controller = OpenRGBVisualMapPlugin::api->CreateVirtualRGBController(&setup);

    /*-----------------------------------------------------*\
    | Track this instance for lifecycle management          |
    \*-----------------------------------------------------*/
    {
        std::lock_guard<std::mutex> lock(instances_mutex);
        instances.push_back(this);
    }
}

VirtualController::~VirtualController()
{
    /*-----------------------------------------------------*\
    | Remove this instance from the tracking list           |
    \*-----------------------------------------------------*/
    {
        std::lock_guard<std::mutex> lock(instances_mutex);
        instances.erase(std::remove(instances.begin(), instances.end(), this), instances.end());
    }
}

void VirtualController::UpdateVirtualZone()
{
    std::lock_guard<std::recursive_mutex> state_lock(added_zones_mutex);
    /*-----------------------------------------------------*\
    | Resize matrix map                                     |
    \*-----------------------------------------------------*/
    setup.zones[0].matrix_map.height                    = height;
    setup.zones[0].matrix_map.width                     = width;
    setup.zones[0].matrix_map.map.resize(height * width);

    std::vector<std::vector<std::string>> real_leds;
    real_leds.resize(setup.zones[0].matrix_map.map.size());

    /*-----------------------------------------------------*\
    | Fill the map with NA                                  |
    \*-----------------------------------------------------*/
    for(std::size_t led_idx = 0; led_idx < setup.zones[0].matrix_map.map.size(); led_idx++)
    {
        setup.zones[0].matrix_map.map[led_idx]          = NA;
    }

    /*-----------------------------------------------------*\
    | Iterate controllers, count and place LEDs             |
    | Count real LEDs in the same loop                      |
    \*-----------------------------------------------------*/
    unsigned int    map_leds_count                      = 0;

    led_routes.clear();

    for(ControllerZone* ctrl_zone: added_zones)
    {
        RGBControllerInterface* controller = ctrl_zone->controller;
        std::vector<LedRouting::LedRoute> routes = LedRouting::BuildRoutes(
            ctrl_zone,
            QPointF(ctrl_zone->settings.x, ctrl_zone->settings.y),
            QSize(width, height));

        for(const LedRouting::LedRoute& route : routes)
        {
            const std::string led_name = controller->GetLEDName(route.led_index);

            for(const LedRouting::PixelWeight& overlap : route.overlaps)
            {
                const unsigned int xy = overlap.pixel.y() * width + overlap.pixel.x();

                if(real_leds[xy].empty())
                {
                    map_leds_count++;
                }

                real_leds[xy].push_back(led_name);
            }
        }

        led_routes[ctrl_zone] = std::move(routes);
    }

    /*-----------------------------------------------------*\
    | Update zone data                                      |
    \*-----------------------------------------------------*/
    setup.zones[0].leds_count                       = map_leds_count;
    setup.zones[0].leds_min                         = map_leds_count;
    setup.zones[0].leds_max                         = map_leds_count;
    setup.leds.resize(map_leds_count);

    /*-----------------------------------------------------*\
    | Update LED names and positions in matrix map          |
    \*-----------------------------------------------------*/
    int i = 0;

    for(unsigned int h = 0; h < height; h++)
    {
        for(unsigned int w = 0; w < width; w++)
        {
            unsigned int xy = (h*width) + w;

            if(!real_leds[xy].empty())
            {
                setup.zones[0].matrix_map.map[xy]   = i;

                if(real_leds[xy].size() > 1)
                {
                    setup.leds[i].name = "Multiple LEDs (" + std::to_string(real_leds[xy].size()) + ")";
                }
                else
                {
                    setup.leds[i].name = real_leds[xy][0];
                }

                i++;
            }
        }
    }

    OpenRGBVisualMapPlugin::api->UpdateVirtualRGBController(virtual_controller, &setup);
}

void VirtualController::DeviceUpdateLEDs()
{
    std::lock_guard<std::recursive_mutex> state_lock(added_zones_mutex);
    float           brightness = virtual_controller->GetModeBrightness(0) / 100.f;
    unsigned int    color_index = 0;
    QImage          image(width, height, QImage::Format_ARGB32);
    QColor          transparent("#00000000");

    for(unsigned int h = 0; h < height; h++)
    {
        for(unsigned int w = 0; w < width; w++)
        {
            QColor color;

            if(setup.zones[0].matrix_map.map[(h*width) + w] == NA)
            {
                color = transparent;
            }
            else
            {
                const RGBColor rgb = virtual_controller->GetColor(color_index++);
                color = QColor(RGBGetRValue(rgb) * brightness, RGBGetGValue(rgb)* brightness, RGBGetBValue(rgb)* brightness);
            }

            image.setPixelColor(w, h, color);
        }
    }

    ApplyToDevice(image);
}

void VirtualController::UpdateSize(int w, int h)
{
    std::lock_guard<std::recursive_mutex> state_lock(added_zones_mutex);
    width   = w;
    height  = h;

    UpdateVirtualZone();
}

void VirtualController::SetName(std::string name)
{
    std::lock_guard<std::recursive_mutex> state_lock(added_zones_mutex);
    setup.name = name;
    OpenRGBVisualMapPlugin::api->UpdateVirtualRGBController(virtual_controller, &setup);   
}

void VirtualController::SetPostUpdateCallBack(std::function<void(const QImage&)> callback)
{
    std::lock_guard<std::recursive_mutex> state_lock(added_zones_mutex);
    this->callback = callback;
}

void VirtualController::Register(bool state, bool hide_members)
{
    if(state)
    {
        if(!registered)
        {
            ForceDirectMode();

            if(hide_members)
            {
                std::set<RGBControllerInterface*> controllers;

                for(ControllerZone* ctrl_zone: added_zones)
                {
                    controllers.insert(ctrl_zone->controller);
                }

                /*-----------------------------------------*\
                | Ensure controller is in the latest list   |
                | as this function can be called during     |
                | list updates                              |
                \*-----------------------------------------*/
                std::vector<RGBControllerInterface*> available_controllers = OpenRGBVisualMapPlugin::api->GetRGBControllers();

                for(std::set<RGBControllerInterface*>::iterator it = controllers.begin(); it != controllers.end(); )
                {
                    if(std::find(available_controllers.begin(), available_controllers.end(), *it) == available_controllers.end())
                    {
                        it = controllers.erase(it);
                    }
                    else
                    {
                        ++it;
                    }
                }

                for(RGBControllerInterface* controller : controllers)
                {
                    controller->SetHidden(true);
                }

                members_hidden = true;
            }

            OpenRGBVisualMapPlugin::api->RegisterVirtualRGBControllerInThread(virtual_controller);
            registered = true;
        }
    }
    else
    {
        if(registered)
        {
            OpenRGBVisualMapPlugin::api->UnregisterVirtualRGBController(virtual_controller);
            registered = false;

            if(members_hidden)
            {
                std::set<RGBControllerInterface*> controllers;

                for(ControllerZone* ctrl_zone: added_zones)
                {
                    controllers.insert(ctrl_zone->controller);
                }

                /*-----------------------------------------*\
                | Ensure controller is in the latest list   |
                | as this function can be called during     |
                | list updates                              |
                \*-----------------------------------------*/
                std::vector<RGBControllerInterface*> available_controllers = OpenRGBVisualMapPlugin::api->GetRGBControllers();

                for(std::set<RGBControllerInterface*>::iterator it = controllers.begin(); it != controllers.end(); )
                {
                    if(std::find(available_controllers.begin(), available_controllers.end(), *it) == available_controllers.end())
                    {
                        it = controllers.erase(it);
                    }
                    else
                    {
                        ++it;
                    }
                }

                for(RGBControllerInterface* controller : controllers)
                {
                    controller->SetHidden(false);
                }

                members_hidden = false;
            }
        }
    }
}

void VirtualController::ForceDirectMode()
{
    std::lock_guard<std::recursive_mutex> state_lock(added_zones_mutex);
    std::set<RGBControllerInterface*> controllers;

    for(ControllerZone* ctrl_zone: added_zones)
    {
        controllers.insert(ctrl_zone->controller);
    }

    for(RGBControllerInterface* controller : controllers)
    {
        for(unsigned int i = 0; i < controller->GetModeCount(); i++)
        {
            if(controller->GetModeName(i) == "Direct")
            {
                controller->SetActiveMode(i);
            }
        }
    }
}

bool VirtualController::HasZone(ControllerZone* ctrl_zone)
{
    std::lock_guard<std::recursive_mutex> state_lock(added_zones_mutex);
    return std::find(added_zones.begin(), added_zones.end(),ctrl_zone) != added_zones.end();
}

void VirtualController::Add(ControllerZone* ctrl_zone)
{
    std::lock_guard<std::recursive_mutex> lock(added_zones_mutex);

    if(!HasZone(ctrl_zone))
    {
        added_zones.push_back(ctrl_zone);

        /*-------------------------------------------------*\
        | Make sure to have the correct LED size            |
        \*-------------------------------------------------*/
        if(ctrl_zone->isCustomShape() && ctrl_zone->led_count() !=  ctrl_zone->settings.custom_shape->led_positions.size())
        {
            ctrl_zone->settings.custom_shape->resizeCustomShape(ctrl_zone->led_count());
        }

        /*-------------------------------------------------*\
        | Hide controller if necessary                      |
        \*-------------------------------------------------*/
        if(registered && members_hidden && !ctrl_zone->controller->GetHidden())
        {
            ctrl_zone->controller->SetHidden(true);
        }
    }
}

void VirtualController::Remove(ControllerZone* ctrl_zone)
{
    std::lock_guard<std::recursive_mutex> lock(added_zones_mutex);

    if(HasZone(ctrl_zone))
    {
        added_zones.erase(std::find(added_zones.begin(), added_zones.end(), ctrl_zone));
    }
}

void VirtualController::Clear()
{
    /*-----------------------------------------------------*\
    | Locked so a call in flight on the device thread       |
    | drains before the zones go away. Clearing here on     |
    | DETECTION_STARTED stops the device thread touching    |
    | controllers that are about to be freed.               |
    \*-----------------------------------------------------*/
    std::lock_guard<std::recursive_mutex> lock(added_zones_mutex);

    added_zones.clear();
    led_routes.clear();
}

std::vector<ControllerZone*> VirtualController::GetZones()
{
    std::lock_guard<std::recursive_mutex> state_lock(added_zones_mutex);
    return added_zones;
}

bool VirtualController::IsEmpty()
{
    std::lock_guard<std::recursive_mutex> state_lock(added_zones_mutex);
    return added_zones.empty();
}

std::string VirtualController::GetName()
{
    std::lock_guard<std::recursive_mutex> state_lock(added_zones_mutex);
    return virtual_controller->GetName();
}

unsigned int VirtualController::GetTotalLeds()
{
    std::lock_guard<std::recursive_mutex> state_lock(added_zones_mutex);
    unsigned int result = 0;

    for(ControllerZone* ctrl_zone : added_zones)
    {
        result += ctrl_zone->led_count();
    }

    return result;
}

void VirtualController::ApplyImage(const QImage& original)
{
    std::lock_guard<std::recursive_mutex> state_lock(added_zones_mutex);
    /*-----------------------------------------------------*\
    | Make sure the image only targets the existing LEDs    |
    \*-----------------------------------------------------*/
    QImage image(width, height, QImage::Format_ARGB32);

    float brightness = virtual_controller->GetModeBrightness(0) / 100.f;

    QColor transparent("#00000000");

    unsigned int color_idx = 0;

    for(unsigned int h = 0; h < height; h++)
    {
        for(unsigned int w = 0; w < width; w++)
        {
            QColor color;

            if(setup.zones[0].matrix_map.map[(h*width) + w] == NA)
            {
                color = transparent;
            }
            else
            {
                QColor original_color = original.pixelColor(QPoint(w, h));
                int red = original_color.red()   * brightness;
                int grn = original_color.green() * brightness;
                int blu = original_color.blue()  * brightness;
                color = QColor(red, grn, blu);

                if(color_idx < virtual_controller->GetLEDCount())
                {
                    virtual_controller->SetColor(color_idx, ToRGBColor(red,grn,blu));
                }

                color_idx++;
            }

            image.setPixelColor(w, h, color);
        }
    }

    ApplyToDevice(image);
}

void VirtualController::ApplyToDevice(const QImage& image)
{
    /*-----------------------------------------------------*\
    | Make sure we update the controller only once by using |
    | a set                                                 |
    \*-----------------------------------------------------*/
    std::set<RGBControllerInterface*> controllers;

    /*-----------------------------------------------------*\
    | Held across the apply so the zones (and the           |
    | controllers they point at) cannot be cleared/freed    |
    | mid-update by a DETECTION_STARTED on another thread.  |
    \*-----------------------------------------------------*/
    {
        std::lock_guard<std::recursive_mutex> lock(added_zones_mutex);

        for(ControllerZone* ctrl_zone: added_zones)
        {
            ApplyToZone(ctrl_zone, image);
            controllers.insert(ctrl_zone->controller);
        }

        for(RGBControllerInterface* controller : controllers)
        {
            controller->UpdateLEDs();
        }
    }

    callback(image);
}

void VirtualController::ApplyToZone(ControllerZone* ctrl_zone, const QImage& image)
{
    std::lock_guard<std::recursive_mutex> state_lock(added_zones_mutex);
    RGBControllerInterface* controller  = ctrl_zone->controller;
    unsigned int            start_idx;

    if(ctrl_zone->is_segment)
    {
        start_idx                       = controller->GetZoneSegmentStartIndex(ctrl_zone->zone_idx, ctrl_zone->segment_idx);
    }
    else
    {
        start_idx                       = controller->GetZoneStartIndex(ctrl_zone->zone_idx);
    }

    const auto routes = led_routes.find(ctrl_zone);

    if(routes == led_routes.end())
    {
        return;
    }

    for(const LedRouting::LedRoute& route : routes->second)
    {
        const QColor color = LedRouting::MixColor(image, route);
        controller->SetColor(start_idx + route.led_index,
                             ToRGBColor(color.red(), color.green(), color.blue()));
    }
}

void VirtualController::UnregisterAll()
{
    /*-----------------------------------------------------*\
    | Unregister all registered virtual controllers and     |
    | unhide any hidden members. This is called during      |
    | Unload() to ensure all controllers are properly       |
    | cleaned up before the plugin code is unloaded.        |
    \*-----------------------------------------------------*/
    std::lock_guard<std::mutex> lock(instances_mutex);

    for(VirtualController* vc : instances)
    {
        if(vc->registered && OpenRGBVisualMapPlugin::api)
        {
            vc->Register(false, false);
        }
    }
}

void VirtualController::DeviceUpdateLEDs_func(void* object_ptr)
{
    ((VirtualController*)object_ptr)->DeviceUpdateLEDs();
}
