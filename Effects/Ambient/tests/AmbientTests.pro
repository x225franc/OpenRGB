QT += core gui widgets opengl core5compat
TEMPLATE = app
CONFIG += console c++17
CONFIG -= app_bundle
TARGET = AmbientTests
SOURCES += $$PWD/AmbientTests.cpp
PLUGIN_ROOT = $$clean_path($$PWD/../../..)
PLUGIN_BUILD = $$PLUGIN_ROOT/build/optimized
INCLUDEPATH += $$PLUGIN_ROOT $$PLUGIN_ROOT/Effects $$PLUGIN_ROOT/Effects/Ambient $$PLUGIN_ROOT/ScreenCapturer \
    $$PLUGIN_ROOT/OpenRGB $$PLUGIN_ROOT/OpenRGB/RGBController $$PLUGIN_ROOT/OpenRGB/dependencies/json \
    $$PLUGIN_BUILD/_intermediate_release/.ui
LIBS += $$files($$PLUGIN_BUILD/_intermediate_release/.obj/*.obj) -lws2_32 -lole32 -lOpenGL32 -luser32 -lgdi32

INCLUDEPATH += $$PLUGIN_ROOT/OpenRGB/qt
