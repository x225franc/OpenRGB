QT += core gui widgets
TEMPLATE = lib
CONFIG += plugin c++17
CONFIG -= debug_and_release
TARGET = OpenRGBUiReview
INCLUDEPATH += ../.. ../../RGBController ../../dependencies/json
HEADERS += ReviewPlugin.h
SOURCES += ReviewPlugin.cpp
