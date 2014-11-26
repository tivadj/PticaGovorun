#include "transcribermainwindow.h"
#include "ui_transcribermainwindow.h"

TranscriberMainWindow::TranscriberMainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::TranscriberMainWindow)
{
    ui->setupUi(this);

    QObject::connect(ui->pushButtonLoadFileName, SIGNAL(clicked()), this, SLOT(pushButtonLoad_Clicked()));
    QObject::connect(ui->lineEditFileName, SIGNAL(editingFinished()), this, SLOT(lineEditFileName_editingFinished()));

	//
    transcriberModel_ = std::make_shared<TranscriberViewModel>();

    QObject::connect(transcriberModel_.get(), SIGNAL(nextNotification(const QString&)), this, SLOT(transcriberModel_nextNotification(const QString&)));
	QObject::connect(transcriberModel_.get(), SIGNAL(audioSamplesChanged()), this, SLOT(transcriberModel_audioSamplesChanged()));

	//
	ui->widgetSamples->setModel(transcriberModel_);
	updateUI();
}

TranscriberMainWindow::~TranscriberMainWindow()
{
    delete ui;
}

void TranscriberMainWindow::updateUI()
{
    ui->lineEditFileName->setText(transcriberModel_->audioFilePath());
}

void TranscriberMainWindow::transcriberModel_nextNotification(const QString& message)
{
	ui->textEditLogger->insertPlainText(message);
}

void TranscriberMainWindow::transcriberModel_audioSamplesChanged()
{
	ui->widgetSamples->update();
}

void TranscriberMainWindow::pushButtonLoad_Clicked()
{
    transcriberModel_->loadAudioFile();
}

void TranscriberMainWindow::lineEditFileName_editingFinished()
{
    transcriberModel_->setAudioFilePath(ui->lineEditFileName->text());
}
