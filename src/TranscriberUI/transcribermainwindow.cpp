#include <sstream>
#include "transcribermainwindow.h"
#include "ui_transcribermainwindow.h"
#include <QDebug>
#include <QMouseEvent>
#include "SoundUtils.h"

TranscriberMainWindow::TranscriberMainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::TranscriberMainWindow)
{
    ui->setupUi(this);

    QObject::connect(ui->pushButtonLoadFileName, SIGNAL(clicked()), this, SLOT(pushButtonLoad_Clicked()));
	QObject::connect(ui->pushButtonSaveAudioAnnot, SIGNAL(clicked()), this, SLOT(pushButtonSaveAudioAnnot_Clicked()));
	QObject::connect(ui->pushButtonSegmentComposerPlay, SIGNAL(clicked()), this, SLOT(pushButtonSegmentComposerPlay_Clicked()));
    QObject::connect(ui->lineEditFileName, SIGNAL(editingFinished()), this, SLOT(lineEditFileName_editingFinished()));
	QObject::connect(ui->lineEditRecognizerName, SIGNAL(editingFinished()), this, SLOT(lineEditRecognizerName_editingFinished()));
    QObject::connect(ui->horizontalScrollBarSamples, SIGNAL(valueChanged(int)), this, SLOT(horizontalScrollBarSamples_valueChanged(int)));
	QObject::connect(ui->pushButtonPlay, SIGNAL(clicked()), this, SLOT(pushButtonPlay_Clicked()));
	QObject::connect(ui->pushButtonPause, SIGNAL(clicked()), this, SLOT(pushButtonPause_Clicked()));
	QObject::connect(ui->radioButtonWordLevel, SIGNAL(toggled(bool)), this, SLOT(radioButtonWordLevel_toggled(bool)));
	QObject::connect(ui->lineEditMarkerText, SIGNAL(editingFinished()), this, SLOT(lineEditMarkerText_editingFinished()));
	QObject::connect(ui->checkBoxCurMarkerStopOnPlayback, SIGNAL(toggled(bool)), this, SLOT(checkBoxCurMarkerStopOnPlayback_toggled(bool)));

	//
    transcriberModel_ = std::make_shared<TranscriberViewModel>();
	transcriberModel_->setAudioMarkupNavigator(std::make_shared<AudioMarkupNavigator>());

	QObject::connect(transcriberModel_.get(), SIGNAL(audioSamplesLoaded()), this, SLOT(transcriberModel_audioSamplesLoaded()));
    QObject::connect(transcriberModel_.get(), SIGNAL(nextNotification(const QString&)), this, SLOT(transcriberModel_nextNotification(const QString&)));
	QObject::connect(transcriberModel_.get(), SIGNAL(audioSamplesChanged()), this, SLOT(transcriberModel_audioSamplesChanged()));
	QObject::connect(transcriberModel_.get(), SIGNAL(docOffsetXChanged()), this, SLOT(transcriberModel_docOffsetXChanged()));
	QObject::connect(transcriberModel_.get(), SIGNAL(lastMouseDocPosXChanged(float)), this, SLOT(transcriberModel_lastMouseDocPosXChanged(float)));
	QObject::connect(transcriberModel_.get(), SIGNAL(currentFrameIndChanged(long)), this, SLOT(transcriberModel_currentFrameIndChanged(long)));
	QObject::connect(transcriberModel_.get(), SIGNAL(currentMarkerIndChanged()), this, SLOT(transcriberModel_currentMarkerIndChanged()));

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
	ui->lineEditRecognizerName->setText(transcriberModel_->recognizerName());
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
	
	float docOffsetX = transcriberModel_->docOffsetX();
	ui->horizontalScrollBarSamples->setValue((int)docOffsetX);
}


void TranscriberMainWindow::pushButtonLoad_Clicked()
{
	qDebug() << "pushButtonLoad_Clicked";
    transcriberModel_->loadAudioFile();
	//ui->widgetSamples->update(10, 10, 20, 100);
	//ui->widgetSamples->repaint(10, 10, 20, 100);
}

void TranscriberMainWindow::pushButtonSaveAudioAnnot_Clicked()
{
	transcriberModel_->saveAudioMarkupToXml();
}

void TranscriberMainWindow::pushButtonPlay_Clicked()
{
    transcriberModel_->soundPlayerPlay();
}

void TranscriberMainWindow::pushButtonPause_Clicked()
{
	transcriberModel_->soundPlayerPause();
}

void TranscriberMainWindow::radioButtonWordLevel_toggled(bool checked)
{
	transcriberModel_->setCurrentMarkerLevelOfDetail(checked ? PticaGovorun::MarkerLevelOfDetail::Word : PticaGovorun::MarkerLevelOfDetail::Phone);
}

void TranscriberMainWindow::lineEditFileName_editingFinished()
{
    transcriberModel_->setAudioFilePath(ui->lineEditFileName->text());
}

