#include <sstream>
#include "transcribermainwindow.h"
#include "ui_transcribermainwindow.h"
#include <QDebug>
#include <QMouseEvent>

TranscriberMainWindow::TranscriberMainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::TranscriberMainWindow)
{
    ui->setupUi(this);

    QObject::connect(ui->pushButtonLoadFileName, SIGNAL(clicked()), this, SLOT(pushButtonLoad_Clicked()));
    QObject::connect(ui->lineEditFileName, SIGNAL(editingFinished()), this, SLOT(lineEditFileName_editingFinished()));
    QObject::connect(ui->horizontalScrollBarSamples, SIGNAL(valueChanged(int)), this, SLOT(horizontalScrollBarSamples_valueChanged(int)));
	QObject::connect(ui->pushButtonPlay, SIGNAL(clicked()), this, SLOT(pushButtonPlay_Clicked()));
	QObject::connect(ui->pushButtonPause, SIGNAL(clicked()), this, SLOT(pushButtonPause_Clicked()));
	

	//
    transcriberModel_ = std::make_shared<TranscriberViewModel>();

	QObject::connect(transcriberModel_.get(), SIGNAL(audioSamplesLoaded()), this, SLOT(transcriberModel_audioSamplesLoaded()));
    QObject::connect(transcriberModel_.get(), SIGNAL(nextNotification(const QString&)), this, SLOT(transcriberModel_nextNotification(const QString&)));
	QObject::connect(transcriberModel_.get(), SIGNAL(audioSamplesChanged()), this, SLOT(transcriberModel_audioSamplesChanged()));
	QObject::connect(transcriberModel_.get(), SIGNAL(docOffsetXChanged()), this, SLOT(transcriberModel_docOffsetXChanged()));
	QObject::connect(transcriberModel_.get(), SIGNAL(lastMouseDocPosXChanged(float)), this, SLOT(transcriberModel_lastMouseDocPosXChanged(float)));
	QObject::connect(transcriberModel_.get(), SIGNAL(currentFrameIndChanged()), this, SLOT(transcriberModel_currentFrameIndChanged()));

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

void TranscriberMainWindow::transcriberModel_audioSamplesLoaded()
{
	ui->widgetSamples->setFocus(Qt::OtherFocusReason);
}

void TranscriberMainWindow::transcriberModel_nextNotification(const QString& message)
{
	ui->textEditLogger->insertPlainText(message);
}

// called on:
// audioSamples changed
// docOffsetX changed
void TranscriberMainWindow::transcriberModel_audioSamplesChanged()
{
	ui->widgetSamples->update();

	updateSamplesSlider();
}

void TranscriberMainWindow::transcriberModel_docOffsetXChanged()
{
	ui->widgetSamples->update();

	updateSamplesSlider();
}

void TranscriberMainWindow::updateSamplesSlider()
{
	ui->horizontalScrollBarSamples->setMinimum(0);

	//
	int docWidth = transcriberModel_->docWidthPix();
	//int pageStep = (int)(ui->widgetSamples->width() / transcriberModel_->pixelsPerSample());
	int pageStep = ui->widgetSamples->width();

	int slideMax = docWidth;
	slideMax -= pageStep/2;
	ui->horizontalScrollBarSamples->setMaximum(slideMax);

	//const int stepsPerScreen = 10; // need x of steps to pass the page
	//ui->horizontalScrollBarSamples->setSingleStep(pageStep / stepsPerScreen);
	ui->horizontalScrollBarSamples->setSingleStep(100);
	ui->horizontalScrollBarSamples->setPageStep(pageStep);
}


void TranscriberMainWindow::pushButtonLoad_Clicked()
{
    transcriberModel_->loadAudioFile();
}

void TranscriberMainWindow::pushButtonPlay_Clicked()
{
    transcriberModel_->soundPlayerPlay();
}

void TranscriberMainWindow::pushButtonPause_Clicked()
{
	transcriberModel_->soundPlayerPause();
}

void TranscriberMainWindow::lineEditFileName_editingFinished()
{
    transcriberModel_->setAudioFilePath(ui->lineEditFileName->text());
}


void TranscriberMainWindow::horizontalScrollBarSamples_valueChanged(int value)
{
    //qDebug() <<value <<" bar.value="  <<ui->horizontalScrollBarSamples->value();
    transcriberModel_->setDocOffsetX(value);
}

void TranscriberMainWindow::transcriberModel_lastMouseDocPosXChanged(float mouseDocPosX)
{
	{
		std::stringstream msg;
		msg.precision(2);
		msg << std::fixed;
		msg << mouseDocPosX;
		ui->lineEditCurDocPosX->setText(QString::fromStdString(msg.str()));

		float sampleInd = transcriberModel_->docPosXToSampleInd(mouseDocPosX);

		msg.str("");
		//std::stringstream msg;
		//msg.precision(2);
		//msg << std::fixed;
		//msg.str("");
		msg << sampleInd;
		ui->lineEditCurSampleInd->setText(QString::fromStdString(msg.str()));
	}
}

void TranscriberMainWindow::transcriberModel_currentFrameIndChanged()
{
	ui->widgetSamples->update();
}