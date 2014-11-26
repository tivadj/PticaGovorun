/********************************************************************************
** Form generated from reading UI file 'TranscriberMainWindow.ui'
**
** Created by: Qt User Interface Compiler version 5.3.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_TRANSCRIBERMAINWINDOW_H
#define UI_TRANSCRIBERMAINWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QWidget>
#include "AudioSamplesWidget.h"

QT_BEGIN_NAMESPACE

class Ui_TranscriberMainWindow
{
public:
    QWidget *centralWidget;
    QPushButton *pushButtonLoadFileName;
    QLineEdit *lineEditFileName;
    QScrollBar *horizontalScrollBarSamples;
    AudioSamplesWidget *widgetSamples;
    QTextEdit *textEditLogger;
    QMenuBar *menuBar;
    QToolBar *mainToolBar;
    QStatusBar *statusBar;

    void setupUi(QMainWindow *TranscriberMainWindow)
    {
        if (TranscriberMainWindow->objectName().isEmpty())
            TranscriberMainWindow->setObjectName(QStringLiteral("TranscriberMainWindow"));
        TranscriberMainWindow->resize(735, 528);
        centralWidget = new QWidget(TranscriberMainWindow);
        centralWidget->setObjectName(QStringLiteral("centralWidget"));
        pushButtonLoadFileName = new QPushButton(centralWidget);
        pushButtonLoadFileName->setObjectName(QStringLiteral("pushButtonLoadFileName"));
        pushButtonLoadFileName->setGeometry(QRect(10, 10, 75, 23));
        lineEditFileName = new QLineEdit(centralWidget);
        lineEditFileName->setObjectName(QStringLiteral("lineEditFileName"));
        lineEditFileName->setGeometry(QRect(90, 10, 191, 20));
        horizontalScrollBarSamples = new QScrollBar(centralWidget);
        horizontalScrollBarSamples->setObjectName(QStringLiteral("horizontalScrollBarSamples"));
        horizontalScrollBarSamples->setGeometry(QRect(10, 50, 711, 16));
        horizontalScrollBarSamples->setOrientation(Qt::Horizontal);
        widgetSamples = new AudioSamplesWidget(centralWidget);
        widgetSamples->setObjectName(QStringLiteral("widgetSamples"));
        widgetSamples->setGeometry(QRect(10, 70, 711, 291));
        textEditLogger = new QTextEdit(centralWidget);
        textEditLogger->setObjectName(QStringLiteral("textEditLogger"));
        textEditLogger->setGeometry(QRect(0, 430, 701, 41));
        textEditLogger->setReadOnly(true);
        TranscriberMainWindow->setCentralWidget(centralWidget);
        menuBar = new QMenuBar(TranscriberMainWindow);
        menuBar->setObjectName(QStringLiteral("menuBar"));
        menuBar->setGeometry(QRect(0, 0, 735, 21));
        TranscriberMainWindow->setMenuBar(menuBar);
        mainToolBar = new QToolBar(TranscriberMainWindow);
        mainToolBar->setObjectName(QStringLiteral("mainToolBar"));
        TranscriberMainWindow->addToolBar(Qt::TopToolBarArea, mainToolBar);
        statusBar = new QStatusBar(TranscriberMainWindow);
        statusBar->setObjectName(QStringLiteral("statusBar"));
        TranscriberMainWindow->setStatusBar(statusBar);

        retranslateUi(TranscriberMainWindow);

        QMetaObject::connectSlotsByName(TranscriberMainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *TranscriberMainWindow)
    {
        TranscriberMainWindow->setWindowTitle(QApplication::translate("TranscriberMainWindow", "TranscriberMainWindow", 0));
        pushButtonLoadFileName->setText(QApplication::translate("TranscriberMainWindow", "Load", 0));
        lineEditFileName->setText(QApplication::translate("TranscriberMainWindow", "file-name.wav", 0));
        textEditLogger->setHtml(QApplication::translate("TranscriberMainWindow", "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0//EN\" \"http://www.w3.org/TR/REC-html40/strict.dtd\">\n"
"<html><head><meta name=\"qrichtext\" content=\"1\" /><style type=\"text/css\">\n"
"p, li { white-space: pre-wrap; }\n"
"</style></head><body style=\" font-family:'MS Shell Dlg 2'; font-size:8.25pt; font-weight:400; font-style:normal;\">\n"
"<p style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">a</p>\n"
"<p style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">b</p>\n"
"<p style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">c</p></body></html>", 0));
    } // retranslateUi

};

namespace Ui {
    class TranscriberMainWindow: public Ui_TranscriberMainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_TRANSCRIBERMAINWINDOW_H
