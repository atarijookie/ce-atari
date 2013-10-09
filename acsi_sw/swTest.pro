#-------------------------------------------------
#
# Project created by QtCreator 2013-07-04T13:07:02
#
#-------------------------------------------------

QT       += core gui

TARGET = acsiSwTest
TEMPLATE = app


SOURCES +=  main.cpp\
            mainwindow.cpp \
            cconusb.cpp \
            ccorethread.cpp \
            native/scsi.cpp \
    acsidatatrans.cpp

HEADERS  += mainwindow.h \
            cconusb.h \
            ccorethread.h \
            ftd2xx.h \
            global.h \
            datatypes.h \
            native/scsi.h \
            native/scsi_defs.h \
    acsidatatrans.h

FORMS    += mainwindow.ui
