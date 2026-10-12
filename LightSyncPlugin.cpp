#include "LightSyncPlugin.h"
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QSpinBox>
#include <QHeaderView>
#include <QDesktopServices>
#include <QUrl>
#include <QDir>
#include <QEventLoop>
#include <QJsonDocument>
#include <QScrollArea>
#include <windows.h>
#include <powrprof.h>

LightSyncPlugin::~LightSyncPlugin(){Unload();}
OpenRGBPluginInfo LightSyncPlugin::GetPluginInfo()
{
    OpenRGBPluginInfo info{};info.Name="LightSync";info.Label="LightSync";info.Description="Native LAN lighting and Lenovo Windows Dynamic Lighting";
    info.Icon.load(":/LightSync/icon.png");info.URL="https://github.com/x225franc/OpenRGB/tree/plugins/lightsync";info.Version="0.1.1";info.Location=OPENRGB_PLUGIN_LOCATION_TOP;info.ProtocolVersion=OPENRGB_PLUGIN_API_VERSION;return info;
}
void LightSyncPlugin::load(nlohmann::json j)
{
    if(!j.is_object())return;
    settings.enabled=j.value("enabled",true);settings.offOnSuspend=j.value("off_on_suspend",false);settings.onOnResume=j.value("on_on_resume",true);
    settings.interpolate=j.value("interpolate",true);settings.laptop=j.value("laptop",true);
    settings.darkBelow=std::clamp(j.value("dark_below",1),0,25);settings.fadeMs=std::clamp(j.value("fade_ms",100),0,2000);
    settings.goveeFps=std::clamp(j.value("govee_fps",25),1,60);settings.yeelightFps=std::clamp(j.value("yeelight_fps",2),1,2);
    if(j.contains("devices"))cachedDevices=QJsonDocument::fromJson(QByteArray::fromStdString(j["devices"].dump())).array();
    disabled=j.value("disabled_devices",nlohmann::json::object());if(!disabled.is_object())disabled=nlohmann::json::object();
}
nlohmann::json LightSyncPlugin::OnProfileSave()
{
    return {{"enabled",settings.enabled},{"off_on_suspend",settings.offOnSuspend},{"on_on_resume",settings.onOnResume},{"interpolate",settings.interpolate},
            {"laptop",settings.laptop},{"dark_below",settings.darkBelow},{"fade_ms",settings.fadeMs},{"govee_fps",settings.goveeFps},{"yeelight_fps",settings.yeelightFps},{"disabled_devices",disabled},{"devices",nlohmann::json::parse(QJsonDocument(cachedDevices).toJson(QJsonDocument::Compact).toStdString())}};
}
void LightSyncPlugin::Load(OpenRGBPluginAPIInterface* ptr)
{
    api=ptr;directory=QString::fromStdWString((api->GetConfigurationDirectory()/"LightSync").wstring());QDir().mkpath(directory);
    try{load(api->GetSettings("LightSync"));}catch(...){settings=LightSyncSettings{};}
    buildUI();qRegisterMetaType<LightSyncSettings>();
    engine=new LightSyncEngine(api,directory,cachedDevices);engine->moveToThread(&worker);
    connect(&worker,&QThread::finished,engine,&QObject::deleteLater);
    connect(engine,&LightSyncEngine::inventory,this,&LightSyncPlugin::rows);
    connect(engine,&LightSyncEngine::message,this,[this](QString text){if(messageLabel)messageLabel->setText(text);});
    QEventLoop initialization;connect(engine,&LightSyncEngine::initialized,&initialization,&QEventLoop::quit);
    worker.start();auto initial=settings;
    QMetaObject::invokeMethod(engine,[this,initial]{engine->configure(initial);engine->start();},Qt::QueuedConnection);
    initialization.exec(QEventLoop::ExcludeUserInputEvents);
    qApp->installNativeEventFilter(this);
    powerNotification=RegisterSuspendResumeNotification(reinterpret_cast<HANDLE>(widget->winId()),DEVICE_NOTIFY_WINDOW_HANDLE);
}
void LightSyncPlugin::apply(bool save)
{
    if(updating)return;
    if(save){api->SetSettings("LightSync",OnProfileSave());api->SaveSettings();}
    if(engine){auto current=settings;QMetaObject::invokeMethod(engine,[this,current]{engine->configure(current);},Qt::QueuedConnection);}
}
void LightSyncPlugin::buildUI()
{
    widget=new QWidget;auto* layout=new QVBoxLayout(widget);auto* header=new QHBoxLayout;
    auto* title=new QLabel("LightSync");QFont font=title->font();font.setPointSize(font.pointSize()+5);font.setBold(true);title->setFont(font);header->addWidget(title);header->addStretch();
    enabled=new QCheckBox("Enabled");enabled->setChecked(settings.enabled);header->addWidget(enabled);
    auto* scan=new QPushButton("Search again");auto* restart=new QPushButton("Restart");header->addWidget(scan);header->addWidget(restart);layout->addLayout(header);
    connect(enabled,&QCheckBox::toggled,this,[this](bool checked){settings.enabled=checked;apply();});
    connect(scan,&QPushButton::clicked,this,[this]{if(engine)QMetaObject::invokeMethod(engine,&LightSyncEngine::scan,Qt::QueuedConnection);});
    connect(restart,&QPushButton::clicked,this,[this]{if(engine)QMetaObject::invokeMethod(engine,&LightSyncEngine::restart,Qt::QueuedConnection);});
    auto* explain=new QLabel("Devices and individual LEDs appear in Devices, Effects and Visual Map. Assign effects in OpenRGB.\nSelect Lenovo Lighting directly in Ambient to map the whole screen independently of your Razer Visual Map.");explain->setWordWrap(true);layout->addWidget(explain);
    keyboardStatus=new QLabel("Lenovo Lighting: searching for keyboard");keyboardStatus->setWordWrap(true);layout->addWidget(keyboardStatus);
    table=new QTableWidget(0,6);table->setHorizontalHeaderLabels({"Control","Device","Address","Model","LEDs","Status"});table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);table->horizontalHeader()->setSectionResizeMode(5,QHeaderView::Stretch);table->setEditTriggers(QAbstractItemView::NoEditTriggers);layout->addWidget(table,1);
    connect(table,&QTableWidget::itemChanged,this,[this](QTableWidgetItem* item){
        if(updating||item->column()!=0)return;QString id=item->data(Qt::UserRole).toString();bool active=item->checkState()==Qt::Checked;
        disabled[id.toStdString()]=!active;api->SetSettings("LightSync",OnProfileSave());api->SaveSettings();
        QMetaObject::invokeMethod(engine,[this,id,active]{engine->setDeviceEnabled(id,active);},Qt::QueuedConnection);
    });
    auto* group=new QGroupBox("Lights");auto* form=new QFormLayout(group);
    auto check=[this,form](QString label,bool value,bool* field){auto* box=new QCheckBox(label);box->setChecked(value);form->addRow(box);connect(box,&QCheckBox::toggled,this,[this,field](bool v){*field=v;apply();});return box;};
    off=check("Turn lights off on sleep, hibernate or shutdown",settings.offOnSuspend,&settings.offOnSuspend);
    on=check("Turn lights on at startup or resume",settings.onOnResume,&settings.onOnResume);
    smooth=check("Smooth transitions (interpolation)",settings.interpolate,&settings.interpolate);
    laptop=check("Enable Lenovo keyboard (Windows Dynamic Lighting)",settings.laptop,&settings.laptop);
    auto spin=[this,form](QString label,int value,int min,int max,int* field,QString name){auto* box=new QSpinBox;box->setObjectName(name);box->setRange(min,max);box->setValue(value);form->addRow(label,box);connect(box,qOverload<int>(&QSpinBox::valueChanged),this,[this,field](int v){*field=v;apply();});};
    spin("Yeelight: turn off in the dark (below %)",settings.darkBelow,0,25,&settings.darkBelow,"dark");
    spin("Yeelight: fade out before turning off (ms)",settings.fadeMs,0,2000,&settings.fadeMs,"fade");
    spin("Govee update rate (FPS)",settings.goveeFps,1,60,&settings.goveeFps,"govee");
    spin("Yeelight update rate (FPS)",settings.yeelightFps,1,2,&settings.yeelightFps,"yeelight");layout->addWidget(group);
    auto* maintenance=new QHBoxLayout;auto* openLog=new QPushButton("Open log");auto* openFolder=new QPushButton("Open folder");auto* windows=new QPushButton("Windows Dynamic Lighting settings");maintenance->addWidget(openLog);maintenance->addWidget(openFolder);maintenance->addWidget(windows);maintenance->addStretch();layout->addLayout(maintenance);
    connect(openLog,&QPushButton::clicked,this,[this]{QDesktopServices::openUrl(QUrl::fromLocalFile(directory+"/LightSync.log"));});
    connect(openFolder,&QPushButton::clicked,this,[this]{QDesktopServices::openUrl(QUrl::fromLocalFile(directory));});
    connect(windows,&QPushButton::clicked,this,[]{QDesktopServices::openUrl(QUrl("ms-settings:personalization-lighting"));});
    diagnostics=new QLabel;messageLabel=new QLabel;messageLabel->setWordWrap(true);layout->addWidget(diagnostics);layout->addWidget(messageLabel);
}
void LightSyncPlugin::rows(QJsonArray data)
{
    if(!engine||!table)return;
    updating=true;table->setRowCount(data.size());int controlled=0;
    QJsonArray discovered;for(auto row:data)discovered.append(row.toObject()["cache"]);
    if(discovered!=cachedDevices){cachedDevices=discovered;api->SetSettings("LightSync",OnProfileSave());api->SaveSettings();}
    for(int i=0;i<data.size();i++)
    {
        auto d=data[i].toObject();auto id=d["id"].toString();bool active=!disabled.value(id.toStdString(),false);
        auto* control=new QTableWidgetItem;control->setData(Qt::UserRole,id);control->setFlags(Qt::ItemIsEnabled|Qt::ItemIsUserCheckable);control->setCheckState(active?Qt::Checked:Qt::Unchecked);table->setItem(i,0,control);
        QStringList cells={d["name"].toString(),d["ip"].toString(),d["model"].toString(),QString::number(d["leds"].toInt()),d["status"].toString()};
        for(int c=0;c<cells.size();c++)table->setItem(i,c+1,new QTableWidgetItem(cells[c]));
                if(d["model"].toString()=="Lenovo Lighting"){
            bool available=settings.enabled&&active&&settings.laptop&&cells.back().startsWith("Controlled");
            QString status=!settings.enabled?"Plugin disabled":!settings.laptop||!active?"Keyboard control disabled":cells.back();
            keyboardStatus->setText("Lenovo Lighting: "+status);
            QPalette palette=keyboardStatus->palette();palette.setColor(QPalette::WindowText,available?QColor(100,220,130):QColor(245,190,85));keyboardStatus->setPalette(palette);
        }
        if(active&&settings.enabled&&cells.back().contains("Streaming"))controlled++;
        if(active!=d["enabled"].toBool())QMetaObject::invokeMethod(engine,[this,id,active]{engine->setDeviceEnabled(id,active);},Qt::QueuedConnection);
    }
    diagnostics->setText(QString("Engine: %1 | Devices discovered: %2 | LAN devices streaming: %3").arg(settings.enabled?"running":"disabled").arg(data.size()).arg(controlled));updating=false;
}
void LightSyncPlugin::OnProfileLoad(nlohmann::json data)
{
    try{load(data);}catch(...){return;}
    updating=true;enabled->setChecked(settings.enabled);off->setChecked(settings.offOnSuspend);on->setChecked(settings.onOnResume);smooth->setChecked(settings.interpolate);laptop->setChecked(settings.laptop);
    for(auto pair:{std::pair<const char*,int>{"dark",settings.darkBelow},{"fade",settings.fadeMs},{"govee",settings.goveeFps},{"yeelight",settings.yeelightFps}})widget->findChild<QSpinBox*>(pair.first)->setValue(pair.second);
    updating=false;apply();
}
bool LightSyncPlugin::nativeEventFilter(const QByteArray&,void* event,qintptr*)
{
    auto* msg=static_cast<MSG*>(event);if(!engine)return false;
    if(msg->message==WM_POWERBROADCAST)
    {
        if(msg->wParam==PBT_APMSUSPEND)QMetaObject::invokeMethod(engine,[this]{engine->suspend(true);},Qt::QueuedConnection);
        else if(msg->wParam==PBT_APMRESUMEAUTOMATIC||msg->wParam==PBT_APMRESUMESUSPEND)QMetaObject::invokeMethod(engine,[this]{engine->suspend(false);},Qt::QueuedConnection);
    }
    else if(msg->message==WM_ENDSESSION&&msg->wParam)QMetaObject::invokeMethod(engine,[this]{engine->suspend(true);},Qt::BlockingQueuedConnection);
    return false;
}
void LightSyncPlugin::Unload()
{
    if(!engine)return;qApp->removeNativeEventFilter(this);if(powerNotification){UnregisterSuspendResumeNotification(powerNotification);powerNotification=nullptr;}
    disconnect(engine,nullptr,this,nullptr);
    QEventLoop shutdown;connect(engine,&LightSyncEngine::shutdownComplete,&shutdown,&QEventLoop::quit);
    QMetaObject::invokeMethod(engine,&LightSyncEngine::shutdown,Qt::QueuedConnection);
    shutdown.exec(QEventLoop::ExcludeUserInputEvents);worker.quit();worker.wait();engine=nullptr;
    delete widget;widget=nullptr;api=nullptr;
}
