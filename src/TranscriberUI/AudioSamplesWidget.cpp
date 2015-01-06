#include "AudioSamplesWidget.h"
#include <QPainter>
#include <QBrush>
#include <QDebug>
#include "InteropPython.h"

AudioSamplesWidget::AudioSamplesWidget(QWidget *parent) :
    QWidget(parent)
{
	setFocusPolicy(Qt::WheelFocus);

	// set the background
	//setBackgroundRole(QPalette::HighlightedText); // doesn't work
	setAutoFillBackground(true);
	QPalette pal = palette();
	pal.setColor(QPalette::Window, QColor(255, 255, 255)); // set white background
	setPalette(pal);    
}

void AudioSamplesWidget::setModel(std::shared_ptr<TranscriberViewModel> transcriberModel)
{
	transcriberModel_ = transcriberModel;
}

void AudioSamplesWidget::paintEvent(QPaintEvent* pe)
{
	//qDebug() << "paintEvent" <<pe->rect();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QColor marginalRecColor(255, 0, 0);

	painter.setPen(marginalRecColor);
    painter.setBrush(Qt::white);
    //painter.setBackgroundMode(Qt::OpaqueMode);
	//painter.setBackground(QBrush(QColor(255, 255, 0), Qt::SolidPattern));
	const int p = 15;
	painter.drawRect(p,p,width()-2*p, height()-2*p);

	//

	if (transcriberModel_ == nullptr)
		return;

	const auto& audioSamples = transcriberModel_->audioSamples();
	if (audioSamples.empty())
		return;

	float canvasHeight = height();
	float canvasHeightHalf = canvasHeight / 2.0f;

	// determine first left visible sample
	

	QColor sampleColor(0, 0, 0);
	painter.setPen(sampleColor);

	const float XNull = -1;
	float prevX = XNull;
	float prevY = XNull;

	float viewportRight = std::min(width(), pe->rect().right());

	// determine first left visible sample
	float viewportLeft = std::max(0, pe->rect().left());
	float visibleDocLeft = transcriberModel_->docOffsetX() + viewportLeft;
	float visibleDocRight = transcriberModel_->docOffsetX() + viewportRight;
	long firstVisibleSampleInd = transcriberModel_->docPosXToSampleInd(visibleDocLeft);
	firstVisibleSampleInd = std::max(0L, firstVisibleSampleInd); // make it >=0

	for (size_t sampleInd = firstVisibleSampleInd; sampleInd < audioSamples.size(); ++sampleInd)
	{
		float xPix = transcriberModel_->sampleIndToDocPosX(sampleInd);
		xPix -= transcriberModel_->docOffsetX(); // compensate for docOffsetX

		// the optimization to avoid drawing lines from previous sample to
		// the current sample, when both are squeezed in the same pixel
		if (prevX != XNull && prevX == xPix)
			continue;

		// passed beside the right side of the viewport
		if (xPix > viewportRight)
			break;

		short sampleValue = audioSamples[sampleInd];
		float sampleYPerc = sampleValue / (float)std::numeric_limits<short>::max();
		float y = canvasHeightHalf - canvasHeightHalf * sampleYPerc * 0.8f;

		if (prevX != XNull)
		{
			painter.drawLine(QPointF(prevX, prevY), QPointF(xPix, y));
		}

		prevX = xPix;
		prevY = y;
	}


	QColor phonemeColor(0, 0, 0);
	painter.setPen(phonemeColor);
	drawPhonemesAndPhonemeMarkers(painter, canvasHeight, visibleDocLeft, visibleDocRight);

	// draw phrase markers

	drawFrameIndMarkers(painter, canvasHeight, visibleDocLeft, visibleDocRight);

	// draw current frame adornment

	QColor curMarkerColor(0, 0, 0);
	painter.setPen(curMarkerColor);
	
	long currentFrameInd = transcriberModel_->currentFrameInd();
	float curFrameDocX = transcriberModel_->sampleIndToDocPosX(currentFrameInd);
	curFrameDocX -= transcriberModel_->docOffsetX();
	painter.drawLine(curFrameDocX, 0, curFrameDocX, canvasHeight);
}

