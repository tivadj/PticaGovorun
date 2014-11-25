/********************************************************************************
** Form generated from reading UI file 'transcribermainwindow.ui'
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
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_TranscriberMainWindow
{
public:
    QMenuBar *menuBar;
    QToolBar *mainToolBar;
    QWidget *centralWidget;
    QStatusBar *statusBar;

    void setupUi(QMainWindow *TranscriberMainWindow)
    {
        if (TranscriberMainWindow->objectName().isEmpty())
            TranscriberMainWindow->setObjectName(QStringLiteral("TranscriberMainWindow"));
        TranscriberMainWindow->resize(400, 300);
        menuBar = new QMenuBar(TranscriberMainWindow);
        menuBar->setObjectName(QStringLiteral("menuBar"));
        TranscriberMainWindow->setMenuBar(menuBar);
        mainToolBar = new QToolBar(TranscriberMainWindow);
        mainToolBar->setObjectName(QStringLiteral("mainToolBar"));
        TranscriberMainWindow->addToolBar(mainToolBar);
        centralWidget = new QWidget(TranscriberMainWindow);
        centralWidget->setObjectName(QStringLiteral("centralWidget"));
        TranscriberMainWindow->setCentralWidget(centralWidget);
        statusBar = new QStatusBar(TranscriberMainWindow);
        statusBar->setObjectName(QStringLiteral("statusBar"));
        TranscriberMainWindow->setStatusBar(statusBar);

        retranslateUi(TranscriberMainWindow);

        QMetaObject::connectSlotsByName(TranscriberMainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *TranscriberMainWindow)
    {
        TranscriberMainWindow->setWindowTitle(QApplication::translate("TranscriberMainWindow", "TranscriberMainWindow", 0));
    } // retranslateUi

};

namespace Ui {
    class TranscriberMainWindow: public Ui_TranscriberMainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_TRANSCRIBERMAINWINDOW_H
