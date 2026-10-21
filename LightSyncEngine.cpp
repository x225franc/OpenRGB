#include "LightSyncEngine.h"
#include "LampArrayBackend.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QNetworkInterface>
#include <QDir>
#include <QUrl>
#include <QDateTime>
#include <QThread>
#include <algorithm>
#include <cmath>

LightSyncEngine::LightSyncEngine(OpenRGBPluginAPIInterface* a,QString dir,QJsonArray cached):api(a),directory(dir),cachedDevices(cached),logfile(dir+"/LightSync.log") {}
LightSyncEngine::~LightSyncEngine() { delete lamps; }
void LightSyncEngine::log(QString text)
{
    auto line=QDateTime::currentDateTime().toString(Qt::ISODate)+" "+text+"\n";
    if(logfile.isOpen()) { logfile.write(line.toUtf8());logfile.flush(); }
    emit message(text);
}
QByteArray LightSyncEngine::RazerPacket(QByteArray frame)
{
    unsigned char checksum=0;for(auto c:frame)checksum^=static_cast<unsigned char>(c);frame.append(char(checksum));
    QJsonObject data{{"pt",QString::fromLatin1(frame.toBase64())}};
    QJsonObject msg{{"cmd","razer"},{"data",data}};
    return QJsonDocument(QJsonObject{{"msg",msg}}).toJson(QJsonDocument::Compact);
}
QByteArray LightSyncEngine::GoveePacket(const std::vector<RGBColor>& colors)
{
    if(colors.empty() || colors.size()>80) return {};
    int len=int(colors.size()*3+2);QByteArray frame;
    for(int c:{0xBB,len>>8,len&255,0xB0,1,int(colors.size())})frame.append(char(c));
    for(auto c:colors){frame.append(char(RGBGetRValue(c)));frame.append(char(RGBGetGValue(c)));frame.append(char(RGBGetBValue(c)));}
    return RazerPacket(frame);
}
QJsonObject LightSyncEngine::ParseYeelight(const QByteArray& packet)
{
    QMap<QByteArray,QByteArray> fields;
    for(auto line:packet.split('\n')){int colon=line.indexOf(':');if(colon>0)fields[line.left(colon).trimmed().toLower()]=line.mid(colon+1).trimmed();}
    QUrl location(QString::fromUtf8(fields.value("location")));
    if(fields.value("id").isEmpty() || location.scheme()!="yeelight" || location.host().isEmpty())return {};
    return {{"kind","yeelight"},{"id",QString::fromUtf8(fields["id"])},{"ip",location.host()},{"port",location.port(55443)},
            {"model",QString::fromUtf8(fields["model"])},{"name",QString::fromUtf8(fields["name"])}};
}
void LightSyncEngine::start()
{
    QDir().mkpath(directory);logfile.open(QIODevice::Append|QIODevice::Text);clock.start();sender=new QUdpSocket(this);
    refreshDiscovery();
    lamps=new LampArrayBackend;
    timer=new QTimer(this);timer->setTimerType(Qt::PreciseTimer);timer->setInterval(16);connect(timer,&QTimer::timeout,this,&LightSyncEngine::tick);timer->start();
    heartbeat=new QTimer(this);heartbeat->setInterval(20000);connect(heartbeat,&QTimer::timeout,this,&LightSyncEngine::scan);heartbeat->start();
    for(auto item:cachedDevices)found(item.toObject());
    cachedDevices={};emit initialized();
    log("LightSync native transport started");scan();
}
void LightSyncEngine::refreshDiscovery()
{
    QStringList addresses;
    for(auto iface:QNetworkInterface::allInterfaces())
        if((iface.flags()&QNetworkInterface::IsUp)&&!(iface.flags()&QNetworkInterface::IsLoopBack))
            for(auto entry:iface.addressEntries())
                if(entry.ip().protocol()==QAbstractSocket::IPv4Protocol)
                    addresses<<QString::number(iface.index())+":"+entry.ip().toString()+"/"+QString::number(entry.prefixLength());
    addresses.sort();const auto signature=addresses.join(";");
    if(signature==networkSignature&&!discovery.empty())return;
    for(auto* socket:discovery){socket->disconnect(this);socket->close();socket->deleteLater();}
    discovery.clear();networkSignature=signature;
    for(auto iface:QNetworkInterface::allInterfaces())
    {
        if(!(iface.flags()&QNetworkInterface::IsUp)||iface.flags()&QNetworkInterface::IsLoopBack)continue;
        for(auto address:iface.addressEntries())
        {
            auto local=address.ip();if(local.protocol()!=QAbstractSocket::IPv4Protocol||local.toString().startsWith("169.254."))continue;
            auto* g=new QUdpSocket(this);
            if(g->bind(local,4002,QUdpSocket::ShareAddress|QUdpSocket::ReuseAddressHint))
            {
                g->setMulticastInterface(iface);g->joinMulticastGroup(QHostAddress("239.255.255.250"),iface);discovery.push_back(g);
                connect(g,&QUdpSocket::readyRead,this,[this,g,local]{receive(g,local,true);});g->setProperty("govee",true);
            }else{log("Govee discovery bind failed on "+local.toString()+": "+g->errorString());g->deleteLater();}
            auto* y=new QUdpSocket(this);
            if(y->bind(local,0)){y->setMulticastInterface(iface);discovery.push_back(y);connect(y,&QUdpSocket::readyRead,this,[this,y,local]{receive(y,local,false);});}
            else y->deleteLater();
        }
    }
}
void LightSyncEngine::searchAgain(){forgotten.clear();scan();}
void LightSyncEngine::scan()
{
    if(stopped||!settings.enabled||sleeping)return;
    refreshDiscovery();
    log("Searching Govee, Yeelight and Lenovo Dynamic Lighting");
    for(auto* socket:discovery)
    {
        bool g=socket->property("govee").toBool();
        QByteArray request=g?QByteArray("{\"msg\":{\"cmd\":\"scan\",\"data\":{\"account_topic\":\"reserve\"}}}"):
            QByteArray("M-SEARCH * HTTP/1.1\r\nHOST: 239.255.255.250:1982\r\nMAN: \"ssdp:discover\"\r\nST: wifi_bulb\r\n\r\n");
        socket->writeDatagram(request,QHostAddress("239.255.255.250"),g?4001:1982);
    }
    if(settings.laptop)
    {
        try
        {
            for(auto device:lamps->discover())
            {
                QJsonArray positions;for(auto p:device.positions)positions.append(QJsonArray{p.x(),p.y()});
                found({{"kind","laptop"},{"id",device.id},{"name",device.name},{"model","Lenovo Lighting"},{"count",int(device.positions.size())},{"positions",positions}});
            }
        }
        catch(std::exception const& e){log("Dynamic Lighting discovery: "+QString::fromUtf8(e.what()));}
        catch(...){log("Dynamic Lighting discovery failed; check OpenRGB application identity");}
    }
    publish();
}
void LightSyncEngine::receive(QUdpSocket* socket,QHostAddress local,bool isGovee)
{
    while(socket->hasPendingDatagrams())
    {
        QByteArray packet;auto length=socket->pendingDatagramSize();if(length>65536){socket->readDatagram(nullptr,0);continue;}packet.resize(int(length));
        QHostAddress remote;socket->readDatagram(packet.data(),packet.size(),&remote);
        QJsonObject device;
        if(isGovee)
        {
            auto msg=QJsonDocument::fromJson(packet).object().value("msg").toObject();
            if(msg.value("cmd").toString()!="scan")continue;
            auto data=msg.value("data").toObject();auto model=data.value("sku").toString();
            device={{"kind","govee"},{"id",data.value("device").toString()},{"ip",remote.toString()},{"model",model},{"name",model},
                    {"count",model=="H6056"?6:10}};
        }else device=ParseYeelight(packet);
        if(device.isEmpty()||device.value("id").toString().isEmpty())continue;
        device["local"]=local.toString();found(device);
    }
}
void LightSyncEngine::found(QJsonObject item)
{
    QString id=item["kind"].toString()+":"+item["id"].toString();
    if(forgotten.contains(id))return;
    auto it=std::find_if(devices.begin(),devices.end(),[&](auto& d){return d->id==id;});
    if(it!=devices.end())
    {
        auto& d=**it;auto ip=item["ip"].toString();
        const QHostAddress local(item["local"].toString());const int port=item["port"].toInt(55443);
        if(d.ip!=ip||d.local!=local||d.port!=port){release(d);d.ip=ip;d.local=local;d.port=port;d.nextAttempt=0;d.externallyOff=false;d.sent.clear();d.sentRevision=0;log("Updated endpoint for "+id+": "+ip);}
        d.lastSeen=clock.elapsed();d.discoveryData=item;publish();return;
    }
    auto d=std::make_shared<LightSyncDevice>();d->id=id;d->kind=item["kind"].toString();d->model=item["model"].toString();
    d->discoveryData=item;d->name=item["name"].toString();if(d->name.isEmpty())d->name=d->model;
    d->ip=item["ip"].toString();d->local=QHostAddress(item["local"].toString());d->port=item["port"].toInt(55443);
    d->count=std::clamp(item["count"].toInt(1),1,1024);d->latest.resize(d->count);d->lastSeen=clock.elapsed();
    d->setup.name=(d->kind=="laptop"?QString("Lenovo Lighting"):"LightSync "+d->name+" ["+d->ip+"]").toStdString();
    d->setup.serial=("LightSync:"+d->id).toStdString();d->setup.vendor="LightSync";d->setup.description="Native LightSync transport; effects supplied by OpenRGB";
    d->setup.location=(d->kind=="laptop"?d->id:d->ip).toStdString();d->setup.version="0.1.0";d->setup.type=d->kind=="laptop"?DEVICE_TYPE_KEYBOARD:DEVICE_TYPE_LIGHT;
    d->setup.active_mode=0;d->setup.modes.resize(1);auto& m=d->setup.modes[0];m.name="Direct";m.value=0;m.flags=MODE_FLAG_HAS_PER_LED_COLOR|MODE_FLAG_HAS_BRIGHTNESS;
    m.brightness=100;m.brightness_min=0;m.brightness_max=100;m.color_mode=MODE_COLORS_PER_LED;
    d->setup.zones.resize(1);auto& zone=d->setup.zones[0];zone.name=d->kind=="laptop"?"Keyboard":"Segments";zone.type=ZONE_TYPE_LINEAR;
    zone.leds_count=d->count;zone.leds_min=d->count;zone.leds_max=d->count;
    d->setup.leds.resize(d->count);for(int i=0;i<d->count;i++){d->setup.leds[i].name="LED "+std::to_string(i+1);d->setup.leds[i].value=i;}
    if(d->kind=="laptop")
    {
        zone.type=ZONE_TYPE_MATRIX;zone.matrix_map.width=128;zone.matrix_map.height=64;zone.matrix_map.map.assign(128*64,0xFFFFFFFFu);
        auto positions=item["positions"].toArray();for(int i=0;i<positions.size();i++)
        {
            auto p=positions[i].toArray();int cell=std::clamp(int(p[1].toDouble()*63),0,63)*128+std::clamp(int(p[0].toDouble()*127),0,127);
            int first=cell;while(zone.matrix_map.map[cell]!=0xFFFFFFFFu){cell=(cell+1)%(128*64);if(cell==first)break;}zone.matrix_map.map[cell]=i;
        }
    }
    createController(d);devices.push_back(d);log(QString("Discovered ")+QString::fromStdString(d->setup.name)+QString("; ")+QString::number(d->count)+" LEDs");publish();
}
void LightSyncEngine::createController(const std::shared_ptr<LightSyncDevice>& d)
{
    d->setup.object_ptr=d.get();d->setup.DeviceUpdateLEDs=Update;d->setup.DeviceUpdateZoneLEDs=UpdateZone;d->setup.DeviceUpdateSingleLED=UpdateLED;d->setup.DeviceUpdateMode=Update;
    d->controller=api->CreateVirtualRGBController(&d->setup);api->RegisterVirtualRGBController(d->controller);
}
void LightSyncEngine::Update(void* ptr)
{
    auto& d=*static_cast<LightSyncDevice*>(ptr);if(!d.controller)return;
    std::lock_guard<std::mutex> lock(d.mutex);
    auto* colors=d.controller->GetColorsPointer();std::copy(colors,colors+d.count,d.latest.begin());d.brightness=std::clamp(int(d.controller->GetModeBrightness(0)),0,100);++d.revision;
}
void LightSyncEngine::UpdateZone(void* p,int){Update(p);}void LightSyncEngine::UpdateLED(void* p,int){Update(p);}
void LightSyncEngine::configure(LightSyncSettings next)
{
    bool was=settings.enabled;settings=next;
    if(!next.enabled){for(auto& d:devices)release(*d);if(lamps)lamps->release();}
    else if(!was){for(auto& d:devices)d->externallyOff=false;scan();}publish();
}
void LightSyncEngine::setDeviceEnabled(QString id,bool enabled)
{
    for(auto& d:devices)if(d->id==id){d->enabled=enabled;if(!enabled)release(*d);else{d->nextAttempt=0;d->externallyOff=false;}}publish();
}
void LightSyncEngine::changeAddress(QString id,QString address)
{
    const QHostAddress target(address);
    if(target.protocol()!=QAbstractSocket::IPv4Protocol||target.isNull()||target.isMulticast()||target.isLoopback()||target==QHostAddress(QHostAddress::Broadcast)){log("Invalid IPv4 address");return;}
    for(auto& d:devices)if(d->id==id&&d->kind!="laptop")
    {
        QHostAddress local;int bestPrefix=-1;
        for(auto iface:QNetworkInterface::allInterfaces())if(iface.flags()&QNetworkInterface::IsUp)
            for(auto entry:iface.addressEntries())if(entry.ip().protocol()==QAbstractSocket::IPv4Protocol&&entry.prefixLength()>bestPrefix&&target.isInSubnet(entry.ip(),entry.prefixLength())){local=entry.ip();bestPrefix=entry.prefixLength();}
        if(local.isNull()){log("No active local IPv4 interface reaches "+address+"; check the network address");return;}
        auto item=d->discoveryData;item["ip"]=target.toString();item["local"]=local.toString();
        found(item);log("Address changed; device identity and Visual Map routes retained");return;
    }
}
void LightSyncEngine::forgetDevice(QString id)
{
    auto it=std::find_if(devices.begin(),devices.end(),[&](auto& d){return d->id==id&&d->kind!="laptop";});
    if(it==devices.end())return;
    auto node=*it;node->enabled=false;release(*node);
    if(node->controller)api->UnregisterVirtualRGBController(node->controller);
    // Effects/Visual Map may still hold references: defer destruction to unload.
    retired.push_back(node);devices.erase(it);forgotten.insert(id);
    log("Forgot "+id+"; Search again allows discovery again");publish();
}
void LightSyncEngine::restart(){for(auto& d:devices){release(*d);d->externallyOff=false;}if(lamps)lamps->resume();scan();log("Transport restarted; device identities and Visual Map routes retained");}
void LightSyncEngine::suspend(bool sleep)
{
    if(sleep && sleeping)return;
    if(!sleep && !sleeping && lastResume>=0 && clock.elapsed()-lastResume<2000)return;
    sleeping=sleep;
    if(sleep)
    {
        log("Suspend: releasing LAN and Windows Dynamic Lighting sessions");
        for(auto& d:devices){if(settings.offOnSuspend&&d->kind!="laptop")power(*d,false);release(*d);}
        if(lamps)lamps->release();
    }
    else
    {
        lastResume=clock.elapsed();
        log("Resume: reopening Windows Dynamic Lighting; preserving device identities and latest colors");
        // Windows can invalidate LampArray objects across hibernation even when
        // the device ID stays unchanged or the suspend message was missed.
        if(lamps)lamps->resume();
        lastKeyboardRecovery=clock.elapsed();
        scan();
        for(auto& d:devices){d->nextAttempt=clock.elapsed()+1500;if(settings.onOnResume)d->externallyOff=false;}
        for(int delay:{1500,5000,15000})QTimer::singleShot(delay,this,[this]{if(!stopped&&!sleeping)scan();});
    }
    publish();
}
void LightSyncEngine::recoverKeyboard(QString reason)
{
    if(stopped||sleeping||!settings.enabled||!settings.laptop||!lamps)return;
    if(lastKeyboardRecovery>=0&&clock.elapsed()-lastKeyboardRecovery<1000)return;
    lastKeyboardRecovery=clock.elapsed();
    log(reason+": releasing and rediscovering Lenovo Windows Dynamic Lighting");
    lamps->resume();scan();
    for(auto& d:devices)if(d->kind=="laptop")d->nextAttempt=clock.elapsed()+1500;
    for(int delay:{1500,5000,15000})QTimer::singleShot(delay,this,[this]{if(!stopped&&!sleeping)scan();});
    publish();
}
void LightSyncEngine::command(LightSyncDevice& d,QString method,QJsonArray params)
{
    QJsonObject json{{"id",commandId++},{"method",method},{"params",params}};
    auto bytes=QJsonDocument(json).toJson(QJsonDocument::Compact)+"\r\n";
    auto* socket=d.music&&d.music->state()==QAbstractSocket::ConnectedState?d.music:d.control;
    if(method=="set_scene" && (!d.music || socket!=d.music))return; // No high-rate normal TCP fallback.
    if(socket&&socket->state()==QAbstractSocket::ConnectedState)socket->write(bytes);
}
void LightSyncEngine::power(LightSyncDevice& d,bool on)
{
    if(d.kind=="govee")sender->writeDatagram(QJsonDocument(QJsonObject{{"msg",QJsonObject{{"cmd","turn"},{"data",QJsonObject{{"value",on?1:0}}}}}}).toJson(QJsonDocument::Compact),QHostAddress(d.ip),4003);
    else if(d.kind=="yeelight")
    {
        QTcpSocket socket;socket.connectToHost(d.ip,d.port);
        if(socket.waitForConnected(600)){socket.write(QJsonDocument(QJsonObject{{"id",1},{"method","set_power"},{"params",QJsonArray{on?"on":"off","smooth",100}}}).toJson(QJsonDocument::Compact)+"\r\n");socket.waitForBytesWritten(200);socket.disconnectFromHost();}
    }
}
void LightSyncEngine::release(LightSyncDevice& d)
{
    if(d.kind=="govee"&&d.streaming)
    {
        sender->writeDatagram(RazerPacket(QByteArray::fromHex("bb0001b100")),QHostAddress(d.ip),4003);
    }
    if(d.kind=="yeelight"&&(d.control||d.music))
    {
        if(d.music){d.music->disconnect(this);d.music->abort();d.music->deleteLater();d.music=nullptr;}
        QTcpSocket release;release.connectToHost(d.ip,d.port);
        if(release.waitForConnected(700))
        {
            release.write("{\"id\":1,\"method\":\"set_music\",\"params\":[0]}\r\n");release.waitForBytesWritten(200);
            bool acknowledged=false;QByteArray response;
            if(release.waitForReadyRead(700)){response=release.readAll();acknowledged=QJsonDocument::fromJson(response.trimmed()).object().contains("result");}
            log("Yeelight music release "+d.ip+(acknowledged?" acknowledged":" sent (no acknowledgement)"));release.disconnectFromHost();
        }else log("Yeelight music release unreachable: "+d.ip);
    }
    if(d.control){d.control->disconnect(this);d.control->abort();d.control->deleteLater();d.control=nullptr;}
    if(d.listener){d.listener->close();d.listener->deleteLater();d.listener=nullptr;}
    d.streaming=false;d.submitted=false;d.phase=0;d.controlRead.clear();d.nextAttempt=clock.elapsed()+1000;d.status="Control released";
}
void LightSyncEngine::govee(LightSyncDevice& d,const std::vector<RGBColor>& colors,int brightness,qint64 now)
{
    if(!d.streaming)
    {
        sender->writeDatagram(RazerPacket(QByteArray::fromHex("bb0001b101")),QHostAddress(d.ip),4003);d.streaming=true;d.phaseAt=now;d.previousBright=-1;return;
    }
    if(now-d.phaseAt<400)return;
    if(d.previousBright!=brightness&&now-d.phaseAt>=800)
    {
        sender->writeDatagram(QJsonDocument(QJsonObject{{"msg",QJsonObject{{"cmd","brightness"},{"data",QJsonObject{{"value",brightness}}}}}}).toJson(QJsonDocument::Compact),QHostAddress(d.ip),4003);d.previousBright=brightness;
    }
    if(now-d.lastSend<1000/std::max(1,settings.goveeFps))return;
    auto bytes=GoveePacket(colors);
    if(sender->writeDatagram(bytes,QHostAddress(d.ip),4003)==bytes.size()){d.sent=colors;d.lastSend=now;d.status=now-d.lastSeen>45000?"Streaming (discovery reply missing)":"Streaming";}
    else d.status=sender->errorString();
}
void LightSyncEngine::yeelight(LightSyncDevice& d,RGBColor rgb,int brightness,qint64 now)
{
    if(!d.music||d.music->state()!=QAbstractSocket::ConnectedState)
    {
        if(d.phase==0&&now>=d.nextAttempt)
        {
            if(d.control||d.listener)release(d);
            d.control=new QTcpSocket(this);d.listener=new QTcpServer(this);
            if(!d.listener->listen(d.local,0)){d.status=d.listener->errorString();release(d);d.nextAttempt=now+5000;return;}
            d.phase=1;d.phaseAt=now;d.status="Connecting";d.control->connectToHost(d.ip,d.port);
            connect(d.control,&QTcpSocket::connected,this,[this,&d]{command(d,"set_music",{0});d.phase=2;d.phaseAt=clock.elapsed();});
            connect(d.control,&QTcpSocket::readyRead,this,[this,&d]{
                if(!d.control)return;
                d.controlRead+=d.control->readAll();
                if(d.controlRead.size()>65536){d.controlRead.clear();return;}
                while(d.controlRead.contains('\n'))
                {
                    int end=d.controlRead.indexOf('\n');auto line=d.controlRead.left(end).trimmed();d.controlRead.remove(0,end+1);
                    auto reply=QJsonDocument::fromJson(line).object();
                    if(reply["method"].toString()=="props"&&reply["params"].toObject()["power"].toString()=="off"&&!d.dark)
                    {
                        release(d);d.externallyOff=true;d.status="Switched off externally; restart or toggle Control to resume";
                        log("Respecting external power off: "+d.ip);return;
                    }
                }
            });
            connect(d.listener,&QTcpServer::newConnection,this,[this,&d]{
                if(d.music){auto* extra=d.listener->nextPendingConnection();extra->deleteLater();return;}
                d.music=d.listener->nextPendingConnection();d.music->setParent(this);d.music->setSocketOption(QAbstractSocket::LowDelayOption,1);d.phase=4;d.failures=0;d.status="Connected";d.lastSend=0;
            });
        }
        if(d.phase==2&&now-d.phaseAt>=400){command(d,"set_music",{1,d.local.toString(),int(d.listener->serverPort())});d.phase=3;d.phaseAt=now;}
        if(d.phase&&now-d.phaseAt>5000){release(d);d.failures++;d.nextAttempt=now+std::min(30000,1500*d.failures);d.status="Reconnecting";}
        return;
    }
    if(d.music->bytesToWrite()>2048){d.status="LAN stream congested; dropping stale frame";return;}
    // Established music transport has no normal-TCP command quota.
    if(now-d.lastSend<1000/std::max(1,settings.yeelightFps))return;
    int r=RGBGetRValue(rgb),g=RGBGetGValue(rgb),b=RGBGetBValue(rgb),level=std::max({r,g,b});
    int cutoff=settings.darkBelow*255/100;
    bool dark=cutoff>0&&(d.dark?level<cutoff+std::max(4,cutoff/2):level<=cutoff);
    if(dark||brightness==0)
    {
        if(d.dark)return;
        if(d.fadeAt<0){d.fadeAt=now;d.fadeBright=d.previousBright;}
        if(now-d.fadeAt<settings.fadeMs)
        {
            int faded=std::max(1,int(d.fadeBright*(1.0-double(now-d.fadeAt)/std::max(1,settings.fadeMs))));
            command(d,"set_scene",{"color",int(RGBToBGRColor(d.previousColor)),faded});
        }else{command(d,"set_power",{"off","smooth",100});d.dark=true;}
        d.lastSend=now;return;
    }
    d.fadeAt=-1;int bright=std::clamp(int(std::round(level*brightness/255.0)),1,100);
    RGBColor normalized=level?ToRGBColor(r*255/level,g*255/level,b*255/level):d.previousColor;
    if(!d.dark&&normalized==d.previousColor&&bright==d.previousBright&&now-d.lastSend<10000)return;
    // set_scene applies the hue before waking the light at its intended brightness.
    command(d,"set_scene",{"color",int(RGBToBGRColor(normalized)),bright});d.dark=false;d.previousColor=normalized;d.previousBright=bright;
    d.lastSend=now;d.status="Streaming";
}
void LightSyncEngine::tick()
{
    if(stopped||sleeping||!settings.enabled)return;
    auto now=clock.elapsed();
    double dt=lastTick?std::min(1.0,(now-lastTick)/1000.0):0.016;lastTick=now;
    for(auto& node:devices)
    {
        auto& d=*node;if(!d.enabled||d.externallyOff||now<d.nextAttempt||(!settings.laptop&&d.kind=="laptop"))continue;
        std::vector<RGBColor> colors;int brightness;quint64 revision;
        {std::lock_guard<std::mutex> lock(d.mutex);colors=d.latest;brightness=d.brightness;revision=d.revision;}
        if(!revision)continue; // Discovery must never switch on an untouched device.
        if(!d.submitted){if(settings.onOnResume && d.kind=="govee"){power(d,true);d.nextAttempt=now+400;d.submitted=true;continue;}d.submitted=true;}
        if(settings.interpolate && d.kind!="laptop")
        {
            if(d.smooth.size()!=colors.size()*3){d.smooth.resize(colors.size()*3);for(size_t i=0;i<colors.size();i++){d.smooth[i*3]=RGBGetRValue(colors[i]);d.smooth[i*3+1]=RGBGetGValue(colors[i]);d.smooth[i*3+2]=RGBGetBValue(colors[i]);}}
            double alpha=1-std::exp(-dt/0.04);
            for(size_t i=0;i<colors.size();i++)
            {
                int channels[3]={int(RGBGetRValue(colors[i])),int(RGBGetGValue(colors[i])),int(RGBGetBValue(colors[i]))};
                for(int c=0;c<3;c++)d.smooth[i*3+c]+=(channels[c]-d.smooth[i*3+c])*alpha;
                colors[i]=ToRGBColor(int(std::round(d.smooth[i*3])),int(std::round(d.smooth[i*3+1])),int(std::round(d.smooth[i*3+2])));
            }
        }else d.smooth.clear();
        if(d.kind=="govee")govee(d,colors,brightness,now);
        else if(d.kind=="yeelight")yeelight(d,colors[0],brightness,now);
        else if(d.kind=="laptop"){d.status=lamps->send(d.id.mid(QString("laptop:").size()),colors,brightness);d.lastSend=now;}
    }
    static qint64 published=0;if(now-published>=1000){publish();published=now;}
}
void LightSyncEngine::publish()
{
    for(auto& d:devices){QString state=d->status.section(" | ",0,0);if(d->reportedStatus!=state){log(QString::fromStdString(d->setup.name)+": "+state);d->reportedStatus=state;}}
    QJsonArray rows;for(auto& d:devices)rows.append(QJsonObject{{"id",d->id},{"name",QString::fromStdString(d->setup.name)},{"ip",d->ip},{"model",d->model},{"leds",d->count},{"enabled",d->enabled},{"status",settings.enabled?d->status:"Plugin disabled; control released"},{"cache",d->discoveryData}});emit inventory(rows);
}
void LightSyncEngine::shutdown()
{
    stopped=true;if(timer)timer->stop();if(heartbeat)heartbeat->stop();
    for(auto& d:devices)release(*d);if(lamps)lamps->release();
    for(auto* socket:discovery)socket->close();
    // Keep controllers stable during restart/disable. Removal only happens on real plugin unload.
    for(auto& d:devices){api->UnregisterVirtualRGBController(d->controller);api->DeleteVirtualRGBController(d->controller);d->controller=nullptr;}
    for(auto& d:retired)if(d->controller){api->DeleteVirtualRGBController(d->controller);d->controller=nullptr;}
    retired.clear();devices.clear();delete lamps;lamps=nullptr;log("LightSync stopped; LAN sessions released");logfile.close();emit shutdownComplete();
}


