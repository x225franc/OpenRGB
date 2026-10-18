/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <QWidget>
#include <QVBoxLayout>
#include <QTabWidget>
#include <QTableWidget>
#include <QGroupBox>
#include <QPushButton>
#include <QLabel>
#include <QToolButton>
#include <QComboBox>
namespace ModernPluginLayout
{
inline bool MakeCommandBar(QWidget* root)
{
    if(!root || root->property("modernCommandLayout").toBool())
        return false;
    auto label = root->findChild<QLabel*>("ActiveProfileLabel");
    auto profiles = root->findChild<QComboBox*>("ProfileBox");
    auto save = root->findChild<QToolButton*>("ButtonSaveProfile");
    auto remove = root->findChild<QPushButton*>("ButtonDeleteProfile");
    auto preview = root->findChild<QPushButton*>("ButtonToggleDeviceView");
    auto scan = root->findChild<QPushButton*>("ButtonRescan");
    if(!label || !profiles || !save || !remove || !preview || !scan)
        return false;
    root->setProperty("modernCommandLayout", true);
    delete root->layout();
    auto layout = new QHBoxLayout(root);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(8);
    layout->addWidget(label);
    layout->addWidget(profiles, 1);
    layout->addWidget(save);
    layout->addWidget(remove);
    layout->addSpacing(12);
    layout->addWidget(preview);
    layout->addWidget(scan);
    return true;
}
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
