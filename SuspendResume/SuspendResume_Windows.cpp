/*---------------------------------------------------------*\
| SuspendResume_Windows.cpp                                 |
|                                                           |
|   Suspend/resume Windows implementation                   |
|                                                           |
|   Zach Deibert (zachdeibert)                  12 Nov 2024 |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include "SuspendResume_Windows.h"
#include "windows.h"
#include "../startup/NativeRecovery_Windows.h"

SuspendResumeListener::SuspendResumeListener()
{
    dispatch_context=new QObject(QCoreApplication::instance());
    resume_timer=new QTimer(dispatch_context);
    resume_timer->setSingleShot(true);
    QObject::connect(resume_timer,&QTimer::timeout,dispatch_context,[this] {
        if(!stopping&&!suspended)OnResume();
    });
    QCoreApplication::instance()->installNativeEventFilter(this);
}

SuspendResumeListener::~SuspendResumeListener()
{
    StopPowerNotifications();
    QCoreApplication::instance()->removeNativeEventFilter(this);
    delete dispatch_context;
}

void SuspendResumeListener::StopPowerNotifications()
{
    stopping=true;resume_timer->stop();
}

bool SuspendResumeListener::nativeEventFilter(const QByteArray &event_type, void *message, NEFResultType *result)
{
    (void)result;
    if(!stopping && event_type == "windows_generic_MSG")
    {
        switch(((MSG *)message)->message)
        {
            case WM_ENDSESSION:
                if(((MSG*)message)->wParam){StopPowerNotifications();NativeRecovery::MarkNormalExit();}
                break;
            case WM_DISPLAYCHANGE:
                if(resume_timer->isActive())
                    resume_timer->start(int(qMax<qint64>(0,qMin<qint64>(2500,10000-resume_settling.elapsed()))));
                break;
            case WM_POWERBROADCAST:
                switch(((MSG *)message)->wParam)
                {
                    case PBT_APMSUSPEND:
                        resume_notified=false;
                        resume_timer->stop();
                        if(!suspended)
                        {
                            suspended=true;
                            // Never destroy plugin UI while Qt is dispatching a native window message.
                            QTimer::singleShot(0,dispatch_context,[this] {if(!stopping&&suspended)OnSuspend();});
                        }
                        break;
                    case PBT_APMRESUMEAUTOMATIC:
                    case PBT_APMRESUMESUSPEND:
                        if(resume_notified)break;
                        resume_notified=true;
                        suspended=false;
                        resume_settling.start();
                        resume_timer->start(2500);
                        break;
                }
                break;
        }
    }
    return false;
}
