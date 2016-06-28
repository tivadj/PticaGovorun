#include "SpeechTranscriptionWidget.h"
#include "ui_SpeechTranscriptionWidget.h"
#include <QDebug>

namespace PticaGovorun
{
	SpeechTranscriptionWidget::SpeechTranscriptionWidget(QWidget *parent) :
		QWidget(parent),
		ui(new Ui::SpeechTranscriptionWidget)
	{
		ui->setupUi(this);
		QObject::connect(ui->horizontalScrollBarSamples, SIGNAL(valueChanged(int)), this, SLOT(horizontalScrollBarSamples_valueChanged(int)));
		QObject::connect(ui->radioButtonWordLevel, SIGNAL(toggled(bool)), this, SLOT(radioButtonWordLevel_toggled(bool)));
		QObject::connect(ui->lineEditMarkerText, SIGNAL(editingFinished()), this, SLOT(lineEditMarkerText_editingFinished()));
		QObject::connect(ui->checkBoxCurMarkerStopOnPlayback, SIGNAL(toggled(bool)), this, SLOT(checkBoxCurMarkerStopOnPlayback_toggled(bool)));
		QObject::connect(ui->checkBoxCurMarkerUseInTrain, SIGNAL(toggled(bool)), this, SLOT(checkBoxCurMarkerUseInTrain_toggled(bool)));
		QObject::connect(ui->radioButtonLangNone, SIGNAL(toggled(bool)), this, SLOT(groupBoxLang_toggled(bool)));
		QObject::connect(ui->radioButtonLangRu, SIGNAL(toggled(bool)), this, SLOT(groupBoxLang_toggled(bool)));
		QObject::connect(ui->radioButtonLangUk, SIGNAL(toggled(bool)), this, SLOT(groupBoxLang_toggled(bool)));
		QObject::connect(ui->comboBoxSpeakerId, SIGNAL(currentIndexChanged(int)), this, SLOT(comboBoxSpeakerId_currentIndexChanged(int)));
	}

	SpeechTranscriptionWidget::~SpeechTranscriptionWidget()
	{
		delete ui;
	}

	void SpeechTranscriptionWidget::setTranscriberViewModel(std::shared_ptr<SpeechTranscriptionViewModel> value)
	{
		transcriberModel_ = value;
		ui->widgetSamples->setModel(value);

		QObject::connect(transcriberModel_.get(), SIGNAL(audioSamplesLoaded()), this, SLOT(transcriberModel_audioSamplesLoaded()));
		QObject::connect(transcriberModel_.get(), SIGNAL(audioSamplesChanged()), this, SLOT(transcriberModel_audioSamplesChanged()));
		QObject::connect(transcriberModel_.get(), SIGNAL(lastMouseDocPosXChanged(float)), this, SLOT(transcriberModel_lastMouseDocPosXChanged(float)));
		QObject::connect(transcriberModel_.get(), SIGNAL(docOffsetXChanged()), this, SLOT(transcriberModel_docOffsetXChanged()));
		QObject::connect(transcriberModel_.get(), SIGNAL(cursorChanged(std::pair<long, long>)), this, SLOT(transcriberModel_cursorChanged(std::pair<long, long>)));
		QObject::connect(transcriberModel_.get(), SIGNAL(currentMarkerIndChanged()), this, SLOT(transcriberModel_currentMarkerIndChanged()));
		QObject::connect(transcriberModel_.get(), SIGNAL(playingSampleIndChanged(long)), this, SLOT(transcriberModel_playingSampleIndChanged(long)));
		
		transcriberModel_audioSamplesLoaded();
	}

	void SpeechTranscriptionWidget::UpdateCursorUI()
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

	void SpeechTranscriptionWidget::UpdateSpeakerListUI()
	{
		ui->comboBoxSpeakerId->clear();
		ui->comboBoxSpeakerId->addItem("empty", QVariant::fromValue(QString("")));

		const auto& speakers = transcriberModel_->speechAnnotation().speakers();
		for (const PticaGovorun::SpeakerFantom& speaker : speakers)
		{
			ui->comboBoxSpeakerId->addItem(QString::fromStdWString(speaker.Name), QVariant::fromValue(QString::fromStdWString(speaker.BriefId)));
		}
	}