void AudioSamplesWidget::drawFrameIndMarkers(QPainter& painter, int markerHeightMax, float visibleDocLeft, float visibleDocRight)
{
	auto docOffsetX = transcriberModel_->docOffsetX();

	int visibleMarkerInd = -1;

	QColor wordMarkerColor(0, 255, 0); // green
	QColor phoneMarkerColor(255, 127, 39); // orange
	int markerCenterY = markerHeightMax / 2;
	int markerHalfHeightMax = markerHeightMax / 2;
	
	//
	const auto& markers = transcriberModel_->frameIndMarkers();
	for (size_t markerInd = 0; markerInd < markers.size(); ++markerInd)
	{
		const PticaGovorun::TimePointMarker& marker = markers[markerInd];

		long frameInd = marker.SampleInd;
		float frameDocX = transcriberModel_->sampleIndToDocPosX(frameInd);
		
		// repaint only markers from invalidated region
		if (frameDocX < visibleDocLeft) continue;
		if (frameDocX > visibleDocRight) break;

		visibleMarkerInd++;

		// draw marker

		auto x = frameDocX - docOffsetX;

		int markerHalfHeight = markerHalfHeightMax;
		if (marker.LevelOfDetail == PticaGovorun::MarkerLevelOfDetail::Word)
		{
			painter.setPen(wordMarkerColor);
			markerHalfHeight = markerHalfHeightMax;
		}
		else if (marker.LevelOfDetail == PticaGovorun::MarkerLevelOfDetail::Phone)
		{
			painter.setPen(phoneMarkerColor);
			markerHalfHeight = markerHalfHeightMax * 0.6;
		}

		painter.drawLine(x, markerCenterY - markerHalfHeight, x, markerCenterY + markerHalfHeight);

		// draw cross above the marker to indicate that it stops audio player
		if (marker.StopsPlayback)
		{
			const int Dx = 3;
			painter.drawLine(x-Dx, 0, x+Dx, 2*Dx);
			painter.drawLine(x-Dx, 2*Dx, x+Dx, 0);		
		}

		// draw marker id
		if (marker.LevelOfDetail == PticaGovorun::MarkerLevelOfDetail::Phone)
		{
			painter.drawText(x, markerCenterY - markerHalfHeight, QString("%1").arg(marker.Id));
		}

		// draw speech recognition results

		// the recognized text from the previous invisible marker may still be visible 
		if (visibleMarkerInd == 0 && markerInd >= 1)
		{
			const auto& prevMarker = markers[markerInd - 1];
			float prevFrameDocX = transcriberModel_->sampleIndToDocPosX(prevMarker.SampleInd);
			drawMarkerRecognizedText(painter, prevMarker, prevFrameDocX);
		}
		drawMarkerRecognizedText(painter, marker, frameDocX);
	}
}

void AudioSamplesWidget::drawMarkerRecognizedText(QPainter& painter, const PticaGovorun::TimePointMarker& marker, float frameDocX)
{
	auto docOffsetX = transcriberModel_->docOffsetX();
	auto x = frameDocX - docOffsetX;

	static const int TextHeight = 16;

	if (!marker.TranscripText.isEmpty())
	{
		painter.drawText(x, TextHeight, marker.TranscripText);
	}
	if (!marker.RecogSegmentText.isEmpty())
	{
		painter.drawText(x, TextHeight*2, marker.RecogSegmentText);
	}
	if (!marker.RecogSegmentWords.isEmpty())
	{
		painter.drawText(x, TextHeight * 3, marker.RecogSegmentWords);
	}
}

