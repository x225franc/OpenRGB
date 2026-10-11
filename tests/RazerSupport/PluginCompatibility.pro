QT += core gui widgets
TEMPLATE = app
CONFIG += console c++17 release
CONFIG -= debug_and_release
TARGET = PluginCompatibility
SOURCES += $$PWD/PluginCompatibility.cpp
INCLUDEPATH += $$PWD/../.. $$PWD/../../RGBController $$PWD/../../dependencies/json
