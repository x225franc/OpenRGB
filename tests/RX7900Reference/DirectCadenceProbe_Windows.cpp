/* SPDX-License-Identifier: GPL-2.0-or-later */
// Isolated measurement tool. Does not change the production controller.
#include "../../Controllers/AMDRadeonReferenceController/RadeonReferenceADL_Windows.h"
#include <tlhelp32.h>
#include <chrono>
#include <cmath>
#include <cwctype>
#include <fstream>
#include <atomic>
#include <iomanip>
#include <iostream>
#include <thread>

using Clock = std::chrono::steady_clock;
static std::atomic<bool> stop_requested{false};
static BOOL WINAPI StopHandler(DWORD event)
{
    if(event==CTRL_C_EVENT||event==CTRL_BREAK_EVENT) { stop_requested.store(true); return TRUE; }
    return FALSE;
}
static double ms(Clock::duration value) { return std::chrono::duration<double, std::milli>(value).count(); }
static bool ProducersAbsent()
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if(snapshot == INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32W entry{}; entry.dwSize = sizeof(entry);
    bool safe = Process32FirstW(snapshot, &entry) != FALSE;
    if(safe) do
    {
        std::wstring name(entry.szExeFile);
        for(auto& c : name) c = static_cast<wchar_t>(std::towlower(c));
        for(const auto* term : {L"openrgb", L"amd_radeon_rx_7900_xtx_series_rgb_led", L"chroma", L"synapse", L"masterplus"})
            if(name.find(term) != std::wstring::npos) { std::wcerr << L"RGB producer present: " << name << L'\n'; safe = false; }
    } while(Process32NextW(snapshot, &entry));
    CloseHandle(snapshot);
    return safe;
}

class Measurement
{
public:
    RadeonReferenceADL adl;
    int adapter = -1;
    std::ofstream out;
    Clock::time_point epoch = Clock::now();
    unsigned exchanges = 0;
    bool Exchange(const RadeonReference::Message& message, bool echo = true)
    {
        const auto begin = Clock::now();
        const unsigned id = ++exchanges;
        out << "{\"event\":\"exchange-start\",\"id\":" << id << ",\"t_ms\":" << ms(begin-epoch) << ",\"message\":[";
        for(size_t i=0;i<message.size();++i) out << (i ? "," : "") << unsigned(message[i]);
        out << "]}\n";
        ADL_D3DKMT_HANDLE device = 0;
        auto timed = [&](const char* name, auto call)
        {
            const auto start = Clock::now();
            const int result = call();
            const auto end = Clock::now();
            out << "{\"event\":\"management\",\"id\":" << id << ",\"api\":\"" << name
                << "\",\"enter_ms\":" << ms(start-epoch) << ",\"leave_ms\":" << ms(end-epoch) << ",\"result\":" << result << "}\n";
            return result;
        };
        if(timed("PMLog_Create", [&]{return adl.create_device(adl.context2,adapter,&device);}) != ADL_OK) return false;
        const int awake = timed("KeepAwake_Start", [&]{return adl.awake_start(adl.context2,adapter,device);});
        unsigned frame = 0;
        const bool ok = RadeonReference::Exchange(message, [&](bool write, uint8_t* data, unsigned size)
        {
            std::vector<uint8_t> request(data,data+size);
            ADLI2C call{}; call.iSize=sizeof(call); call.iLine=0; call.iAddress=write?0xB4:0xB5;
            call.iAction=write?ADL_DL_I2C_ACTIONWRITE:ADL_DL_I2C_ACTIONREAD; call.iSpeed=100;
            call.iDataSize=static_cast<int>(size); call.pcData=reinterpret_cast<char*>(data);
            const auto start=Clock::now();
            const int result=adl.transfer(adapter,&call);
            const auto end=Clock::now();
            out << "{\"event\":\"transfer\",\"id\":" << id << ",\"frame\":" << ++frame
                << ",\"write\":" << (write?"true":"false") << ",\"enter_ms\":" << ms(start-epoch)
                << ",\"leave_ms\":" << ms(end-epoch) << ",\"result\":" << result << ",\"request\":[";
            for(unsigned i=0;i<size;++i) out << (i?",":"") << unsigned(request[i]);
            out << "],\"response\":[";
            for(unsigned i=0;i<size;++i) out << (i?",":"") << unsigned(data[i]);
            out << "]}\n";
            return result==ADL_OK;
        }, [&]
        {
            const auto start=Clock::now(); Sleep(30); const auto end=Clock::now();
            out << "{\"event\":\"wait\",\"id\":" << id << ",\"enter_ms\":" << ms(start-epoch)
                << ",\"leave_ms\":" << ms(end-epoch) << ",\"requested_ms\":30}\n";
        }, echo);
        if(awake==ADL_OK) timed("KeepAwake_Stop", [&]{return adl.awake_stop(adl.context2,adapter,device);});
        timed("PMLog_Destroy", [&]{return adl.destroy_device(adl.context2,device);});
        out << "{\"event\":\"exchange-end\",\"id\":" << id << ",\"t_ms\":" << ms(Clock::now()-epoch)
            << ",\"duration_ms\":" << ms(Clock::now()-begin) << ",\"ok\":" << (ok?"true":"false") << "}\n";
        out.flush();
        return ok && out.good();
    }
};

