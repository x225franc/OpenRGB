/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <fstream>
#include <filesystem>
#include <iostream>
#include "../../startup/NativeRecovery_Windows.h"
static void Check(bool value,const char* text){if(!value)throw std::runtime_error(text);std::cout<<"PASS "<<text<<std::endl;}
int main(int argc,char** argv)
{
    int result=0;if(NativeRecovery::HandleEarlyCommandLine(argc,argv,result))return result;
    auto args=NativeRecovery::Arguments();
    for(size_t i=0;i+1<args.size();++i)if(args[i]==L"--native-recovery-attempt"){args.erase(args.begin()+i,args.begin()+i+2);break;}
    args.erase(std::remove(args.begin(),args.end(),L"--startminimized"),args.end());
    if(args.size()==2&&args[0].find(L"--case-")==0)
    {
        if(NativeRecovery::restart_attempt){std::ofstream(std::filesystem::path(args[1]))<<"restarted";return 0;}
        if(args[0]==L"--case-budget")NativeRecovery::restart_attempt=3;
        if(!NativeRecovery::Start())return 2;
        if(args[0]==L"--case-normal")return 0;
        if(args[0]==L"--case-cancel")NativeRecovery::MarkNormalExit();
        TerminateProcess(GetCurrentProcess(),0xC0000005u);return 3;
    }
    try
    {
        Check(NativeRecovery::IsCrash(0xC0000005)&&NativeRecovery::IsCrash(0xC000041D)&&!NativeRecovery::IsCrash(0)&&!NativeRecovery::IsCrash(1),"Only exception-style exits trigger recovery");
        NativeRecovery::MonitorState state;
        WNDCLASSW wc={};wc.lpfnWndProc=NativeRecovery::WindowProc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"OpenRGB.RecoveryTests";RegisterClassW(&wc);
        HWND window=CreateWindowExW(0,wc.lpszClassName,L"",0,0,0,0,0,nullptr,nullptr,wc.hInstance,&state);
        Check(window!=nullptr,"Isolated observer window starts");
        SendMessageW(window,WM_POWERBROADCAST,PBT_APMRESUMEAUTOMATIC,0);Check(state.resume_after>GetTickCount64()+7000,"Resume notifications establish a display-settling recovery grace period");
        SendMessageW(window,WM_QUERYENDSESSION,0,0);Check(state.session_pending&&!state.ending,"Pending sign-out prevents a competing recovery");
        SendMessageW(window,WM_ENDSESSION,FALSE,0);Check(!state.session_pending&&!state.ending,"Cancelled sign-out keeps monitoring active");
        SendMessageW(window,WM_ENDSESSION,TRUE,0);Check(state.ending,"Confirmed Windows session end stops recovery");DestroyWindow(window);
        for(const auto& arg:std::vector<std::wstring>{L"",L"a b",L"trailing\\",L"a\\\"b",L"--config",L"C:\\dossier avec espaces\\"})
        {
            auto cmd=L"test "+NativeRecovery::Quote(arg);int n=0;auto parsed=CommandLineToArgvW(cmd.c_str(),&n);
            Check(parsed&&n==2&&arg==parsed[1],"Windows argument quoting round-trip");LocalFree(parsed);
        }
        for(const auto& name:{L"crash",L"normal",L"cancel",L"budget"})
        {
            auto path=std::filesystem::temp_directory_path()/(std::wstring(L"OpenRGB native recovery ")+name+std::to_wstring(GetCurrentProcessId())+L".txt");
            std::filesystem::remove(path);auto exe=NativeRecovery::Executable();auto cmd=NativeRecovery::Command(exe,{std::wstring(L"--case-")+name,path.wstring()});
            STARTUPINFOW si={};si.cb=sizeof(si);PROCESS_INFORMATION child={};
            Check(CreateProcessW(exe.c_str(),&cmd[0],nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&child),"Isolated test parent starts");
            Check(WaitForSingleObject(child.hProcess,10000)==WAIT_OBJECT_0,"Test parent exits");CloseHandle(child.hThread);CloseHandle(child.hProcess);
            Sleep(5000);const bool exists=std::filesystem::exists(path);
            Check(exists==(std::wstring(name)==L"crash"),"Crash recovers; normal Quit, cancellation, and crash budget do not");std::filesystem::remove(path);
        }
        return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
}