void TranscriberMainWindow::lineEditRecognizerName_editingFinished()
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
	float sampleInd = transcriberModel_->docPosXToSampleInd(mouseDocPosX);
	UpdateDocPosXAndFrameInd(mouseDocPosX, sampleInd);
}

void TranscriberMainWindow::UpdateDocPosXAndFrameInd(float mouseDocPosX, float sampleInd)
{
	std::stringstream msg;
	msg.precision(2);
	msg << std::fixed;
	msg << mouseDocPosX;
	ui->lineEditCurDocPosX->setText(QString::fromStdString(msg.str()));

	msg.str("");
	msg << sampleInd;
	ui->lineEditCurSampleInd->setText(QString::fromStdString(msg.str()));
}

void TranscriberMainWindow::transcriberModel_currentFrameIndChanged(long oldCurFrameInd)
{
	QRect updateRect = ui->widgetSamples->rect();
	if (oldCurFrameInd == PticaGovorun::PGFrameIndNull)
		ui->widgetSamples->update(updateRect);

	long newCurFrameInd = transcriberModel_->currentFrameInd();
	if (newCurFrameInd == PticaGovorun::PGFrameIndNull)
		ui->widgetSamples->update(updateRect);

	//
	float newCursorDocPosX = transcriberModel_->sampleIndToDocPosX(newCurFrameInd);
	UpdateDocPosXAndFrameInd(newCursorDocPosX, newCurFrameInd);

	// update only invalidated rect

	auto minFrameInd = std::min(oldCurFrameInd, newCurFrameInd);
	auto maxFrameInd = std::max(oldCurFrameInd, newCurFrameInd);

	auto minX = transcriberModel_->sampleIndToDocPosX(minFrameInd);
	minX -= transcriberModel_->docOffsetX();

	auto maxX = transcriberModel_->sampleIndToDocPosX(maxFrameInd);
	maxX -= transcriberModel_->docOffsetX();

	const static int VerticalMarkerWidth = 3;
	updateRect.setLeft(minX - VerticalMarkerWidth);
	updateRect.setRight(maxX + VerticalMarkerWidth);

	// QWidget::repaint is more responsive and the moving cursor may not flicker
	// but if drawing is slow, the UI may stop responding at all
	// QWidget::update may not be in time to interactively update the UI (the cursor may flicker)
	// but the UI will be responsive under intensive drawing
	ui->widgetSamples->update(updateRect);
	//ui->widgetSamples->repaint(updateRect); // NOTE: may hang in paint routines on audio playing
}

void TranscriberMainWindow::transcriberModel_currentMarkerIndChanged()
{
	PticaGovorun::MarkerLevelOfDetail uiMarkerLevel = transcriberModel_->templateMarkerLevelOfDetail();
	QString uiMarkerIdStr = "###";
	QString uiMarkerTranscriptStr = "";
	bool uiMarkerStopsPlayback = false;
	bool uiMarkerStopsPlaybackEnabled = false;

	int markerInd = transcriberModel_->currentMarkerInd();
	if (markerInd != -1)
	{
		const auto& marker = transcriberModel_->frameIndMarkers()[markerInd];
		uiMarkerIdStr = QString("%1").arg(marker.Id);
		uiMarkerLevel = marker.LevelOfDetail;

		uiMarkerTranscriptStr = marker.TranscripText;
		uiMarkerStopsPlayback = marker.StopsPlayback;
		uiMarkerStopsPlaybackEnabled = true;
	}
	else
	{
		uiMarkerStopsPlaybackEnabled = false;
	}

	// update ui

	ui->labelMarkerId->setText(uiMarkerIdStr);

	if (uiMarkerLevel == PticaGovorun::MarkerLevelOfDetail::Word)
		ui->radioButtonWordLevel->setChecked(true);
	else if (uiMarkerLevel == PticaGovorun::MarkerLevelOfDetail::Phone)
		ui->radioButtonPhoneLevel->setChecked(true);

	ui->lineEditMarkerText->setText(uiMarkerTranscriptStr);
	ui->checkBoxCurMarkerStopOnPlayback->setEnabled(uiMarkerStopsPlaybackEnabled);
	ui->checkBoxCurMarkerStopOnPlayback->setChecked(uiMarkerStopsPlayback);
}

void TranscriberMainWindow::lineEditMarkerText_editingFinished()
{
	transcriberModel_->setCurrentMarkerTranscriptText(ui->lineEditMarkerText->text());
}

void TranscriberMainWindow::checkBoxCurMarkerStopOnPlayback_toggled(bool checked)
{
	transcriberModel_->setCurrentMarkerStopOnPlayback(checked);
}

void TranscriberMainWindow::pushButtonSegmentComposerPlay_Clicked()
{
	QString composingRecipe = ui->plainTextEditAudioSegmentsComposer->toPlainText();
	transcriberModel_->playSegmentComposingRecipe(composingRecipe);
}