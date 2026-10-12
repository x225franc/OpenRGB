#pragma once
#include <QObject>
#include <QThread>
#include <QWidget>
#include <QCheckBox>
#include <QLabel>
#include <QTableWidget>
#include <QAbstractNativeEventFilter>
#include "LightSyncEngine.h"
class LightSyncPlugin : public QObject, public OpenRGBPluginInterface, public QAbstractNativeEventFilter
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID OpenRGBPluginInterface_IID FILE "LightSync.json")
    Q_INTERFACES(OpenRGBPluginInterface)
public:
    ~LightSyncPlugin() override;
    OpenRGBPluginInfo GetPluginInfo() override;
    unsigned int GetPluginAPIVersion() override { return OPENRGB_PLUGIN_API_VERSION; }
    void Load(OpenRGBPluginAPIInterface*) override;
    QWidget* GetWidget() override { return widget; }
    QMenu* GetTrayMenu() override { return nullptr; }
    void Unload() override;
    void OnProfileAboutToLoad() override {}
    void OnProfileLoad(nlohmann::json) override;
    nlohmann::json OnProfileSave() override;
    unsigned char* OnSDKCommand(unsigned int,unsigned char*,unsigned int* size) override { *size=0;return nullptr; }
    void ProfileManagerUpdated(unsigned int) override {}
    void ResourceManagerUpdated(unsigned int) override {}
    void SettingsManagerUpdated(unsigned int) override {}
    bool nativeEventFilter(const QByteArray&,void*,qintptr*) override;
private:
    void buildUI();
    void apply(bool save=true);
    void load(nlohmann::json);
    void rows(QJsonArray);
    OpenRGBPluginAPIInterface* api=nullptr;
    LightSyncEngine* engine=nullptr;
    QThread worker;
    QWidget* widget=nullptr;
    QTableWidget* table=nullptr;
    QLabel* diagnostics=nullptr;
    QLabel* messageLabel=nullptr;
    QString directory;
    LightSyncSettings settings;
    QJsonArray cachedDevices;
    nlohmann::json disabled=nlohmann::json::object();
    QCheckBox *enabled=nullptr,*off=nullptr,*on=nullptr,*smooth=nullptr,*laptop=nullptr;
    bool updating=false;
    void* powerNotification=nullptr;
};
