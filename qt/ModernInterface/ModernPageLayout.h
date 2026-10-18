/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <QWidget>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QGroupBox>
namespace ModernPageLayout
{
// Only transfer the layout, never the tab page. Plugins may cast and retain
// that page, and own widgets with hardware callbacks inside it.
inline bool MakeScrollable(QWidget* page)
{
    if(!page || page->property("modernScrollPage").toBool() || !page->layout())
        return false;
    page->setProperty("modernScrollPage", true);
    auto content = new QWidget(page);
    content->setObjectName("EffectPageContent");
    auto original = page->layout();
    content->setLayout(original);
    original->setSizeConstraint(QLayout::SetMinimumSize);
    auto outer = new QVBoxLayout(page);
    outer->setContentsMargins(0, 0, 0, 0);
    auto scroll = new QScrollArea(page);
    scroll->setObjectName("EffectPageScroll");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(content);
    outer->addWidget(scroll);
    for(auto group : content->findChildren<QGroupBox*>())
        if(group->objectName() == "performance_group" && group->layout())
            group->setMinimumHeight(group->minimumSizeHint().height());
    return true;
}
}
