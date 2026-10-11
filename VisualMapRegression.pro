include(OpenRGBVisualMapPlugin.pro)
TEMPLATE = app
CONFIG -= plugin
CONFIG += console
TARGET = VisualMapRegression
SOURCES += $$PWD/tests/VisualMapRegression.cpp
INCLUDEPATH += $$PWD/tests
