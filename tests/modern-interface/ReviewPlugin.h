/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <QObject>
#include <QTimer>
#include <QJsonObject>
#include "OpenRGBPluginInterface.h"
// Development-only Qt driver. Never included in the delivered plugins folder.
class ReviewPlugin : public QObject, public OpenRGBPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID OpenRGBPluginInterface_IID FILE "ReviewPlugin.json")
    Q_INTERFACES(OpenRGBPluginInterface)
public:
    OpenRGBPluginInfo GetPluginInfo() override;
    unsigned int GetPluginAPIVersion() override
    {
        return OPENRGB_PLUGIN_API_VERSION;
    }
    void Load(OpenRGBPluginAPIInterface*) override;
    QWidget* GetWidget() override
    {
        return panel;
    }
    QMenu* GetTrayMenu() override
    {
        return nullptr;
    }
    void Unload() override
    {
        timer.stop();
    }
    void OnProfileAboutToLoad() override { }
    void OnProfileLoad(nlohmann::json) override { }
    nlohmann::json OnProfileSave() override
    {
        return nlohmann::json::object();
    }
    unsigned char* OnSDKCommand(unsigned int, unsigned char*, unsigned int* size) override
    {
        *size = 0;
        return nullptr;
    }
    void ProfileManagerUpdated(unsigned int) override { }
    void ResourceManagerUpdated(unsigned int) override { }
    void SettingsManagerUpdated(unsigned int) override { }

private:
    QWidget* panel = nullptr;
    QTimer timer;
    QString directory, lastId;
    void poll();
    QJsonObject execute(const QJsonObject& command);
};
