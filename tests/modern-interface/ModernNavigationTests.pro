QT += core gui widgets testlib
CONFIG += console testcase c++17
CONFIG -= app_bundle debug_and_release
TEMPLATE = app
TARGET = ModernNavigationTests
INCLUDEPATH += ../../qt/ModernInterface
SOURCES += ModernNavigationTests.cpp ../../qt/ModernInterface/ModernNavigation.cpp ../../qt/ModernInterface/ModernIcons.cpp
HEADERS += ../../qt/ModernInterface/ModernNavigation.h

RESOURCES += ../../qt/resources.qrc
