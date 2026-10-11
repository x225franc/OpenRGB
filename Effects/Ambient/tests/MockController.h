/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "RGBControllerInterface.h"
#include <vector>
class MockController : public RGBControllerInterface
{
public:
    unsigned int count = 4, matrix_width = 2, matrix_height = 2, transmissions = 0;
    bool invalidate_after_snapshot = false;
    unsigned int raw_map_reads = 0;
    matrix_map_type Snapshot()
    {
        matrix_map_type result; result.width = matrix_width; result.height = matrix_height; result.map = mapping;
        if(invalidate_after_snapshot) { mapping.clear(); mapping.shrink_to_fit(); matrix_width = matrix_height = 99; }
        return result;
    }
    zone_type type = ZONE_TYPE_LINEAR;
    std::vector<RGBColor> colors = std::vector<RGBColor>(1024, 0);
    std::vector<unsigned int> mapping{0, 1, 2, 3};
    std::string GetName() override { return "Ambient test controller"; }
    std::string GetVendor() override { return {}; }
    std::string GetDescription() override { return {}; }
    std::string GetVersion() override { return {}; }
    std::string GetSerial() override { return {}; }
    std::string GetLocation() override { return {}; }
    std::string GetDisplayName() override { return {}; }
    device_type GetDeviceType() override { return DEVICE_TYPE_VIRTUAL; }
    controller_flags GetFlags() override { return {}; }
    bool GetHidden() override { return {}; }
    void SetHidden(bool) override {  }
    zone GetZone(unsigned int) override { return {}; }
    int GetZoneActiveMode(unsigned int) override { return {}; }
    RGBColor GetZoneColor(unsigned int, unsigned int) override { return {}; }
    RGBColor* GetZoneColorsPointer(unsigned int) override { return colors.data() + 7; }
    unsigned int GetZoneCount() override { return {}; }
    std::string GetZoneDisplayName(unsigned int) override { return {}; }
    zone_flags GetZoneFlags(unsigned int) override { return {}; }
    unsigned int GetZoneLEDsCount(unsigned int) override { return count; }
    unsigned int GetZoneLEDsMax(unsigned int) override { return {}; }
    unsigned int GetZoneLEDsMin(unsigned int) override { return {}; }
    matrix_map_type GetZoneMatrixMap(unsigned int) override { return Snapshot(); }
    const unsigned int* GetZoneMatrixMapData(unsigned int) override { ++raw_map_reads; return mapping.data(); }
    unsigned int GetZoneMatrixMapHeight(unsigned int) override { return matrix_height; }
    unsigned int GetZoneMatrixMapWidth(unsigned int) override { return matrix_width; }
    unsigned int GetZoneModeCount(unsigned int) override { return {}; }
    unsigned int GetZoneModeBrightness(unsigned int, unsigned int) override { return {}; }
    unsigned int GetZoneModeBrightnessMax(unsigned int, unsigned int) override { return {}; }
    unsigned int GetZoneModeBrightnessMin(unsigned int, unsigned int) override { return {}; }
    RGBColor GetZoneModeColor(unsigned int, unsigned int, unsigned int) override { return {}; }
    unsigned int GetZoneModeColorMode(unsigned int, unsigned int) override { return {}; }
    unsigned int GetZoneModeColorsCount(unsigned int, unsigned int) override { return {}; }
    unsigned int GetZoneModeColorsMax(unsigned int, unsigned int) override { return {}; }
    unsigned int GetZoneModeColorsMin(unsigned int, unsigned int) override { return {}; }
    unsigned int GetZoneModeDirection(unsigned int, unsigned int) override { return {}; }
    unsigned int GetZoneModeFlags(unsigned int, unsigned int) override { return {}; }
    std::string GetZoneModeName(unsigned int, unsigned int) override { return {}; }
    unsigned int GetZoneModeSpeed(unsigned int, unsigned int) override { return {}; }
    unsigned int GetZoneModeSpeedMax(unsigned int, unsigned int) override { return {}; }
    unsigned int GetZoneModeSpeedMin(unsigned int, unsigned int) override { return {}; }
    std::string GetZoneName(unsigned int) override { return {}; }
    unsigned int GetZoneSegmentCount(unsigned int) override { return {}; }
    segment_flags GetZoneSegmentFlags(unsigned int, unsigned int) override { return {}; }
    unsigned int GetZoneSegmentLEDsCount(unsigned int, unsigned int) override { return count; }
    matrix_map_type GetZoneSegmentMatrixMap(unsigned int, unsigned int) override { return Snapshot(); }
    const unsigned int * GetZoneSegmentMatrixMapData(unsigned int, unsigned int) override { ++raw_map_reads; return mapping.data(); }
    unsigned int GetZoneSegmentMatrixMapHeight(unsigned int, unsigned int) override { return matrix_height; }
    unsigned int GetZoneSegmentMatrixMapWidth(unsigned int, unsigned int) override { return matrix_width; }
    std::string GetZoneSegmentName(unsigned int, unsigned int) override { return {}; }
    unsigned int GetZoneSegmentStartIndex(unsigned int, unsigned int) override { return 3; }
    unsigned int GetZoneSegmentType(unsigned int, unsigned int) override { return type; }
    unsigned int GetZoneStartIndex(unsigned int) override { return 7; }
    zone_type GetZoneType(unsigned int) override { return type; }
    unsigned int GetLEDsInZone(unsigned int) override { return {}; }
    void SetZoneActiveMode(unsigned int, int) override {  }
    void SetZoneColor(unsigned int, unsigned int, RGBColor) override {  }
    void SetZoneModeBrightness(unsigned int, unsigned int, unsigned int) override {  }
    void SetZoneModeColor(unsigned int, unsigned int, unsigned int, RGBColor) override {  }
    void SetZoneModeColorMode(unsigned int, unsigned int, unsigned int) override {  }
    void SetZoneModeColorsCount(unsigned int, unsigned int, unsigned int) override {  }
    void SetZoneModeDirection(unsigned int, unsigned int, unsigned int) override {  }
    void SetZoneModeSpeed(unsigned int, unsigned int, unsigned int) override {  }
    bool SupportsPerZoneModes() override { return {}; }
    unsigned int GetModeCount() override { return {}; }
    unsigned int GetModeBrightness(unsigned int) override { return {}; }
    unsigned int GetModeBrightnessMax(unsigned int) override { return {}; }
    unsigned int GetModeBrightnessMin(unsigned int) override { return {}; }
    RGBColor GetModeColor(unsigned int, unsigned int) override { return {}; }
    unsigned int GetModeColorMode(unsigned int) override { return {}; }
    unsigned int GetModeColorsCount(unsigned int) override { return {}; }
    unsigned int GetModeColorsMax(unsigned int) override { return {}; }
    unsigned int GetModeColorsMin(unsigned int) override { return {}; }
    unsigned int GetModeDirection(unsigned int) override { return {}; }
    unsigned int GetModeFlags(unsigned int) override { return {}; }
    std::string GetModeName(unsigned int) override { return {}; }
    unsigned int GetModeSpeed(unsigned int) override { return {}; }
    unsigned int GetModeSpeedMax(unsigned int) override { return {}; }
    unsigned int GetModeSpeedMin(unsigned int) override { return {}; }
    void SetModeBrightness(unsigned int, unsigned int) override {  }
    void SetModeColor(unsigned int, unsigned int, RGBColor) override {  }
    void SetModeColorMode(unsigned int, unsigned int) override {  }
    void SetModeColorsCount(unsigned int, unsigned int) override {  }
    void SetModeDirection(unsigned int, unsigned int) override {  }
    void SetModeSpeed(unsigned int, unsigned int) override {  }
    int GetActiveMode() override { return {}; }
    void SetActiveMode(int) override {  }
    void SetCustomMode() override {  }
    unsigned int GetLEDCount() override { return unsigned(colors.size()); }
    std::string GetLEDName(unsigned int) override { return {}; }
    std::string GetLEDDisplayName(unsigned int) override { return {}; }
    RGBColor GetColor(unsigned int led) override { return colors.at(led); }
    RGBColor* GetColorsPointer() override { return colors.data(); }
    void SetColor(unsigned int led, RGBColor color) override { colors.at(led) = color; }
    void SetAllColors(RGBColor) override {  }
    void SetAllZoneColors(int, RGBColor) override {  }
    nlohmann::json GetDeviceSpecificConfigurationSchema() override { return {}; }
    nlohmann::json GetDeviceSpecificConfiguration() override { return {}; }
    void SetDeviceSpecificConfiguration(nlohmann::json) override {  }
    nlohmann::json GetDeviceSpecificZoneConfigurationSchema(int) override { return {}; }
    nlohmann::json GetDeviceSpecificZoneConfiguration(int) override { return {}; }
    void SetDeviceSpecificZoneConfiguration(int, nlohmann::json) override {  }
    void RegisterUpdateCallback(RGBControllerCallback, void *) override {  }
    void UnregisterUpdateCallback(void *) override {  }
    void ClearCallbacks() override {  }
    void SignalUpdate(unsigned int) override {  }
    void UpdateLEDs() override { ++transmissions; }
    void UpdateZoneLEDs(int) override {  }
    void UpdateSingleLED(int) override {  }
    void UpdateMode() override {  }
    void UpdateZoneMode(int) override {  }
    void SaveMode() override {  }
    void ClearSegments(int) override {  }
    void AddSegment(int, segment) override {  }
    void ConfigureZone(int, zone) override {  }
    void ResizeZone(int, int) override {  }
    void ConfigureDevice(controller_flags, std::string) override {  }
};