void AudioSamplesWidget::drawPhonemesAndPhonemeMarkers(QPainter& painter, int markerHeight, float visibleDocLeft, float visibleDocRight)
{
	using namespace PticaGovorun;
	int leftMarkerInd = -1;
	std::tuple<long,long> seg = transcriberModel_->getFrameRangeToPlay(transcriberModel_->currentFrameInd(), SegmentStartFrameToPlayChoice::SegmentBegin, &leftMarkerInd);
	if (leftMarkerInd == -1)
		return;
	
	int segBegSample = std::get<0>(seg);
	int segEndSample = std::get<1>(seg);

	// extend the segment with silence
	long extendedSegBegSample = segBegSample - transcriberModel_->silencePadAudioFramesCount();
	long extendedSegEndSample = segEndSample + transcriberModel_->silencePadAudioFramesCount();

	const int PhoneRowHeight = 10; // height of each phone's delimiter cell
	
	// make the number of phone rows so that each phone cell do not overlap with neighbour cell in the next column
	const int CellGapXPix = 10; // (in pixels) the width of the empty space between neighbour phone cells in one row
	float pixPerSample = transcriberModel_->pixelsPerSample();
	int PhoneRowsCount = 1 + (FrameSize * pixPerSample + CellGapXPix) / (FrameShift * pixPerSample);

	int phonemesBottomLine = height();

	// paint optimization, prevent drawing the phone ruler when audio plays
	if (!transcriberModel_->soundPlayerIsPlaying())
		drawShiftedFramesRuler(painter, phonemesBottomLine, extendedSegBegSample, extendedSegEndSample, PhoneRowHeight, PhoneRowsCount);

	// recognized phones

	const auto& markers = transcriberModel_->frameIndMarkers();
	const PticaGovorun::TimePointMarker& marker = markers[leftMarkerInd];
	int phoneNameY = phonemesBottomLine - PhoneRowHeight*(PhoneRowsCount + 1); // +1 to jump to the upper line
	
	long phonesOffsetX = marker.SampleInd;
	if (marker.RecogAlignedPhonemeSeqPadded)
		phonesOffsetX -= transcriberModel_->silencePadAudioFramesCount();

	drawPhoneMarkersAndNames(painter, phonesOffsetX, marker.RecogAlignedPhonemeSeq, markerHeight, markerHeight*0.3, phoneNameY);

	// transcription text phones

	int transcripTextMarkerBottomY = markerHeight*0.69;
	int transcripTextPhoneNameY = transcripTextMarkerBottomY + 16; // +height of text
	drawPhoneMarkersAndNames(painter, phonesOffsetX, marker.TranscripTextPhones.AlignInfo, transcripTextMarkerBottomY, markerHeight*0.3, transcripTextPhoneNameY);

	// grid is in the top
	int gridTopY = 0;
	int gridBotY = markerHeight*0.3;
	drawClassifiedPhonesGrid(painter, phonesOffsetX, marker.ClassifiedFrames, gridTopY, gridBotY);
}

void AudioSamplesWidget::drawShiftedFramesRuler(QPainter& painter, int phonemesBottomLine, int ruleBegSample, int rulerEndSample, int phoneRowHeight, int phoneRowsCount)
{
	using namespace PticaGovorun;
	int curY = phonemesBottomLine;
	int phonemeRowsAccum = 0;

	float docOffsetX = transcriberModel_->docOffsetX();

	for (long leftSideSampleInd = ruleBegSample; leftSideSampleInd < rulerEndSample; leftSideSampleInd += FrameShift)
	{
		long rightSideSampleInd = leftSideSampleInd + FrameSize;

		//

		float leftSideDocX = transcriberModel_->sampleIndToDocPosX(leftSideSampleInd);
		float rightSideDocX = transcriberModel_->sampleIndToDocPosX(rightSideSampleInd);

		int x1 = leftSideDocX - docOffsetX;
		int x2 = rightSideDocX - docOffsetX;

		painter.drawLine(x1, curY - phoneRowHeight / 2, x2, curY - phoneRowHeight / 2); // horizontal line
		painter.drawLine(x1, curY - phoneRowHeight, x1, curY); // left vertical line
		painter.drawLine(x2, curY - phoneRowHeight, x2, curY); // left vertical line

		curY -= phoneRowHeight;
		phonemeRowsAccum++;

		if (phonemeRowsAccum >= phoneRowsCount)
		{
			phonemeRowsAccum = 0;
			curY = phonemesBottomLine;
		}
	}
}

void AudioSamplesWidget::drawPhoneMarkersAndNames(QPainter& painter, long phonesOffsetSampleInd, const std::vector<PticaGovorun::AlignedPhoneme>& markerPhones, int markerBottomY, int maxPhoneMarkerHeight, int phoneTextY)
{
	if (markerPhones.empty())
		return;

	float docOffsetX = transcriberModel_->docOffsetX();

	int i = 0;
	for (const PticaGovorun::AlignedPhoneme& phone : markerPhones)
	{
		// align to the start of the segment
		long leftSampleInd = phonesOffsetSampleInd + phone.BegSample;
		long rightSampleInd = phonesOffsetSampleInd + phone.EndSample;

		float begSampleDocX = transcriberModel_->sampleIndToDocPosX(leftSampleInd);
		float endSampleDocX = transcriberModel_->sampleIndToDocPosX(rightSampleInd);

		// map to the current viewport
		int x1 = begSampleDocX - docOffsetX;
		int x2 = endSampleDocX - docOffsetX;

		// to ease the neighbour phones discrimination neighbour phones have markers of different color and height
		QColor markerColor(0, 255, 0);
		int choice = i % 3;
		float scaleFromMax = -1;
		if (choice == 0)
		{
			markerColor = QColor(255, 0, 0);
			scaleFromMax = 1;
		}
		else if (choice == 1)
		{
			markerColor = QColor(0, 0, 255);
			scaleFromMax = 0.8;
		}
		else if (choice == 2)
		{
			markerColor = QColor(0, 255, 0);
			scaleFromMax = 0.6;
		}
		painter.setPen(markerColor);

		int yTop = markerBottomY - maxPhoneMarkerHeight*scaleFromMax;
		painter.drawLine(x1, yTop, x1, markerBottomY); // left vertical line
		painter.drawLine(x2, yTop, x2, markerBottomY); // right vertical line
		painter.drawLine(x1, yTop, x2, yTop); // horiz line connects two vertical lines

		// phone name

		const int TextPad = 2; // pixels, prevent phone marker and phone name overlapping 
		//painter.drawText(x1 + TextPad, phoneTextY, QString::fromStdString(phone.Name)); // name to the right of the left vertical line
		painter.drawText((x1 + x2) / 2 - TextPad, markerBottomY - maxPhoneMarkerHeight - TextPad, QString::fromStdString(phone.Name)); // name above the horizontal line

		i++;
	}
}

