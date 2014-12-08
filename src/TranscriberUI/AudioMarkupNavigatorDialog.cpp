#include "AudioMarkupNavigatorDialog.h"
#include "ui_AudioMarkupNavigatorDialog.h"
#include <QShowEvent>

AudioMarkupNavigatorDialog::AudioMarkupNavigatorDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AudioMarkupNavigatorDialog)
{
    ui->setupUi(this);
}

AudioMarkupNavigatorDialog::~AudioMarkupNavigatorDialog()
{
    delete ui;
}

void AudioMarkupNavigatorDialog::showEvent(QShowEvent*)
{
	ui->lineEditMarkerId->setFocus();
}

int AudioMarkupNavigatorDialog::markerId() const
{
	bool parseIntOp;
	int markerId = ui->lineEditMarkerId->text().toInt(&parseIntOp);
	return parseIntOp ? markerId : -1;
}

