#include "LightSyncEngine.h"
#include "LampArrayRecovery.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QPluginLoader>
#include <iostream>
#include <stdexcept>
struct LightSyncEngineTestAccess
{
 static void seed(LightSyncEngine& e,std::shared_ptr<LightSyncDevice> d){e.clock.start();e.devices.push_back(d);}
 static void found(LightSyncEngine& e,QJsonObject d){e.found(d);}
 static size_t count(LightSyncEngine& e){return e.devices.size();}
 static size_t retained(LightSyncEngine& e){return e.retired.size();}
 static bool ignored(LightSyncEngine& e,QString id){return e.forgotten.contains(id);}
};
static void check(bool value,const char* label){if(!value)throw std::runtime_error(label);std::cout<<"PASS "<<label<<std::endl;}
int main(int argc,char** argv)
{
 QCoreApplication app(argc,argv);
 try {
  LampArrayRecovery recovery;
  const auto now=LampArrayRecovery::Clock::now();
  check(!recovery.NeedsReopen(false,now+std::chrono::hours(1)),"Normal priority loss never starts an ownership-recovery loop");
  recovery.Begin(now);
  check(!recovery.NeedsReopen(false,now+std::chrono::seconds(30)),"Windows has 45 seconds to grant post-resume control");
  check(recovery.NeedsReopen(false,now+std::chrono::seconds(45)),"Stale post-resume session is reopened after the arbitration grace period");
  recovery.Reopened(now+std::chrono::seconds(45));
  check(!recovery.NeedsReopen(false,now+std::chrono::seconds(46)),"Session reopening is throttled instead of attempted every frame");
  check(!recovery.NeedsReopen(true,now+std::chrono::seconds(50))&&!recovery.NeedsReopen(false,now+std::chrono::seconds(51)),"Transient foreground-only control resets the unavailable grace period");
  check(recovery.NeedsReopen(false,now+std::chrono::seconds(96)),"Foreground-only availability still allows recovery of background ownership");
  check(!recovery.NeedsReopen(false,now+std::chrono::minutes(4)),"Ownership retries stop after the three-minute recovery window");
  recovery.Begin(now+std::chrono::minutes(5));
  check(recovery.NeedsReopen(false,now+std::chrono::seconds(345)),"A second hibernation arms a new bounded recovery window");
  recovery.Failed(now);
  check(!recovery.NeedsReopen(false,now+std::chrono::seconds(1))&&recovery.NeedsReopen(false,now+std::chrono::seconds(2)),"Invalid Windows lighting object retries after bounded backoff");
  auto packet=LightSyncEngine::GoveePacket({ToRGBColor(255,0,0),ToRGBColor(0,255,0),ToRGBColor(0,0,255)});
  auto data=QJsonDocument::fromJson(packet).object()["msg"].toObject()["data"].toObject()["pt"].toString().toLatin1();
  auto frame=QByteArray::fromBase64(data);
  check(frame.toHex()=="bb000bb00103ff000000ff000000fffd","Govee RGB segment layout and checksum match independent known vector");
  check(LightSyncEngine::GoveePacket({}).isEmpty(),"Empty Govee frame rejected");
  check(LightSyncEngine::GoveePacket(std::vector<RGBColor>(81)).isEmpty(),"Oversized Govee frame rejected");
  auto bulb=LightSyncEngine::ParseYeelight("HTTP/1.1 200 OK\r\nID: 0x123\r\nLOCATION: yeelight://192.168.0.25:55443\r\nmodel: color\r\nname: Desk\r\n");
  check(bulb["id"]=="0x123"&&bulb["ip"]=="192.168.0.25"&&bulb["port"]==55443,"Yeelight discovery parses case-insensitive headers and endpoint");
  check(LightSyncEngine::ParseYeelight("HTTP/1.1 200 OK\r\nlocation: http://example.com\r\n").isEmpty(),"Unrelated SSDP packets rejected");
  {
   LightSyncEngine engine(nullptr,QString());
   auto node=std::make_shared<LightSyncDevice>();node->id="yeelight:test";node->kind="yeelight";node->ip="192.168.0.25";node->local=QHostAddress("192.168.0.2");node->revision=7;node->latest={ToRGBColor(12,34,56)};
   LightSyncEngineTestAccess::seed(engine,node);
   QJsonArray inventory;QObject::connect(&engine,&LightSyncEngine::inventory,[&](QJsonArray rows){inventory=rows;});
   QJsonObject endpoint{{"kind","yeelight"},{"id","test"},{"ip","192.168.1.25"},{"local","192.168.1.2"},{"port",55544}};
   LightSyncEngineTestAccess::found(engine,endpoint);
   check(LightSyncEngineTestAccess::count(engine)==1&&node->ip=="192.168.1.25"&&node->port==55544&&node->local==QHostAddress("192.168.1.2"),"Router rediscovery updates IP, port and local interface without duplicating identity");
   check(node->revision==7&&node->latest==std::vector<RGBColor>{ToRGBColor(12,34,56)},"Endpoint replacement retains assigned color scene");
   engine.changeAddress(node->id,"239.255.255.250");check(node->ip=="192.168.1.25","Multicast destination rejected for manual device address");
   engine.forgetDevice(node->id);
   check(inventory.isEmpty()&&LightSyncEngineTestAccess::count(engine)==0&&LightSyncEngineTestAccess::retained(engine)==1,"Forget removes saved inventory while retaining callback storage until unload");
   LightSyncEngineTestAccess::found(engine,endpoint);
   check(LightSyncEngineTestAccess::count(engine)==0&&LightSyncEngineTestAccess::ignored(engine,node->id),"Background discovery cannot immediately resurrect a forgotten device");
   LightSyncSettings disabled;disabled.enabled=false;engine.configure(disabled);engine.searchAgain();
   check(!LightSyncEngineTestAccess::ignored(engine,node->id),"Explicit Search again permits rediscovery");
  }
  QPluginLoader plugin(argc>1?QString::fromLocal8Bit(argv[1]):"OpenRGBLightSyncPlugin.dll");
  check(plugin.metaData()["MetaData"].toObject()["OpenRGBPluginAPIVersion"].toInt()==5 && !plugin.metaData()["MetaData"].toObject()["Id"].toString().isEmpty(),"Plugin metadata satisfies OpenRGB discovery requirements");
  check(plugin.load(),"Native plugin DLL loads with installed Qt ABI");
  auto* iface=qobject_cast<OpenRGBPluginInterface*>(plugin.instance());
  check(iface&&iface->GetPluginAPIVersion()==5&&iface->GetPluginInfo().Label=="LightSync","Plugin exports OpenRGB API 5 and LightSync tab");
  check(iface->GetPluginInfo().Icon.size()==QSize(256,256),"Standalone LightSync icon is embedded and decodes correctly");plugin.unload();return 0;
 }catch(std::exception const& e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
}


