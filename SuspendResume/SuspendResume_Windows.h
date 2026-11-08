/*---------------------------------------------------------*\
| SuspendResume_Windows.h                                   |
|                                                           |
|   Suspend/resume Windows implementation                   |
|                                                           |
|   Zach Deibert (zachdeibert)                  12 Nov 2024 |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <QAbstractNativeEventFilter>
#include <QByteArray>
#include <QCoreApplication>
#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include "SuspendResume.h"

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#define NEFResultType long
#else
#define NEFResultType qintptr
#endif

class SuspendResumeListener : public SuspendResumeListenerBase, private QAbstractNativeEventFilter
{
protected:
    SuspendResumeListener();
    virtual ~SuspendResumeListener();
    void StopPowerNotifications();

private:
    QObject* dispatch_context=nullptr;
    QTimer* resume_timer=nullptr;
    QElapsedTimer resume_settling;
    bool suspended=false;
    bool resume_notified=false;
    bool stopping=false;
    bool nativeEventFilter(const QByteArray &event_type, void *message, NEFResultType *result);
};
