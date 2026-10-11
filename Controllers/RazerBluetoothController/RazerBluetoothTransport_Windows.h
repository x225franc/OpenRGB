/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
// Windows SDK C++/WinRT still includes experimental/coroutine in C++17.
#ifndef _SILENCE_EXPERIMENTAL_COROUTINE_DEPRECATION_WARNINGS
#define _SILENCE_EXPERIMENTAL_COROUTINE_DEPRECATION_WARNINGS
#endif
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Storage.Streams.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <vector>

namespace RazerBLE
{
using namespace winrt;
using namespace Windows::Devices::Bluetooth;
using namespace Windows::Devices::Bluetooth::GenericAttributeProfile;
using namespace Windows::Devices::Enumeration;
using namespace Windows::Storage::Streams;
using Bytes = std::vector<uint8_t>;
struct Apartment
{
    Apartment() { winrt::init_apartment(winrt::apartment_type::multi_threaded); }
    ~Apartment() { winrt::uninit_apartment(); }
    Apartment(Apartment const&) = delete;
    Apartment& operator=(Apartment const&) = delete;
};
inline const guid ServiceUUID{L"52401523-f97c-7f90-0e7f-6c6f4e36db1c"};
inline const guid WriteUUID{L"52401524-f97c-7f90-0e7f-6c6f4e36db1c"};
inline const guid NotifyUUID{L"52401525-f97c-7f90-0e7f-6c6f4e36db1c"};

template<class Operation> auto Await(Operation operation)
{
    if(operation.wait_for(std::chrono::seconds(10)) != Windows::Foundation::AsyncStatus::Completed)
    {
        operation.Cancel();
        throw std::runtime_error("Bluetooth operation timed out or failed");
    }
    return operation.GetResults();
}
inline Bytes ReadBuffer(IBuffer const& buffer)
{
    auto reader = DataReader::FromBuffer(buffer);
    Bytes bytes(reader.UnconsumedBufferLength());
    reader.ReadBytes(bytes);
    return bytes;
}
struct Notifications
{
    std::mutex mutex;
    std::condition_variable changed;
    std::vector<Bytes> frames;
};

class Transport
{
    BluetoothLEDevice device{nullptr};
    GattDeviceService service{nullptr};
    GattCharacteristic write{nullptr}, notify{nullptr};
    std::shared_ptr<Notifications> replies = std::make_shared<Notifications>();
    event_token token{};
    bool listening = false;
    uint8_t request = 0x30;
public:
    bool keyboard = false;
    std::vector<uint8_t> zones;
    std::vector<Bytes> initial;
    void Close() noexcept
    {
        if(listening)
        {
            notify.ValueChanged(token);
            listening = false;
            try { Await(notify.WriteClientCharacteristicConfigurationDescriptorAsync(GattClientCharacteristicConfigurationDescriptorValue::None)); } catch(...) {}
        }
        write = nullptr;
        notify = nullptr;
        if(service) { try { service.Close(); } catch(...) {} service = nullptr; }
        if(device) { try { device.Close(); } catch(...) {} device = nullptr; }
    }
    ~Transport() { Close(); }
    void Open(hstring const& id)
    {
        Close();
        device = Await(BluetoothLEDevice::FromIdAsync(id));
        if(!device) throw std::runtime_error("Bluetooth device unavailable");
        auto services = Await(device.GetGattServicesForUuidAsync(ServiceUUID, BluetoothCacheMode::Uncached));
        if(services.Status() != GattCommunicationStatus::Success || services.Services().Size() != 1)
            throw std::runtime_error("Vendor service unavailable");
        service = services.Services().GetAt(0);
        auto identityServices = Await(device.GetGattServicesForUuidAsync(guid{L"0000180a-0000-1000-8000-00805f9b34fb"}, BluetoothCacheMode::Uncached));
        Bytes identity;
        for(auto const& s : identityServices.Services())
        {
            auto chars = Await(s.GetCharacteristicsForUuidAsync(guid{L"00002a50-0000-1000-8000-00805f9b34fb"}, BluetoothCacheMode::Uncached));
            for(auto const& c : chars.Characteristics())
            {
                auto result = Await(c.ReadValueAsync(BluetoothCacheMode::Uncached));
                if(result.Status() == GattCommunicationStatus::Success) identity = ReadBuffer(result.Value());
            }
            s.Close();
        }
        if(identity.size() != 7 || identity[0] != 2) throw std::runtime_error("Missing USB-source PnP identity");
        unsigned vid = identity[1] | (identity[2] << 8), pid = identity[3] | (identity[4] << 8);
        keyboard = vid == 0x1532 && pid == 0x0272;
        if(!keyboard && !(vid == 0x068E && pid == 0x00D8)) throw std::runtime_error("Unvalidated Bluetooth model");
        zones = keyboard ? std::vector<uint8_t>{5} : std::vector<uint8_t>{1,10};
        auto chars = Await(service.GetCharacteristicsAsync(BluetoothCacheMode::Uncached));
        if(chars.Status() != GattCommunicationStatus::Success) throw std::runtime_error("Characteristics unavailable");
        for(auto const& c : chars.Characteristics())
        {
            if(c.Uuid() == WriteUUID) write = c;
            if(c.Uuid() == NotifyUUID) notify = c;
        }
        if(!write || !notify) throw std::runtime_error("Vendor characteristics missing");
        auto state = replies;
        token = notify.ValueChanged([state](auto const&, GattValueChangedEventArgs const& args)
        {
            auto bytes = ReadBuffer(args.CharacteristicValue());
            std::lock_guard<std::mutex> lock(state->mutex);
            if(state->frames.size() < 64) state->frames.push_back(std::move(bytes));
            state->changed.notify_all();
        });
        listening = true;
        if(Await(notify.WriteClientCharacteristicConfigurationDescriptorAsync(GattClientCharacteristicConfigurationDescriptorValue::Notify)) != GattCommunicationStatus::Success)
            throw std::runtime_error("Notifications unavailable");
        auto catalogue = Exchange({0x10,0x80,0,1});
        initial.clear();
        for(uint8_t zone : zones)
        {
            if(std::find(catalogue.begin(), catalogue.end(), zone) == catalogue.end()) throw std::runtime_error("Zone missing");
            auto color = Exchange({0x10,0x83,0,zone});
            auto brightness = Exchange({0x10,0x85,1,zone});
            if(color.size() != 10 || brightness.size() != 1) throw std::runtime_error("Invalid zone response");
            color.push_back(brightness[0]); initial.push_back(color);
        }
    }
    Bytes Exchange(std::array<uint8_t,4> key, Bytes const& payload = {})
    {
        uint8_t id = request++;
        { std::lock_guard<std::mutex> lock(replies->mutex); replies->frames.clear(); }
        auto send = [&](Bytes const& bytes)
        {
            DataWriter writer;
            writer.WriteBytes(bytes);
            auto result = Await(write.WriteValueWithResultAsync(writer.DetachBuffer(), GattWriteOption::WriteWithResponse));
            if(result.Status() != GattCommunicationStatus::Success) throw std::runtime_error("GATT write failed");
        };
        Bytes header{id, static_cast<uint8_t>(payload.size()),0,0};
        header.insert(header.end(),key.begin(),key.end());
        send(header);
        if(!payload.empty()) send(payload);
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        std::unique_lock<std::mutex> lock(replies->mutex);
        for(;;)
        {
            auto found = std::find_if(replies->frames.begin(), replies->frames.end(), [id](Bytes const& f)
            { return f.size() >= 8 && f[0] == id && f[2] == 0 && f[3] == 0 && (f[7] == 2 || f[7] == 3 || f[7] == 5); });
            if(found != replies->frames.end())
            {
                if((*found)[7] != 2) throw std::runtime_error("Vendor rejected command");
                size_t length = (*found)[1];
                Bytes value;
                for(auto it = std::next(found); it != replies->frames.end(); ++it) value.insert(value.end(), it->begin(), it->end());
                if(value.size() >= length) { value.resize(length); return value; }
            }
            if(replies->changed.wait_until(lock,deadline) == std::cv_status::timeout) throw std::runtime_error("Vendor reply timeout");
        }
    }
    bool IsConnected() const noexcept
    {
        try { return device && device.ConnectionStatus() == BluetoothConnectionStatus::Connected; }
        catch(...) { return false; }
    }
    void Set(uint8_t zone, Bytes const& state)
    {
        if(state.size() != 11 || std::find(zones.begin(),zones.end(),zone) == zones.end()) throw std::runtime_error("Invalid lighting state");
        Exchange({0x10,3,0,zone}, Bytes(state.begin(),state.begin()+10));
        Exchange({0x10,5,1,zone}, {state[10]});
    }
};
}
