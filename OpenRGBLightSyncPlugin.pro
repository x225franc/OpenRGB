QT += core gui widgets network
TEMPLATE = lib
CONFIG += plugin c++20
TARGET = OpenRGBLightSyncPlugin
CONFIG -= debug_and_release
OPENRGB_ROOT = $$clean_path($$PWD/../..)
INCLUDEPATH += $$OPENRGB_ROOT $$OPENRGB_ROOT/RGBController $$OPENRGB_ROOT/dependencies/json
HEADERS += LightSyncPlugin.h LightSyncEngine.h LampArrayBackend.h
SOURCES += LightSyncPlugin.cpp LightSyncEngine.cpp LampArrayBackend.cpp
win32 {
 DEFINES += WIN32_LEAN_AND_MEAN NOMINMAX
 LIBS += -lwindowsapp -lpowrprof -luser32
 QMAKE_CXXFLAGS += /permissive- /bigobj
}



