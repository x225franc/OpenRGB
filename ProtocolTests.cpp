#include "LightSyncEngine.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QPluginLoader>
#include <iostream>
#include <stdexcept>
static void check(bool value,const char* label){if(!value)throw std::runtime_error(label);std::cout<<"PASS "<<label<<std::endl;}
int main(int argc,char** argv)
{
 QCoreApplication app(argc,argv);
 try {
  auto packet=LightSyncEngine::GoveePacket({ToRGBColor(255,0,0),ToRGBColor(0,255,0),ToRGBColor(0,0,255)});
  auto data=QJsonDocument::fromJson(packet).object()["msg"].toObject()["data"].toObject()["pt"].toString().toLatin1();
  auto frame=QByteArray::fromBase64(data);
  check(frame.toHex()=="bb000bb00103ff000000ff000000fffd","Govee RGB segment layout and checksum match independent known vector");
  check(LightSyncEngine::GoveePacket({}).isEmpty(),"Empty Govee frame rejected");
  check(LightSyncEngine::GoveePacket(std::vector<RGBColor>(81)).isEmpty(),"Oversized Govee frame rejected");
  auto bulb=LightSyncEngine::ParseYeelight("HTTP/1.1 200 OK\r\nID: 0x123\r\nLOCATION: yeelight://192.168.0.25:55443\r\nmodel: color\r\nname: Desk\r\n");
  check(bulb["id"]=="0x123"&&bulb["ip"]=="192.168.0.25"&&bulb["port"]==55443,"Yeelight discovery parses case-insensitive headers and endpoint");
  check(LightSyncEngine::ParseYeelight("HTTP/1.1 200 OK\r\nlocation: http://example.com\r\n").isEmpty(),"Unrelated SSDP packets rejected");
  QPluginLoader plugin(argc>1?QString::fromLocal8Bit(argv[1]):"OpenRGBLightSyncPlugin.dll");
  check(plugin.metaData()["MetaData"].toObject()["OpenRGBPluginAPIVersion"].toInt()==5 && !plugin.metaData()["MetaData"].toObject()["Id"].toString().isEmpty(),"Plugin metadata satisfies OpenRGB discovery requirements");
  check(plugin.load(),"Native plugin DLL loads with installed Qt ABI");
  auto* iface=qobject_cast<OpenRGBPluginInterface*>(plugin.instance());
  check(iface&&iface->GetPluginAPIVersion()==5&&iface->GetPluginInfo().Label=="LightSync","Plugin exports OpenRGB API 5 and LightSync tab");
  plugin.unload();return 0;
 }catch(std::exception const& e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
}