	void SpeechTranscriptionWidget::resizeEvent(QResizeEvent* e)
	{
		QSize samplesWidgetSize = ui->widgetSamples->size();
		qDebug() << samplesWidgetSize;

		transcriberModel_->setViewportSize(samplesWidgetSize.width(), samplesWidgetSize.height());
	}

	void SpeechTranscriptionWidget::horizontalScrollBarSamples_valueChanged(int value)
	{
		//qDebug() <<value <<" bar.value="  <<ui->horizontalScrollBarSamples->value();
		transcriberModel_->setDocOffsetX(value);
	}

	void SpeechTranscriptionWidget::transcriberModel_audioSamplesLoaded()
	{
		ui->widgetSamples->setFocus(Qt::OtherFocusReason);
		UpdateSpeakerListUI();
	}

	// called on:
	// audioSamples changed
	// docOffsetX changed
	void SpeechTranscriptionWidget::transcriberModel_audioSamplesChanged()
	{
		ui->widgetSamples->update();

		updateSamplesSlider();
	}

	void SpeechTranscriptionWidget::transcriberModel_docOffsetXChanged()
	{
		ui->widgetSamples->update();

		updateSamplesSlider();
	}

	void SpeechTranscriptionWidget::updateSamplesSlider()
	{
		ui->horizontalScrollBarSamples->setMinimum(0);

		//
		int docWidth = transcriberModel_->docWidthPix();
		//int pageStep = (int)(ui->widgetSamples->width() / transcriberModel_->pixelsPerSample());
		int pageStep = ui->widgetSamples->width();

		int slideMax = docWidth;
		slideMax -= pageStep / 2;
		ui->horizontalScrollBarSamples->setMaximum(slideMax);

		//const int stepsPerScreen = 10; // need x of steps to pass the page
		//ui->horizontalScrollBarSamples->setSingleStep(pageStep / stepsPerScreen);
		ui->horizontalScrollBarSamples->setSingleStep(100);
		ui->horizontalScrollBarSamples->setPageStep(pageStep);

		float docOffsetX = transcriberModel_->docOffsetX();
		ui->horizontalScrollBarSamples->setValue((int)docOffsetX);
	}

