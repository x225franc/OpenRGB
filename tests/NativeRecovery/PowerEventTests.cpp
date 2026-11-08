/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <QCoreApplication>
#include <QTimer>
#include <QElapsedTimer>
#include <QThread>
#include <QObject>
#include <iostream>
#include <windows.h>
#define private public
#include "../../SuspendResume/SuspendResume_Windows.h"
#undef private
#include "../../SuspendResume/SuspendResume_Windows.cpp"
struct Listener:public SuspendResumeListener
{
    int suspends=0,resumes=0;
    void OnSuspend()override{++suspends;}
    void OnResume()override{++resumes;}
    void Send(UINT type,WPARAM value=0){MSG m={};m.message=type;m.wParam=value;qintptr result=0;nativeEventFilter("windows_generic_MSG",&m,&result);}
};
void Wait(int ms){QElapsedTimer t;t.start();while(t.elapsed()<ms){QCoreApplication::processEvents();QThread::msleep(5);}}
void Check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);std::cout<<"PASS "<<text<<std::endl;}
int main(int argc,char** argv){QCoreApplication app(argc,argv);try{
    Listener l;l.Send(WM_POWERBROADCAST,PBT_APMSUSPEND);Check(l.suspends==0,"Suspend work is outside native message dispatch");Wait(20);Check(l.suspends==1,"Suspend work executes on Qt event loop");
    l.Send(WM_POWERBROADCAST,PBT_APMRESUMEAUTOMATIC);l.Send(WM_POWERBROADCAST,PBT_APMRESUMESUSPEND);Check(l.resumes==0,"Resume never runs inside native filter");Wait(500);l.Send(WM_DISPLAYCHANGE);Wait(2100);Check(l.resumes==0,"Display reconfiguration postpones plugin UI restore");Wait(600);Check(l.resumes==1,"Resume duplicates coalesce into one restore");
    l.Send(WM_POWERBROADCAST,PBT_APMRESUMESUSPEND);Wait(2700);Check(l.resumes==1,"Late duplicate resume does not reload plugins twice");
    l.Send(WM_POWERBROADCAST,PBT_APMSUSPEND);Wait(20);l.Send(WM_POWERBROADCAST,PBT_APMRESUMEAUTOMATIC);l.Send(WM_ENDSESSION,TRUE);Wait(2700);Check(l.resumes==1,"Shutdown cancels pending plugin restore");
    {Listener temporary;temporary.Send(WM_POWERBROADCAST,PBT_APMRESUMEAUTOMATIC);}Wait(2700);Check(true,"Destruction cancels queued callbacks");return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}}
