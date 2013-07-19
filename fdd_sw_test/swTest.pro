#-------------------------------------------------
#
# Project created by QtCreator 2013-07-04T13:07:02
#
#-------------------------------------------------

QT       += core gui

TARGET = swTest
TEMPLATE = app


SOURCES +=  main.cpp\
            mainwindow.cpp \
            cconusb.cpp \
            ccorethread.cpp \
            floppyimagest.cpp \
            floppyimagemsa.cpp \
    floppyimagefactory.cpp \
    mfmcachedimage.cpp

HEADERS  += mainwindow.h \
            cconusb.h \
            ccorethread.h \
            ftd2xx.h \
            ifloppyimage.h \
            floppyimagest.h \
            floppyimagemsa.h \
            global.h \
            datatypes.h \
    floppyimagefactory.h \
    mfmcachedimage.h

FORMS    += mainwindow.ui