int main(int argc, char** argv)
{
    if(argc!=4 && argc!=5) { std::cerr << "Usage: DirectCadenceProbe.exe FPS SECONDS NEW_TRACE.jsonl [mode-once]\n"; return 1; }
    const bool mode_once = argc==5 && std::string(argv[4])=="mode-once";
    if(argc==5 && !mode_once) return 1;
    double fps=0,seconds=0;
    try { fps=std::stod(argv[1]); seconds=std::stod(argv[2]); } catch(...) { return 1; }
    if(!std::isfinite(fps)||!std::isfinite(seconds)||fps<1||fps>10||seconds<1||seconds>1800) return 1;
    if(GetFileAttributesA(argv[3])!=INVALID_FILE_ATTRIBUTES) { std::cerr << "Refusing to overwrite trace\n"; return 1; }
    if(!ProducersAbsent()) return 2;
    SetConsoleCtrlHandler(StopHandler, TRUE);
    Measurement measurement;
    measurement.out.open(argv[3]);
    if(!measurement.out) return 1;
    measurement.out << std::fixed << std::setprecision(6);
    if(!measurement.adl.Initialize()) return 3;
    int count=0;
    if(measurement.adl.count(&count)!=ADL_OK||count<1||count>128) return 3;
    std::vector<AdapterInfo> adapters(static_cast<size_t>(count));
    for(auto& item:adapters) item.iSize=sizeof(item);
    if(measurement.adl.info(adapters.data(),static_cast<int>(adapters.size()*sizeof(AdapterInfo)))!=ADL_OK) return 3;
    for(const auto& item:adapters)
    {
        const std::string pnp(item.strPNPString,strnlen(item.strPNPString,sizeof(item.strPNPString)));
        if(!item.iPresent||pnp.find("VEN_1002&DEV_744C&SUBSYS_0E3B1002")==std::string::npos) continue;
        measurement.adapter=item.iAdapterIndex;
        std::ostringstream pci; pci << "AMD SMU I2C: " << item.iBusNumber << ':' << item.iDeviceNumber << ':' << item.iFunctionNumber;
        RadeonReferenceDeviceLock lock(pci.str());
        if(!lock.Acquired()) return 2;
        measurement.out << "{\"event\":\"start\",\"target_fps\":" << fps << ",\"seconds\":" << seconds
            << ",\"adapter\":" << item.iAdapterIndex << ",\"bus\":" << item.iBusNumber
            << ",\"device\":" << item.iDeviceNumber << ",\"function\":" << item.iFunctionNumber
            << ",\"sequence\":\"" << (mode_once?"static-apply-mode-selected-once":"static-mode-apply") << "\",\"wait_ms\":30,\"unix_ms\":"
            << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
        if(!measurement.Exchange(RadeonReference::Identify(),false)||!measurement.Exchange(RadeonReference::Enable())
           ||!measurement.Exchange(RadeonReference::SelectControl())||!measurement.Exchange(RadeonReference::Apply())) return 4;
        // Explicit experimental branch: known Static selection is sent once.
        // No omitted ACK, reduced wait, unknown opcode or production change.
        if(mode_once && (!measurement.Exchange(RadeonReference::Mode(false))||!measurement.Exchange(RadeonReference::Apply()))) return 4;
        const auto start=Clock::now();
        const auto period=std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(1/fps));
        unsigned frames=0;
        while(std::chrono::duration<double>(Clock::now()-start).count()<seconds)
        {
            if(stop_requested.load() || GetFileAttributesA((std::string(argv[3])+".stop").c_str())!=INVALID_FILE_ATTRIBUTES)
            { measurement.out << "{\"event\":\"stopped-at-frame-boundary\"}\n"; return 6; }
            if(!ProducersAbsent()) { measurement.out << "{\"event\":\"abort-producer\"}\n"; return 2; }
            const auto begin=Clock::now();
            const double t=std::chrono::duration<double>(begin-start).count();
            // Slow continuous hue changes and a gentle brightness envelope; no strobe.
            const double brightness=0.65+0.35*std::sin(t*6.283185307179586/12);
            auto channel=[&](double shift){return static_cast<uint8_t>(std::lround(255*brightness*(0.5+0.5*std::sin(t*6.283185307179586/18+shift))));};
            const auto color=RadeonReference::Static(channel(0),channel(2.094395102),channel(4.188790205),5);
            const bool ok=measurement.Exchange(color)&&(mode_once||measurement.Exchange(RadeonReference::Mode(false)))&&measurement.Exchange(RadeonReference::Apply());
            measurement.out << "{\"event\":\"frame\",\"n\":" << frames++ << ",\"enter_ms\":" << ms(begin-measurement.epoch)
                << ",\"leave_ms\":" << ms(Clock::now()-measurement.epoch) << ",\"ok\":" << (ok?"true":"false") << "}\n";
            measurement.out.flush();
            if(!ok) { std::cerr << "STOP: first failure. No retry or restoration writes.\n"; return 4; }
            // No queued frames, no catch-up burst, one in-flight frame maximum.
            std::this_thread::sleep_until(begin+period);
        }
        measurement.out << "{\"event\":\"complete\",\"frames\":" << frames << ",\"elapsed_ms\":" << ms(Clock::now()-start) << "}\n";
        std::cout << "Completed " << frames << " frames; visual validation required. Last color retained.\n";
        return 0;
    }
    std::cerr << "Exact PCI target not found\n"; return 5;
}
