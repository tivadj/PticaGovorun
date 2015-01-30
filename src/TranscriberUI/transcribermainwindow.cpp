#include <sstream>
#include <array>
#include "transcribermainwindow.h"
#include "ui_transcribermainwindow.h"
#include <QDebug>
#include <QMouseEvent>
#include <QSize>
#include <QTextDocumentFragment>
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
	QObject::connect(transcriberModel_.get(), SIGNAL(cursorChanged(std::pair<long, long>)), this, SLOT(transcriberModel_cursorChanged(std::pair<long, long>)));
	QObject::connect(transcriberModel_.get(), SIGNAL(currentMarkerIndChanged()), this, SLOT(transcriberModel_currentMarkerIndChanged()));
	QObject::connect(transcriberModel_.get(), SIGNAL(playingSampleIndChanged(long)), this, SLOT(transcriberModel_playingSampleIndChanged(long)));

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
	ui->plainTextEditLogger->moveCursor(QTextCursor::End);
	ui->plainTextEditLogger->insertPlainText(message);
	ui->plainTextEditLogger->insertPlainText("\n");
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
    transcriberModel_->loadAudioFileRequest();
}

void TranscriberMainWindow::pushButtonSaveAudioAnnot_Clicked()
{
	transcriberModel_->saveAudioMarkupToXml();
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
}

void TranscriberMainWindow::UpdateCursorUI()
{
	const char* emptyText = "###";

	// fill UI for the case of cursor=None
	ui->lineEditCursorFirstFrameInd->setText(emptyText);
	ui->lineEditCursorFirstDocPosX->setText(emptyText);
	ui->lineEditCursorSecondFrameInd->setText(emptyText);
	ui->lineEditCursorSecondDocPosX->setText(emptyText);
	ui->lineEditCursorFramesCount->setText(emptyText);
	ui->lineEditCursorDocLen->setText(emptyText);

	std::stringstream msg;
	msg.precision(2);
	msg << std::fixed;

	float firstDocX = -1;
	std::pair<long, long> cursor = transcriberModel_->cursor();
	if (cursor.first != PticaGovorun::NullSampleInd)
	{
		msg.str("");
		msg << cursor.first;
		ui->lineEditCursorFirstFrameInd->setText(QString::fromStdString(msg.str()));

		firstDocX = transcriberModel_->sampleIndToDocPosX(cursor.first);
		msg.str("");
		msg << firstDocX;
		ui->lineEditCursorFirstDocPosX->setText(QString::fromStdString(msg.str()));
	}

	if (cursor.second != PticaGovorun::NullSampleInd)
	{
		msg.str("");
		msg << cursor.second;
		ui->lineEditCursorSecondFrameInd->setText(QString::fromStdString(msg.str()));

		float secondDocX = transcriberModel_->sampleIndToDocPosX(cursor.second);
		msg.str("");
		msg << secondDocX;
		ui->lineEditCursorSecondDocPosX->setText(QString::fromStdString(msg.str()));

		// cursor length

		assert(firstDocX != -1);
		long len = cursor.second - cursor.first;
		msg.str("");
		msg << len;
		ui->lineEditCursorFramesCount->setText(QString::fromStdString(msg.str()));

		float lenDocX = secondDocX - firstDocX;
		msg.str("");
		msg << lenDocX;
		ui->lineEditCursorDocLen->setText(QString::fromStdString(msg.str()));
	}
}