	void SpeechTranscriptionWidget::transcriberModel_cursorChanged(std::pair<long, long> oldCursor)
	{
		UpdateCursorUI();

		QRect updateRect = ui->widgetSamples->rect();

		auto nullCur = SpeechTranscriptionViewModel::nullCursor();
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

	void SpeechTranscriptionWidget::transcriberModel_currentMarkerIndChanged()
	{
		MarkerLevelOfDetail uiMarkerLevel = transcriberModel_->templateMarkerLevelOfDetail();
		SpeechLanguage uiMarkerLang = SpeechLanguage::NotSet;
		QString uiMarkerIdStr = "###";
		QString uiMarkerTranscriptStr = "";
		bool uiMarkerStopsPlayback = false;
		bool uiMarkerStopsPlaybackEnabled = false;
		QString uiMarkerSpeakerBriefId = "";

		int markerInd = transcriberModel_->currentMarkerInd();
		if (markerInd != -1)
		{
			const auto& marker = transcriberModel_->frameIndMarkers()[markerInd];
			qDebug() << "Current markerId=" << marker.Id;

			uiMarkerIdStr = QString("%1").arg(marker.Id);
			uiMarkerLevel = marker.LevelOfDetail;
			uiMarkerLang = marker.Language;

			uiMarkerTranscriptStr = marker.TranscripText;
			uiMarkerStopsPlayback = marker.StopsPlayback;
			uiMarkerStopsPlaybackEnabled = true;
			uiMarkerSpeakerBriefId = QString::fromStdWString(marker.SpeakerBriefId);

			bool useInTrain = marker.ExcludePhase == nullptr || marker.ExcludePhase == ResourceUsagePhase::Test;
			ui->checkBoxCurMarkerUseInTrain->setEnabled(true);
			ui->checkBoxCurMarkerUseInTrain->setChecked(useInTrain);
		}
		else
		{
			uiMarkerStopsPlaybackEnabled = false;
			uiMarkerLang = transcriberModel_->templateMarkerSpeechLanguage();
			ui->checkBoxCurMarkerUseInTrain->setEnabled(false);
			ui->checkBoxCurMarkerUseInTrain->setChecked(false);
		}

		// update ui

		ui->labelMarkerId->setText(uiMarkerIdStr);

		if (uiMarkerLevel == PticaGovorun::MarkerLevelOfDetail::Word)
			ui->radioButtonWordLevel->setChecked(true);
		else if (uiMarkerLevel == PticaGovorun::MarkerLevelOfDetail::Phone)
			ui->radioButtonPhoneLevel->setChecked(true);

		// speech lang
		QRadioButton *langButton = ui->radioButtonLangNone;
		if (uiMarkerLang == SpeechLanguage::Ukrainian)
			langButton = ui->radioButtonLangUk;
		else if (uiMarkerLang == SpeechLanguage::Russian)
			langButton = ui->radioButtonLangRu;
		langButton->setChecked(true);

		ui->lineEditMarkerText->setText(uiMarkerTranscriptStr);
		ui->lineEditMarkerPhoneList->setText(transcriberModel_->currentMarkerPhoneListString());
		ui->checkBoxCurMarkerStopOnPlayback->setEnabled(uiMarkerStopsPlaybackEnabled);
		ui->checkBoxCurMarkerStopOnPlayback->setChecked(uiMarkerStopsPlayback);

		int comboSpeakerInd = ui->comboBoxSpeakerId->findData(QVariant::fromValue(uiMarkerSpeakerBriefId));
		ui->comboBoxSpeakerId->setCurrentIndex(comboSpeakerInd);
		
		ui->widgetSamples->setFocus(Qt::OtherFocusReason);
	}

	void SpeechTranscriptionWidget::transcriberModel_playingSampleIndChanged(long oldPlayingSampleInd)
	{
		QRect updateRect = ui->widgetSamples->rect();

		auto playingSampleInd = transcriberModel_->playingSampleInd();
		if (oldPlayingSampleInd != PticaGovorun::NullSampleInd && playingSampleInd != PticaGovorun::NullSampleInd)
		{
			assert(oldPlayingSampleInd <= playingSampleInd && "Audio must be played forward?");

			auto minSampleInd = oldPlayingSampleInd;
			auto maxSampleInd = playingSampleInd;

			auto leftX = transcriberModel_->sampleIndToDocPosX(minSampleInd);
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

	void SpeechTranscriptionWidget::transcriberModel_lastMouseDocPosXChanged(float mouseDocPosX)
	{
		float sampleInd = transcriberModel_->docPosXToSampleInd(mouseDocPosX);
	}

	void SpeechTranscriptionWidget::comboBoxSpeakerId_currentIndexChanged(int index)
	{
		QVariant tag = ui->comboBoxSpeakerId->itemData(index);
		std::wstring speakerBriefId = tag.toString().toStdWString();
		transcriberModel_->setCurrentMarkerSpeaker(speakerBriefId);
	}

	void SpeechTranscriptionWidget::radioButtonWordLevel_toggled(bool checked)
	{
		transcriberModel_->setCurrentMarkerLevelOfDetail(checked ? PticaGovorun::MarkerLevelOfDetail::Word : PticaGovorun::MarkerLevelOfDetail::Phone);
	}

	void SpeechTranscriptionWidget::groupBoxLang_toggled(bool checked)
	{
		using namespace PticaGovorun;
		SpeechLanguage lang = SpeechLanguage::NotSet;
		if (ui->radioButtonLangRu->isChecked())
			lang = SpeechLanguage::Russian;
		else if (ui->radioButtonLangUk->isChecked())
			lang = SpeechLanguage::Ukrainian;
		transcriberModel_->setCurrentMarkerLang(lang);
	}

	void SpeechTranscriptionWidget::lineEditMarkerText_editingFinished()
	{
		transcriberModel_->setCurrentMarkerTranscriptText(ui->lineEditMarkerText->text());
		ui->widgetSamples->setFocus();
	}

	void SpeechTranscriptionWidget::checkBoxCurMarkerStopOnPlayback_toggled(bool checked)
	{
		transcriberModel_->setCurrentMarkerStopOnPlayback(checked);
	}

	void SpeechTranscriptionWidget::checkBoxCurMarkerUseInTrain_toggled(bool checked)
	{
		auto excludePhase = boost::make_optional<ResourceUsagePhase>(!checked, ResourceUsagePhase::Train);
		transcriberModel_->setCurrentMarkerExcludePhase(excludePhase);
	}

	void SpeechTranscriptionWidget::keyPressEvent(QKeyEvent* ke)
	{
		// lanes view
		if (ke->key() == Qt::Key_F2)
			ui->lineEditMarkerText->setFocus();
		else if (ke->key() == Qt::Key_Plus)
			transcriberModel_->increaseLanesCountRequest();
		else if (ke->key() == Qt::Key_Minus)
			transcriberModel_->decreaseLanesCountRequest();
		else if (ke->key() == Qt::Key_Insert)
			transcriberModel_->insertNewMarkerAtCursorRequest();
		else if (ke->key() == Qt::Key_Delete)
			transcriberModel_->deleteRequest(ke->modifiers().testFlag(Qt::ControlModifier));
		else if (ke->key() == Qt::Key_T && ke->modifiers().testFlag(Qt::ControlModifier))
			transcriberModel_->selectMarkerClosestToCurrentCursorRequest();

		// navigation
		else if (ke->key() == Qt::Key_End && ke->modifiers().testFlag(Qt::ControlModifier))
			transcriberModel_->scrollDocumentEndRequest();
		else if (ke->key() == Qt::Key_Home && ke->modifiers().testFlag(Qt::ControlModifier))
			transcriberModel_->scrollDocumentStartRequest();
		else if (ke->key() == Qt::Key_Right && ke->modifiers().testFlag(Qt::ControlModifier))
			transcriberModel_->selectMarkerForward();
		else if (ke->key() == Qt::Key_Left && ke->modifiers().testFlag(Qt::ControlModifier))
			transcriberModel_->selectMarkerBackward();
		else if (ke->key() == Qt::Key_PageUp)
			transcriberModel_->scrollPageBackwardRequest(); // PageUp is near Home => it scrolls backward
		else if (ke->key() == Qt::Key_PageDown)
			transcriberModel_->scrollPageForwardRequest();

		// play
		//if (ke->key() == Qt::Key_Space || ke->key() == Qt::Key_C)
		//	transcriberModel_->soundPlayerTogglePlayPause();
		//// F9 is used to start playing when typing transcript text in
		//else if (ke->key() == Qt::Key_X || ke->key() == Qt::Key_F9)
		//	transcriberModel_->soundPlayerPlayCurrentSegment(SegmentStartFrameToPlayChoice::SegmentBegin);
		//else if (ke->key() == Qt::Key_Backslash)
		//	transcriberModel_->soundPlayerPlayCurrentSegment(SegmentStartFrameToPlayChoice::CurrentCursor);

		if (ke->key() == Qt::Key_Space)
			transcriberModel_->soundPlayerTogglePlayPause();
		// F9 is used to start playing when typing transcript text in
		else if (ke->key() == Qt::Key_F9)
		{
			if (ke->modifiers().testFlag(Qt::ControlModifier))
				transcriberModel_->soundPlayerPlayCurrentSegment(SegmentStartFrameToPlayChoice::CurrentCursor);
			else
				transcriberModel_->soundPlayerPlayCurrentSegment(SegmentStartFrameToPlayChoice::SegmentBegin);
		}
		else if (ke->key() == Qt::Key_Backslash)
		{
			if (ke->modifiers().testFlag(Qt::ControlModifier))
				transcriberModel_->soundPlayerPlayCurrentSegment(SegmentStartFrameToPlayChoice::CurrentCursor);
			else
				transcriberModel_->soundPlayerPlayCurrentSegment(SegmentStartFrameToPlayChoice::SegmentBegin);
		}

		// analyze
		else if (ke->key() == Qt::Key_R && ke->modifiers().testFlag(Qt::ControlModifier))
			transcriberModel_->recognizeCurrentSegmentJuliusRequest();
		else if (ke->key() == Qt::Key_F1)
			transcriberModel_->recognizeCurrentSegmentSphinxRequest();
		else if (ke->key() == Qt::Key_A)
			transcriberModel_->alignPhonesForCurrentSegmentRequest();

		// experimental

		else if (ke->key() == Qt::Key_F6)
			transcriberModel_->analyzeUnlabeledSpeech();
		else if (ke->key() == Qt::Key_F4)
			transcriberModel_->dumpSilence();
		else if (ke->key() == Qt::Key_D && ke->modifiers().testFlag(Qt::ControlModifier))
			transcriberModel_->saveCurrentRangeAsWavRequest();

		// persistence
		else if (ke->key() == Qt::Key_F5)
			transcriberModel_->refreshRequest();

		else
			QWidget::keyPressEvent(ke);
	}
}