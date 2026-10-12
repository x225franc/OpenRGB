#include "LampArrayBackend.h"
#include <map>
#include <algorithm>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Devices.Lights.h>
#include <winrt/Windows.UI.h>
using namespace winrt;
using namespace Windows::Devices::Lights;
struct LampArrayBackend::Impl { std::map<QString,LampArray> arrays; };
LampArrayBackend::LampArrayBackend() : impl(new Impl) { init_apartment(apartment_type::multi_threaded); }
LampArrayBackend::~LampArrayBackend() { release(); uninit_apartment(); }
std::vector<LampArrayBackend::Device> LampArrayBackend::discover()
{
    std::vector<Device> result;
    auto found = Windows::Devices::Enumeration::DeviceInformation::FindAllAsync(LampArray::GetDeviceSelector()).get();
    std::map<QString,LampArray> refreshed;
    for(auto info : found)
    {
        auto array = LampArray::FromIdAsync(info.Id()).get();
        if(!array || array.HardwareVendorId() == 0x1532 || array.LampCount() <= 0) continue;
        // Only the laptop's Lenovo device: never compete with OpenRGB's native Razer controllers.
        if(array.HardwareVendorId() != 0x17EF && array.HardwareVendorId() != 0x048D) continue;
        Device device; device.id = QString::fromWCharArray(info.Id().c_str()); device.name = "Lenovo Lighting";
        float minX=1e9f,minY=1e9f,maxX=-1e9f,maxY=-1e9f;
        for(int i=0;i<array.LampCount();i++)
        {
            auto p=array.GetLampInfo(i).Position();
            device.positions.emplace_back(p.x,p.y);
            minX=std::min(minX,p.x);maxX=std::max(maxX,p.x);minY=std::min(minY,p.y);maxY=std::max(maxY,p.y);
        }
        for(auto& p:device.positions) p=QPointF((p.x()-minX)/std::max(0.0001f,maxX-minX),(p.y()-minY)/std::max(0.0001f,maxY-minY));
        array.IsEnabled(true); refreshed.emplace(device.id,array); result.push_back(std::move(device));
    }
    impl->arrays.swap(refreshed);
    return result;
}
QString LampArrayBackend::send(QString id,const std::vector<RGBColor>& rgb,int brightness)
{
    auto it=impl->arrays.find(id);if(it==impl->arrays.end()) return "Searching for Windows Dynamic Lighting device";
    try
    {
        auto& array=it->second;
        if(!array.IsAvailable()) return "Waiting for Windows to grant control (Dynamic Lighting priority)";
        if(size_t(array.LampCount())!=rgb.size()) return "Lamp count changed; search again";
        std::vector<Windows::UI::Color> colors;std::vector<int32_t> indices;
        for(size_t i=0;i<rgb.size();i++)
        {
            auto c=rgb[i];colors.push_back({255,uint8_t(RGBGetRValue(c)),uint8_t(RGBGetGValue(c)),uint8_t(RGBGetBValue(c))});indices.push_back(int(i));
        }
        array.BrightnessLevel(std::clamp(brightness/100.0,0.0,1.0));
        array.SetColorsForIndices(colors,indices);
        return "Controlled by Windows Dynamic Lighting";
    }
    catch(hresult_error const& e) { return QString::fromWCharArray(e.message().c_str()); }
}
void LampArrayBackend::release() { impl->arrays.clear(); }
