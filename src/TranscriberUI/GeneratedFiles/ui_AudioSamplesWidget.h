/********************************************************************************
** Form generated from reading UI file 'AudioSamplesWidget.ui'
**
** Created by: Qt User Interface Compiler version 5.3.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_AUDIOSAMPLESWIDGET_H
#define UI_AUDIOSAMPLESWIDGET_H

#include <QtCore/QVariant>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_AudioSamplesWidget
{
public:

    void setupUi(QWidget *AudioSamplesWidget)
    {
        if (AudioSamplesWidget->objectName().isEmpty())
            AudioSamplesWidget->setObjectName(QStringLiteral("AudioSamplesWidget"));
        AudioSamplesWidget->resize(400, 300);

        retranslateUi(AudioSamplesWidget);

        QMetaObject::connectSlotsByName(AudioSamplesWidget);
    } // setupUi

    void retranslateUi(QWidget *AudioSamplesWidget)
    {
        AudioSamplesWidget->setWindowTitle(QApplication::translate("AudioSamplesWidget", "Form", 0));
    } // retranslateUi

};

namespace Ui {
    class AudioSamplesWidget: public Ui_AudioSamplesWidget {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_AUDIOSAMPLESWIDGET_H
