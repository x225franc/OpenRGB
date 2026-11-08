/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#ifdef _WIN32
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <windows.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <memory>
#include <cwchar>

// The observer runs this same executable before Qt/service initialization.
// It never loads plugins, enumerates hardware, or uses a scheduled task.
namespace NativeRecovery
{
inline void Log(const char* message)
{
    wchar_t folder[32768];DWORD n=GetEnvironmentVariableW(L"LOCALAPPDATA",folder,32768);if(!n||n>=32768)return;
    std::wstring directory=std::wstring(folder)+L"\\OpenRGB";CreateDirectoryW(directory.c_str(),nullptr);
    HANDLE file=CreateFileW((directory+L"\\NativeRecovery.log").c_str(),FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return;SYSTEMTIME time;GetSystemTime(&time);char line[512];
    int count=snprintf(line,sizeof(line),"%04u-%02u-%02u %02u:%02u:%02u UTC pid=%lu %s\r\n",time.wYear,time.wMonth,time.wDay,time.wHour,time.wMinute,time.wSecond,GetCurrentProcessId(),message);DWORD written=0;if(count>0)WriteFile(file,line,DWORD(count),&written,nullptr);CloseHandle(file);
}
inline HANDLE cancel_event = nullptr;
inline unsigned restart_attempt = 0;
inline void MarkNormalExit() { if(cancel_event) SetEvent(cancel_event); }
inline void Stop() { MarkNormalExit();if(cancel_event){CloseHandle(cancel_event);cancel_event=nullptr;} }

inline std::wstring Quote(const std::wstring& value)
{
    std::wstring out=L"\"";size_t slashes=0;
    for(wchar_t ch:value)
    {
        if(ch==L'\\') {++slashes;continue;}
        if(ch==L'\"')out.append(slashes*2+1,L'\\');else out.append(slashes,L'\\');
        slashes=0;out+=ch;
    }
    out.append(slashes*2,L'\\');return out+L'\"';
}
inline std::wstring Executable()
{
    wchar_t path[32768];DWORD n=GetModuleFileNameW(nullptr,path,32768);
    return n&&n<32768?std::wstring(path,n):std::wstring();
}
inline std::vector<std::wstring> Arguments()
{
    int count=0;auto raw=CommandLineToArgvW(GetCommandLineW(),&count);
    std::vector<std::wstring> args;
    if(raw){for(int i=1;i<count;++i)args.emplace_back(raw[i]);LocalFree(raw);}
    return args;
}
inline std::wstring Command(const std::wstring& executable,const std::vector<std::wstring>& args)
{
    std::wstring cmd=Quote(executable);for(const auto& arg:args)cmd+=L" "+Quote(arg);return cmd;
}
inline bool IsCrash(DWORD code) { return (code&0xC0000000u)==0xC0000000u; }
struct MonitorState { bool ending=false;bool session_pending=false;ULONGLONG resume_after=0; };
inline LRESULT CALLBACK WindowProc(HWND window,UINT message,WPARAM wparam,LPARAM lparam)
{
    auto state=reinterpret_cast<MonitorState*>(GetWindowLongPtrW(window,GWLP_USERDATA));
    if(message==WM_NCCREATE)
    {
        state=static_cast<MonitorState*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);
        SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(state));
    }
    if(state)
    {
        if(message==WM_QUERYENDSESSION){state->session_pending=true;return TRUE;}
        if(message==WM_ENDSESSION){state->session_pending=false;if(wparam)state->ending=true;}
        if(message==WM_POWERBROADCAST&&(wparam==PBT_APMRESUMEAUTOMATIC||wparam==PBT_APMRESUMESUSPEND))
            state->resume_after=GetTickCount64()+8000;
    }
    return DefWindowProcW(window,message,wparam,lparam);
}
inline void PumpMessages()
{
    MSG message;while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}
}
inline int Monitor(HANDLE parent,HANDLE cancel,HANDLE ready,unsigned attempt,const std::vector<std::wstring>& original)
{
    if(!parent||!cancel||!GetProcessId(parent)){Log("Invalid inherited process handles");return 2;}
    wchar_t parent_path[32768];DWORD length=32768;
    if(!QueryFullProcessImageNameW(parent,0,parent_path,&length)||_wcsicmp(parent_path,Executable().c_str())){Log("Parent executable validation failed");return 2;}
    Log("Observer attached");
    MonitorState state;
    WNDCLASSW wc={};wc.lpfnWndProc=WindowProc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"OpenRGB.NativeRecovery";
    RegisterClassW(&wc);
    HWND window=CreateWindowExW(WS_EX_TOOLWINDOW,wc.lpszClassName,L"OpenRGB Recovery",0,0,0,0,0,nullptr,nullptr,wc.hInstance,&state);
    if(!window)return 2;
    SetEvent(ready);CloseHandle(ready);
    const ULONGLONG started=GetTickCount64();HANDLE handles[]={cancel,parent};bool crashed=false;
    for(;;)
    {
        DWORD result=MsgWaitForMultipleObjects(2,handles,FALSE,INFINITE,QS_ALLINPUT);
        if(result==WAIT_OBJECT_0||result==WAIT_FAILED||state.ending)break;
        if(result==WAIT_OBJECT_0+1)
        {
            DWORD code=0;crashed=GetExitCodeProcess(parent,&code)&&IsCrash(code);Log(crashed?"Exception exit detected":"Normal exit detected");break;
        }
        PumpMessages();if(state.ending)break;
    }
    PumpMessages(); // Drain resume/session notifications even when the process handle won the wait.
    // Wait on the actual process handle, not a stale PID observed during WER dump creation.
    // Normal Quit, sign-out, shutdown, non-crash exits, and intentional teardown never restart.
    if(crashed&&!state.ending&&!state.session_pending&&WaitForSingleObject(cancel,0)!=WAIT_OBJECT_0)
    {
        if(GetTickCount64()-started>600000)attempt=0;
        if(attempt<3)
        {
            const ULONGLONG until=(std::max)(GetTickCount64()+3000,state.resume_after);
            while(GetTickCount64()<until&&!state.ending)
            {
                DWORD wait=static_cast<DWORD>((std::min)(ULONGLONG(1000),until-GetTickCount64()));
                if(MsgWaitForMultipleObjects(1,&cancel,FALSE,wait,QS_ALLINPUT)==WAIT_OBJECT_0){state.ending=true;break;}
                PumpMessages();
            }
            if(!state.ending)
            {
                std::vector<std::wstring> args=original;
                bool minimized=false;for(const auto& a:args)minimized|=a==L"--startminimized";
                if(!minimized)args.emplace_back(L"--startminimized");
                args.emplace_back(L"--native-recovery-attempt");args.push_back(std::to_wstring(attempt+1));
                auto exe=Executable(),cmd=Command(exe,args);STARTUPINFOW si={};si.cb=sizeof(si);si.dwFlags=STARTF_USESHOWWINDOW;si.wShowWindow=SW_HIDE;
                PROCESS_INFORMATION child={};
                if(CreateProcessW(exe.c_str(),&cmd[0],nullptr,nullptr,FALSE,0,nullptr,nullptr,&si,&child)){Log("Replacement launched");CloseHandle(child.hThread);CloseHandle(child.hProcess);}else Log("Replacement launch failed");
            }
        }
    }
    DestroyWindow(window);CloseHandle(parent);CloseHandle(cancel);return 0;
}
inline bool HandleEarlyCommandLine(int& argc,char** argv,int& result)
{
    auto args=Arguments();
    if(args.size()>=5&&args[0]==L"--native-recovery-monitor")
    {
        wchar_t *end=nullptr;
        auto parse=[&end](const std::wstring& text){end=nullptr;auto value=_wcstoui64(text.c_str(),&end,10);return end&&!*end?value:0ull;};
        auto parent=reinterpret_cast<HANDLE>(static_cast<uintptr_t>(parse(args[1])));
        auto cancel=reinterpret_cast<HANDLE>(static_cast<uintptr_t>(parse(args[2])));
        unsigned attempt=static_cast<unsigned>(parse(args[3]));
        auto ready=reinterpret_cast<HANDLE>(static_cast<uintptr_t>(parse(args[4])));
        result=attempt<=3?Monitor(parent,cancel,ready,attempt,{args.begin()+5,args.end()}):2;return true;
    }
    for(int i=1;i+1<argc;++i)if(std::string(argv[i])=="--native-recovery-attempt")
    {
        restart_attempt=static_cast<unsigned>(strtoul(argv[i+1],nullptr,10));
        if(restart_attempt>3){result=2;return true;}
        for(int j=i;j+2<argc;++j)argv[j]=argv[j+2];argc-=2;argv[argc]=nullptr;break;
    }
    return false;
}
inline bool Start()
{
    if(cancel_event)return true;
    auto args=Arguments();
    for(size_t i=0;i+1<args.size();++i)if(args[i]==L"--native-recovery-attempt"){args.erase(args.begin()+i,args.begin()+i+2);break;}
    SECURITY_ATTRIBUTES security={sizeof(security),nullptr,TRUE};
    HANDLE parent=nullptr;
    if(!DuplicateHandle(GetCurrentProcess(),GetCurrentProcess(),GetCurrentProcess(),&parent,SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,TRUE,0))return false;
    HANDLE cancel=CreateEventW(&security,TRUE,FALSE,nullptr);
    if(!cancel){CloseHandle(parent);return false;}
    HANDLE ready=CreateEventW(&security,TRUE,FALSE,nullptr);
    if(!ready){CloseHandle(parent);CloseHandle(cancel);return false;}
    std::vector<std::wstring> observer={L"--native-recovery-monitor",std::to_wstring(reinterpret_cast<uintptr_t>(parent)),std::to_wstring(reinterpret_cast<uintptr_t>(cancel)),std::to_wstring(restart_attempt),std::to_wstring(reinterpret_cast<uintptr_t>(ready))};
    observer.insert(observer.end(),args.begin(),args.end());auto exe=Executable(),cmd=Command(exe,observer);
    SIZE_T bytes=0;InitializeProcThreadAttributeList(nullptr,1,0,&bytes);std::vector<unsigned char> buffer(bytes);
    auto attributes=reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(buffer.data());bool initialized=InitializeProcThreadAttributeList(attributes,1,0,&bytes)!=FALSE;
    HANDLE inherited[]={parent,cancel,ready};bool ok=false;
    if(initialized&&UpdateProcThreadAttribute(attributes,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,inherited,sizeof(inherited),nullptr,nullptr))
    {
        STARTUPINFOEXW si={};si.StartupInfo.cb=sizeof(si);si.StartupInfo.dwFlags=STARTF_USESHOWWINDOW;si.StartupInfo.wShowWindow=SW_HIDE;si.lpAttributeList=attributes;
        PROCESS_INFORMATION child={};ok=CreateProcessW(exe.c_str(),&cmd[0],nullptr,nullptr,TRUE,CREATE_NO_WINDOW|EXTENDED_STARTUPINFO_PRESENT,nullptr,nullptr,&si.StartupInfo,&child)!=FALSE;
        if(ok){
            HANDLE started[]={ready,child.hProcess};
            ok=WaitForMultipleObjects(2,started,FALSE,5000)==WAIT_OBJECT_0;
            if(ok)cancel_event=cancel;else SetEvent(cancel);
            CloseHandle(child.hThread);CloseHandle(child.hProcess);
        }
    }
    if(initialized)DeleteProcThreadAttributeList(attributes);CloseHandle(parent);CloseHandle(ready);if(!ok)CloseHandle(cancel);
    // Also participate in Windows Restart Manager; this alone is not automatic crash recovery.
    using RegisterRestart=HRESULT(WINAPI*)(PCWSTR,DWORD);
    auto reg=reinterpret_cast<RegisterRestart>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"),"RegisterApplicationRestart"));
    if(reg){std::wstring restart;for(const auto& a:args)restart+=Quote(a)+L" ";restart+=L"--startminimized";reg(restart.c_str(),0);}
    Log(ok?"Native observer started":"Native observer start failed");
    return ok;
}
}
#endif
