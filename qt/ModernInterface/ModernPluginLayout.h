/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <QWidget>
#include <QVBoxLayout>
#include <QTabWidget>
#include <QTableWidget>
#include <QGroupBox>
#include <QPushButton>
#include <QLabel>
namespace ModernPluginLayout
{
// Layout-only adaptation of the native LightSync panel. All original widgets,
// signal connections and findChild paths remain alive below the plugin root.
inline bool MakeLightSyncSections(QWidget* root)
{
    if(!root || root->property("modernLightSyncLayout").toBool())
        return false;
    auto original = qobject_cast<QVBoxLayout*>(root->layout());
    auto table = root->findChild<QTableWidget*>();
    QGroupBox* preferences = nullptr;
    for(auto group : root->findChildren<QGroupBox*>())
        if(group->title() == "Lights")
            preferences = group;
    if(!original || original->count() < 8 || !table || !preferences)
        return false;
    root->setProperty("modernLightSyncLayout", true);
    auto tabs = new QTabWidget(root);
    tabs->setObjectName("LightSyncSections");
    tabs->setDocumentMode(true);
    auto overview = new QWidget(tabs);
    auto overviewLayout = new QVBoxLayout(overview);
    auto settings = new QWidget(tabs);
    auto settingsLayout = new QVBoxLayout(settings);
    for(auto layout : { overviewLayout, settingsLayout })
    {
        layout->setContentsMargins(0, 14, 0, 0);
        layout->setSpacing(14);
    }
    while(original->count() > 1)
    {
        auto item = original->takeAt(1);
        if(auto widget = item->widget())
        {
            auto target = widget == preferences ? settingsLayout : overviewLayout;
            target->addWidget(widget, widget == table ? 1 : 0);
            delete item;
        }
        else if(auto layout = item->layout())
        {
            // The only secondary layout in this API-5 panel is maintenance.
            layout->setParent(nullptr);
            settingsLayout->addLayout(layout);
        }
        else
            delete item;
    }
    preferences->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    settingsLayout->addStretch();
    tabs->addTab(overview, QObject::tr("Devices"));
    tabs->addTab(settings, QObject::tr("Settings"));
    original->addWidget(tabs, 1);
    return true;
}
}
