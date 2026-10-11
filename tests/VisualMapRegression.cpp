#include <QApplication>
#include <QTemporaryDir>
#include <QDir>
#include <QEventLoop>
#include <QTimer>
#include <atomic>
#include <thread>
#include <iostream>
#include <stdexcept>
#include "FakeHost.h"
#include "OpenRGBVisualMapPlugin.h"
#include "VirtualController.h"
#include "ui_VirtualControllerTab.h"
#include <QDesktopServices>
#include <QSignalMapper>
#include <QTabBar>
#include <QTreeView>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include "VirtualControllerTab.h"
#include "VisualMapSettingsManager.h"
#include "ZoneManager.h"
void Check(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
class VisualMapRegression { public: static int Run(int argc, char** argv); };
int VisualMapRegression::Run(int argc,char** argv) {
 QApplication app(argc,argv);
 try {
  QTemporaryDir dir; FakeHost api; api.directory=dir.path().toStdWString();
  FakeController physical; physical.state.name="Test light"; physical.state.vendor="Test"; physical.state.serial="123"; physical.state.location="HID: test";
  api.devices.push_back(&physical); OpenRGBVisualMapPlugin::api=&api; ZoneManager::Get()->UpdateControllerZones();
  VirtualControllerTab tab;
  auto* zone=tab.controller_zones.at(0); tab.virtual_controller->Add(zone); tab.AddActiveZone(zone);
  zone->settings.x=12.25; zone->settings.y=7.5; zone->settings.scale=2.0; zone->settings.reverse=true;
  tab.on_grid_Changed(); tab.Recreate();
  auto* restored=tab.virtual_controller->GetZones().at(0);
  Check(restored->settings.x==12.25 && restored->settings.y==7.5 && restored->settings.scale==2.0 && restored->settings.reverse,"Recreate lost edited map geometry");
  tab.PauseForDetection(); tab.Recreate();
  Check(tab.virtual_controller->GetZones().size()==1 && tab.virtual_controller->GetZones()[0]->settings.x==12.25,"Detection pause lost mapping");
  tab.virtual_controller->SetPostUpdateCallBack([](const QImage&){});
  std::atomic<bool> running{true};
  std::thread sender([&] { while(running) { tab.virtual_controller->DeviceUpdateLEDs(); std::this_thread::sleep_for(std::chrono::milliseconds(1)); } });
  for(int i=0;i<500;i++) { tab.virtual_controller->UpdateSize(24+i%45,24+i%45); if(i%20==0)tab.Recreate(); }
  running=false; sender.join();
  Check(VisualMapSettingsManager::SaveMap("regression",tab.active_state),"Map write failed");
  Check(VisualMapSettingsManager::LoadMap("regression")==tab.active_state,"Map roundtrip lost settings");
  QDir().mkpath(QString::fromStdWString((VisualMapSettingsManager::MapsFolder()/"blocked").wstring()));
  Check(!VisualMapSettingsManager::SaveMap("blocked",tab.active_state),"Failed map write reported success");
  Check(VisualMapSettingsManager::LoadMap("regression")==tab.active_state,"Failed write damaged previous map");
  tab.LoadFile("regression");
  tab.virtual_controller->GetZones()[0]->settings.x=17.0;
  tab.on_grid_Changed();
  QEventLoop wait; QTimer::singleShot(500,&wait,&QEventLoop::quit); wait.exec();
  Check(VisualMapSettingsManager::LoadMap("regression")["ctrl_zones"][0]["settings"]["x"]==17.0,"Named map edits were not autosaved");
  tab.ClearVmapAction();
  tab.Recreate();
  Check(tab.virtual_controller->GetZones().empty(),"Cleared map returned during Recreate");
  std::cout<<"PASS: live geometry preserved across Recreate and detection pause; 500 concurrent resize/recreate iterations; atomic map roundtrip and truthful write failure; debounced autosave; cleared map stays empty.\n";
  return 0;
 } catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<'\n';return 1; }
}

int main(int argc,char** argv) { return VisualMapRegression::Run(argc,argv); }
