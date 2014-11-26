#-------------------------------------------------
#
# Project created by QtCreator 2014-11-25T17:45:20
#
#-------------------------------------------------

QT       -= gui
QT       += core

TARGET = PticaGovorunBackend
TEMPLATE = lib

DEFINES += PGAPI_EXPORTS

INCLUDEPATH += $$PWD/../../ThirdParty/libsndfile/include
LIBS += -L$$PWD/../../ThirdParty/libsndfile/lib/x64  -llibsndfile-1


SOURCES += \
    SoundUtils.cpp
HEADERS  += \
    SoundUtils.h \
    stdafx.h 

