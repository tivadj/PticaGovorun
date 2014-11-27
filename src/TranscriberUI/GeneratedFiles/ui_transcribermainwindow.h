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
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>
#include "AudioSamplesWidget.h"

QT_BEGIN_NAMESPACE

class Ui_TranscriberMainWindow
{
public:
    QWidget *centralWidget;
    QVBoxLayout *verticalLayout_2;
    QWidget *widgetUpperPanel;
    QPushButton *pushButtonLoadFileName;
    QLineEdit *lineEditFileName;
    QLineEdit *lineEditCurDocPosX;
    QLineEdit *lineEditCurSampleInd;
    QLabel *label;
    QLabel *label_2;
    QWidget *widgetSamplesMain;
    QVBoxLayout *verticalLayout_3;
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
        TranscriberMainWindow->resize(781, 596);
        centralWidget = new QWidget(TranscriberMainWindow);
        centralWidget->setObjectName(QStringLiteral("centralWidget"));
        verticalLayout_2 = new QVBoxLayout(centralWidget);
        verticalLayout_2->setSpacing(6);
        verticalLayout_2->setContentsMargins(11, 11, 11, 11);
        verticalLayout_2->setObjectName(QStringLiteral("verticalLayout_2"));
        widgetUpperPanel = new QWidget(centralWidget);
        widgetUpperPanel->setObjectName(QStringLiteral("widgetUpperPanel"));
        QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(widgetUpperPanel->sizePolicy().hasHeightForWidth());
        widgetUpperPanel->setSizePolicy(sizePolicy);
        widgetUpperPanel->setMinimumSize(QSize(0, 50));
        pushButtonLoadFileName = new QPushButton(widgetUpperPanel);
        pushButtonLoadFileName->setObjectName(QStringLiteral("pushButtonLoadFileName"));
        pushButtonLoadFileName->setGeometry(QRect(10, 10, 75, 23));
        pushButtonLoadFileName->setDefault(true);
        lineEditFileName = new QLineEdit(widgetUpperPanel);
        lineEditFileName->setObjectName(QStringLiteral("lineEditFileName"));
        lineEditFileName->setGeometry(QRect(90, 10, 391, 20));
        lineEditCurDocPosX = new QLineEdit(widgetUpperPanel);
        lineEditCurDocPosX->setObjectName(QStringLiteral("lineEditCurDocPosX"));
        lineEditCurDocPosX->setGeometry(QRect(670, 0, 81, 20));
        lineEditCurSampleInd = new QLineEdit(widgetUpperPanel);
        lineEditCurSampleInd->setObjectName(QStringLiteral("lineEditCurSampleInd"));
        lineEditCurSampleInd->setGeometry(QRect(670, 20, 81, 20));
        label = new QLabel(widgetUpperPanel);
        label->setObjectName(QStringLiteral("label"));
        label->setGeometry(QRect(620, 0, 46, 13));
        label_2 = new QLabel(widgetUpperPanel);
        label_2->setObjectName(QStringLiteral("label_2"));
        label_2->setGeometry(QRect(610, 20, 51, 20));

        verticalLayout_2->addWidget(widgetUpperPanel);

        widgetSamplesMain = new QWidget(centralWidget);
        widgetSamplesMain->setObjectName(QStringLiteral("widgetSamplesMain"));
        QSizePolicy sizePolicy1(QSizePolicy::Preferred, QSizePolicy::Expanding);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(widgetSamplesMain->sizePolicy().hasHeightForWidth());
        widgetSamplesMain->setSizePolicy(sizePolicy1);
        verticalLayout_3 = new QVBoxLayout(widgetSamplesMain);
        verticalLayout_3->setSpacing(6);
        verticalLayout_3->setContentsMargins(11, 11, 11, 11);
        verticalLayout_3->setObjectName(QStringLiteral("verticalLayout_3"));
        horizontalScrollBarSamples = new QScrollBar(widgetSamplesMain);
        horizontalScrollBarSamples->setObjectName(QStringLiteral("horizontalScrollBarSamples"));
        horizontalScrollBarSamples->setMinimum(0);
        horizontalScrollBarSamples->setOrientation(Qt::Horizontal);

        verticalLayout_3->addWidget(horizontalScrollBarSamples);

        widgetSamples = new AudioSamplesWidget(widgetSamplesMain);
        widgetSamples->setObjectName(QStringLiteral("widgetSamples"));
        sizePolicy1.setHeightForWidth(widgetSamples->sizePolicy().hasHeightForWidth());
        widgetSamples->setSizePolicy(sizePolicy1);

        verticalLayout_3->addWidget(widgetSamples);

        widgetSamples->raise();
        horizontalScrollBarSamples->raise();

        verticalLayout_2->addWidget(widgetSamplesMain);

        textEditLogger = new QTextEdit(centralWidget);
        textEditLogger->setObjectName(QStringLiteral("textEditLogger"));
        QSizePolicy sizePolicy2(QSizePolicy::Expanding, QSizePolicy::Fixed);
        sizePolicy2.setHorizontalStretch(0);
        sizePolicy2.setVerticalStretch(0);
        sizePolicy2.setHeightForWidth(textEditLogger->sizePolicy().hasHeightForWidth());
        textEditLogger->setSizePolicy(sizePolicy2);
        textEditLogger->setMaximumSize(QSize(16777215, 70));
        textEditLogger->setBaseSize(QSize(0, 0));
        textEditLogger->setReadOnly(true);

        verticalLayout_2->addWidget(textEditLogger);

        TranscriberMainWindow->setCentralWidget(centralWidget);
        widgetSamplesMain->raise();
        textEditLogger->raise();
        widgetUpperPanel->raise();
        menuBar = new QMenuBar(TranscriberMainWindow);
        menuBar->setObjectName(QStringLiteral("menuBar"));
        menuBar->setGeometry(QRect(0, 0, 781, 21));
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
        lineEditCurDocPosX->setText(QApplication::translate("TranscriberMainWindow", "###", 0));
        lineEditCurSampleInd->setText(QApplication::translate("TranscriberMainWindow", "###", 0));
        label->setText(QApplication::translate("TranscriberMainWindow", "DocPosX", 0));
        label_2->setText(QApplication::translate("TranscriberMainWindow", "SampleInd", 0));
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
