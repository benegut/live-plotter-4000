######################################################################
# Automatically generated by qmake (3.1) Sat May 21 22:47:35 2022
######################################################################

QT += core gui widgets
TEMPLATE = app
TARGET = live-plotter-4000
INCLUDEPATH += ./ /opt/picoscope/include/

# You can make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# Please consult the documentation of the deprecated API in order to know
# how to port your code away from it.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

DEFINES += QCUSTOMPLOT_USE_LIBRARY

LIBS += -L. -lqcustomplot
LIBS += -L/opt/picoscope/lib -lps4000a
# Input
HEADERS += window.hpp plot.hpp
SOURCES += main.cpp window.cpp plot.cpp