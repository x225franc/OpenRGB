/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "RGBControllerInterface.h"
#include <algorithm>
#include <vector>

namespace RadeonReference
{
// Removing software modes and inverting the speed range must not shift a saved
// hardware selection. Preserve current capability metadata and match exact names.
inline unsigned RestoreModes(std::vector<mode>& current, const std::vector<mode>& saved, int active)
{
    unsigned restored_active = 0;
    for(unsigned target = 0; target < current.size(); ++target)
    {
        auto& destination = current[target];
        for(unsigned source = 0; source < saved.size(); ++source)
        {
            const auto& previous = saved[source];
            if(previous.name != destination.name || previous.flags != destination.flags
               || previous.colors.size() < destination.colors_min
               || previous.colors.size() > destination.colors_max) continue;
            destination.speed = std::clamp(previous.speed, std::min(destination.speed_min, destination.speed_max), std::max(destination.speed_min, destination.speed_max));
            destination.brightness = std::clamp(previous.brightness, std::min(destination.brightness_min, destination.brightness_max), std::max(destination.brightness_min, destination.brightness_max));
            destination.direction = previous.direction;
            destination.color_mode = previous.color_mode;
            destination.colors = previous.colors;
            if(int(source) == active) restored_active = target;
            break;
        }
    }
    return restored_active;
}
}