void AudioSamplesWidget::drawClassifiedPhonesGrid(QPainter& painter, long phonesOffsetSampleInd, const std::vector<PticaGovorun::ClassifiedSpeechSegment>& markerPhones, int gridTopY, int gridBottomY)
{
	// y is directed from top to bottom

	if (markerPhones.empty())
		return;

	float docOffsetX = transcriberModel_->docOffsetX();

	int gridHeight = gridBottomY - gridTopY;
	int phonesCount = PticaGovorun::phoneMonoCount();
	int i = -1;
	std::string phoneName = "?";

	for (const PticaGovorun::ClassifiedSpeechSegment& phone : markerPhones)
	{
		i++;

		// align to the start of the segment
		long leftSampleInd = phonesOffsetSampleInd + phone.BegSample;
		long rightSampleInd = phonesOffsetSampleInd + phone.EndSample;

		float begSampleDocX = transcriberModel_->sampleIndToDocPosX(leftSampleInd);
		float endSampleDocX = transcriberModel_->sampleIndToDocPosX(rightSampleInd);

		// map to the current viewport
		int x1 = begSampleDocX - docOffsetX;
		int x2 = endSampleDocX - docOffsetX;

		auto maxIt = std::max_element(std::begin(phone.PhoneLogProbs), std::end(phone.PhoneLogProbs));
		int mostProbPhoneInd = maxIt - std::begin(phone.PhoneLogProbs);

		for (int phoneInd = 0; phoneInd < phonesCount; ++phoneInd)
		{
			float cellTopY = gridTopY + phoneInd / (float)phonesCount * gridHeight;
			float cellBotY = gridTopY + (phoneInd + 1) / (float)phonesCount * gridHeight;

			float prob = phone.PhoneLogProbs[phoneInd];

			// color from black to green
			QColor cellBgColor(0, 0, 0);
			QColor penColor(255, 255, 255);
			//if (prob < 0.2)
			//	cellBgColor = QColor(0, 0, 0); // black
			//else if (prob < 0.4)
			//	cellBgColor = QColor(0, 0, 255); // blue
			//else if (prob < 0.6)
			//	cellBgColor = QColor(125, 125, 0); // light yellow
			//else if (prob < 0.8)
			//	cellBgColor = QColor(255, 255, 0); // yellow
			//else
			//	cellBgColor = QColor(0, 255, 0); // green
			if (phoneInd == mostProbPhoneInd)
				cellBgColor = QColor(0, 255, 0); // green

			painter.setBrush(QBrush(cellBgColor));
			painter.setPen(penColor);

			painter.drawRect(x1, cellTopY, x2 - x1, cellBotY - cellTopY);

			// phone name

			const int TextPad = 2; // pixels, prevent phone marker and phone name overlapping 
			PticaGovorun::phoneIdToByPhoneName(phoneInd, phoneName);

			int percTens = (int)(prob * 10);
			//QString text = QString::fromStdString(phoneName);
			QString text = QString("%1").arg(percTens);
			painter.drawText((x1 + x2) / 2 - TextPad, (cellTopY + cellBotY) / 2 + TextPad, text);
		}
	}
}

void AudioSamplesWidget::mousePressEvent(QMouseEvent* me)
{
	const QPointF& pos = me->localPos();
	transcriberModel_->setLastMousePressPos(pos);
}

void AudioSamplesWidget::mouseReleaseEvent(QMouseEvent*)
{
	transcriberModel_->dragMarkerStop();
}

void AudioSamplesWidget::mouseMoveEvent(QMouseEvent* me)
{
	const QPointF& pos = me->localPos();
	transcriberModel_->dragMarkerContinue(pos);
}