void TranscriberMainWindow::transcriberModel_cursorChanged(std::pair<long, long> oldCursor)
{
	UpdateCursorUI();

	QRect updateRect = ui->widgetSamples->rect();

	auto nullCur = TranscriberViewModel::nullCursor();
	auto newCursor = transcriberModel_->cursor();
	if (false && oldCursor != nullCur && newCursor != nullCur)
	{
		// update only invalidated rect

		std::array<long, 4> cursorBounds;
		int cursorBoundsCount = 0;
		if (oldCursor.first != PticaGovorun::NullSampleInd)
			cursorBounds[cursorBoundsCount++] = oldCursor.first;
		if (oldCursor.second != PticaGovorun::NullSampleInd)
			cursorBounds[cursorBoundsCount++] = oldCursor.second;
		if (newCursor.first != PticaGovorun::NullSampleInd)
			cursorBounds[cursorBoundsCount++] = newCursor.first;
		if (newCursor.second != PticaGovorun::NullSampleInd)
			cursorBounds[cursorBoundsCount++] = newCursor.second;

		assert(cursorBoundsCount > 0 && "Must be some cursor boundaries on cursor change");

		auto minIt = std::min_element(cursorBounds.begin(), cursorBounds.begin() + cursorBoundsCount);
		auto maxIt = std::max_element(cursorBounds.begin(), cursorBounds.begin() + cursorBoundsCount);

		auto minSampleInd = *minIt;
		auto maxSampleInd = *maxIt;

		auto minX = transcriberModel_->sampleIndToDocPosX(minSampleInd);
		minX -= transcriberModel_->docOffsetX();

		auto maxX = transcriberModel_->sampleIndToDocPosX(maxSampleInd);
		maxX -= transcriberModel_->docOffsetX();

		const static int VerticalMarkerWidth = 3;
		updateRect.setLeft(minX - VerticalMarkerWidth);
		updateRect.setRight(maxX + VerticalMarkerWidth);
	}

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

void TranscriberMainWindow::transcriberModel_playingSampleIndChanged(long oldPlayingSampleInd)
{
	QRect updateRect = ui->widgetSamples->rect();

	auto playingSampleInd = transcriberModel_->playingSampleInd();
	if (oldPlayingSampleInd != PticaGovorun::NullSampleInd && playingSampleInd != PticaGovorun::NullSampleInd)
	{
		assert(oldPlayingSampleInd <= playingSampleInd && "Audio must be played forward?");

		auto minSampleInd = oldPlayingSampleInd;
		auto maxSampleInd = playingSampleInd;

		auto leftX  = transcriberModel_->sampleIndToDocPosX(minSampleInd);
		auto rightX = transcriberModel_->sampleIndToDocPosX(maxSampleInd);

		// repaint optimization when caret moves inside one lane
		ViewportHitInfo hitLeft;
		ViewportHitInfo hitRight;
		if (transcriberModel_->docPosToViewport(leftX, hitLeft) && 
			transcriberModel_->docPosToViewport(rightX, hitRight) &&
			hitLeft.LaneInd == hitRight.LaneInd)
		{
			RectY laneY = transcriberModel_->laneYBounds(hitLeft.LaneInd);

			const static int VerticalMarkerWidth = 1;
			updateRect.setLeft(hitLeft.DocXInsideLane - VerticalMarkerWidth);
			updateRect.setRight(hitRight.DocXInsideLane + VerticalMarkerWidth);
			updateRect.setTop(laneY.Top);
			updateRect.setBottom(laneY.Top + laneY.Height);
		}
	}

	ui->widgetSamples->update(updateRect);
}

void TranscriberMainWindow::lineEditMarkerText_editingFinished()
{
	transcriberModel_->setCurrentMarkerTranscriptText(ui->lineEditMarkerText->text());
	ui->widgetSamples->setFocus();
}

void TranscriberMainWindow::checkBoxCurMarkerStopOnPlayback_toggled(bool checked)
{
	transcriberModel_->setCurrentMarkerStopOnPlayback(checked);
}

void TranscriberMainWindow::pushButtonSegmentComposerPlay_Clicked()
{
	QString composingRecipe;
	if (ui->plainTextEditAudioSegmentsComposer->textCursor().hasSelection())
		composingRecipe = ui->plainTextEditAudioSegmentsComposer->textCursor().selection().toPlainText();
	else
		composingRecipe = ui->plainTextEditAudioSegmentsComposer->toPlainText();

	transcriberModel_->playComposingRecipeRequest(composingRecipe);
}

void TranscriberMainWindow::keyPressEvent(QKeyEvent* ke)
{
	if (ke->key() == Qt::Key_Space || ke->key() == Qt::Key_C)
		transcriberModel_->soundPlayerPlayCurrentSegment(SegmentStartFrameToPlayChoice::CurrentCursor);
	else if (ke->key() == Qt::Key_X)
		transcriberModel_->soundPlayerPlayCurrentSegment(SegmentStartFrameToPlayChoice::SegmentBegin);
	else if (ke->key() == Qt::Key_Backslash)
		transcriberModel_->soundPlayerTogglePlayPause();

	else if (ke->key() == Qt::Key_R)
		transcriberModel_->recognizeCurrentSegmentJuliusRequest();
	else if (ke->key() == Qt::Key_F1)
		transcriberModel_->recognizeCurrentSegmentSphinxRequest();

	else if (ke->key() == Qt::Key_A)
		transcriberModel_->alignPhonesForCurrentSegmentRequest();

	else if (ke->key() == Qt::Key_Insert)
		transcriberModel_->insertNewMarkerAtCursorRequest();
	else if (ke->key() == Qt::Key_Delete)
		transcriberModel_->deleteRequest(ke->modifiers().testFlag(Qt::ControlModifier));
	else if (ke->key() == Qt::Key_T)
		transcriberModel_->selectMarkerClosestToCurrentCursorRequest();

	else if (ke->key() == Qt::Key_G && ke->modifiers().testFlag(Qt::ControlModifier))
		transcriberModel_->navigateToMarkerRequest();
	else if (ke->key() == Qt::Key_PageUp)
		transcriberModel_->scrollPageBackwardRequest(); // PageUp is near Home => it scrolls backward
	else if (ke->key() == Qt::Key_PageDown)
		transcriberModel_->scrollPageForwardRequest();

	// navigation
	else if (ke->key() == Qt::Key_End && ke->modifiers().testFlag(Qt::ControlModifier))
		transcriberModel_->scrollDocumentEndRequest();
	else if (ke->key() == Qt::Key_Home && ke->modifiers().testFlag(Qt::ControlModifier))
		transcriberModel_->scrollDocumentStartRequest();
	else if (ke->key() == Qt::Key_Right && ke->modifiers().testFlag(Qt::ControlModifier))
		transcriberModel_->selectMarkerForward();
	else if (ke->key() == Qt::Key_Left && ke->modifiers().testFlag(Qt::ControlModifier))
		transcriberModel_->selectMarkerBackward();
	

	else if (ke->key() == Qt::Key_Plus)
		transcriberModel_->increaseLanesCountRequest();
	else if (ke->key() == Qt::Key_Minus)
		transcriberModel_->decreaseLanesCountRequest();

	else if (ke->key() == Qt::Key_F11)
		pushButtonSegmentComposerPlay_Clicked();
	else if (ke->key() == Qt::Key_F2)
	{
		ui->lineEditMarkerText->setFocus();
	}
	else if (ke->key() == Qt::Key_F3)
		transcriberModel_->analyzeUnlabeledSpeech();
	else if (ke->key() == Qt::Key_F4)
		transcriberModel_->dumpSilence();
	else if (ke->key() == Qt::Key_D && ke->modifiers().testFlag(Qt::ControlModifier))
		transcriberModel_->saveCurrentRangeAsWavRequest();

	else if (ke->key() == Qt::Key_F5)
		transcriberModel_->refreshRequest();
	else
		QWidget::keyPressEvent(ke);
}

void TranscriberMainWindow::resizeEvent(QResizeEvent* e)
{
	QSize samplesWidgetSize = ui->widgetSamples->size();
	qDebug() << samplesWidgetSize;

	transcriberModel_->setViewportSize(samplesWidgetSize.width(), samplesWidgetSize.height());
}