#-------------------------------------------------
#
# Project created by QtCreator 2014-11-25T17:45:20
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = TranscriberUI
TEMPLATE = app

INCLUDEPATH += $$PWD/..
INCLUDEPATH += $$PWD/../../ThirdParty/libsndfile/include

#LIBS += -L$$PWD/../build/$(Platform)/$(CFG) -lPticaGovorunBackendd
#LIBS += -L$$PWD/../build/x64/%{CurrentBuild:Name} -lPticaGovorunBackendd
win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../../build/x64/release -lPticaGovorunBackend
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/../../build/x64/debug -lPticaGovorunBackendd


#DEPENDPATH += $$PWD/../PticaGovorunBackend

SOURCES += main.cpp \
    TranscriberMainWindow.cpp \
    AudioSamplesWidget.cpp \
    TranscriberViewModel.cpp \
    AudioMarkupNavigatorDialog.cpp \
    FileWorkspaceWidget.cpp \
    AudioTranscriptionWidget.cpp

HEADERS  += \
    TranscriberMainWindow.h \
    AudioSamplesWidget.h \
    TranscriberViewModel.h \
    AudioMarkupNavigatorDialog.h \
    FileWorkspaceWidget.h \
    AudioTranscriptionWidget.h

FORMS    += \
    AudioSamplesWidget.ui \
    AudioMarkupNavigatorDialog.ui \
    FileWorkspaceWidget.ui \
    AudioTranscriptionWidget.ui
