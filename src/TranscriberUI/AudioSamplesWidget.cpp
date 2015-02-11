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

void AudioSamplesWidget::setModel(std::shared_ptr<PticaGovorun::TranscriberViewModel> transcriberModel)
{
	transcriberModel_ = transcriberModel;
}

void AudioSamplesWidget::paintEvent(QPaintEvent* pe)
{
	QRect invalidRect = pe->rect();
	//qDebug() << "paintEvent" <<pe->rect();
    QPainter painter(this);
    //painter.setRenderHint(QPainter::Antialiasing);

    QColor marginalRecColor(255, 0, 0);

	painter.setPen(marginalRecColor);
    painter.setBrush(Qt::white);
    //painter.setBackgroundMode(Qt::OpaqueMode);
	//painter.setBackground(QBrush(QColor(255, 255, 0), Qt::SolidPattern));
	const int p = 15;
	painter.drawRect(p,p,width()-2*p, height()-2*p);

	float canvasWidth = width();
	float canvasHeight = height();
	drawModel(painter, QRect(0, 0, canvasWidth, canvasHeight), invalidRect);
}

void AudioSamplesWidget::drawModel(QPainter& painter, const QRect& viewportRect, const QRect& invalidRect)
{
	if (transcriberModel_ == nullptr)
		return;

	const auto& audioSamples = transcriberModel_->audioSamples();
	if (audioSamples.empty())
		return;

	int lanesCount = transcriberModel_->lanesCount();
	float canvasWidth = viewportRect.width();
	float canvasHeight = viewportRect.height();

	// paint optimization
	// determine lanes which may be invalid and paint only those lanes

	float laneHeight = canvasHeight / (float)lanesCount;
	int laneStartInd = invalidRect.top() / laneHeight;
	int laneEndIndIncl = invalidRect.bottom() / laneHeight;

	// stack horizontal lanes into vertical stack

	for (int laneInd = laneStartInd; laneInd <= laneEndIndIncl; laneInd++)
	{
		int viewLeftX = 0;
		int viewRightX = canvasWidth;

		// paint optimization
		// limit doc range to the invalid part of a canvas
		viewLeftX  = std::max( viewLeftX, invalidRect.left());
		viewRightX = std::min(viewRightX, invalidRect.right());

		// determine portion of the document for current lane

		float laneStartDocX = transcriberModel_->docOffsetX() + laneInd * canvasWidth;
		float docLeft = laneStartDocX + viewLeftX;
		float docRight = laneStartDocX + viewRightX;

		float laneTopY = viewportRect.top() + laneInd * laneHeight;
		QRect laneRect = viewportRect;
		laneRect.setTop(laneTopY);
		laneRect.setBottom(laneTopY + laneHeight);

		// cursor-rectangle in the background
		// draw in 'background' to avoid covering the content
		drawCursorRange(painter, laneRect, docLeft, docRight);

		drawWaveform(painter, laneRect, docLeft, docRight);
		
		auto diagItemDrawFun = [=, &painter](const PticaGovorun::DiagramSegment& diagItem)
		{
			drawDiagramSegment(painter, laneRect, diagItem, laneStartDocX);
		};
		processVisibleDiagramSegments(painter, docLeft, docRight, diagItemDrawFun);

		// draw sentence markers

		drawMarkers(painter, laneRect, docLeft, docRight);

		// cursor-'vertical bar'
		// draw in 'foreground' as the current cursor should always be visible
		drawCursorSingle(painter, laneRect, docLeft);
	}

	drawPlayingSampleInd(painter);
}

void AudioSamplesWidget::drawWaveform(QPainter& painter, const QRect& viewportRect, float visibleDocLeft, float visibleDocRight)
{
	const float XNull = -1;
	float prevX = XNull;
	float prevY = XNull;
	float canvasHeightHalf = viewportRect.height() / 2.0f;

	QColor sampleColor(0, 0, 0);
	painter.setPen(sampleColor);

	long firstVisibleSampleInd = transcriberModel_->docPosXToSampleInd(visibleDocLeft);
	firstVisibleSampleInd = std::max(0L, firstVisibleSampleInd); // make it >=0

	const auto& audioSamples = transcriberModel_->audioSamples();
	for (size_t sampleInd = firstVisibleSampleInd; sampleInd < audioSamples.size(); ++sampleInd)
	{
		float xPix = transcriberModel_->sampleIndToDocPosX(sampleInd);

		// passed beside the right side of the visible document position
		if (xPix > visibleDocRight)
			break;

		xPix -= visibleDocLeft; // compensate for lane offset

		// the optimization to avoid drawing lines from previous sample to
		// the current sample, when both are squeezed in the same pixel
		if (prevX != XNull && prevX == xPix)
			continue;

		short sampleValue = audioSamples[sampleInd];
		float sampleYPerc = sampleValue / (float)std::numeric_limits<short>::max();
		const float TakeYSpace = 0.99f;
		float y = viewportRect.top() + canvasHeightHalf - canvasHeightHalf * sampleYPerc * TakeYSpace;

		if (prevX != XNull)
		{
			painter.drawLine(QPointF(prevX, prevY), QPointF(xPix, y));
		}

		prevX = xPix;
		prevY = y;
	}
}

void AudioSamplesWidget::drawCursorSingle(QPainter& painter, const QRect& viewportRect, float docLeft)
{
	std::pair<long,long> cursor =  transcriberModel_->cursor();
	
	if (cursor.second != PticaGovorun::NullSampleInd) // is not a 'single sample' cursor
		return;

	float curSampleDocX = transcriberModel_->sampleIndToDocPosX(cursor.first);
	PticaGovorun::ViewportHitInfo hitInfo;
	if (!transcriberModel_->docPosToViewport(curSampleDocX, hitInfo))
		return;
	PticaGovorun::RectY laneBnd = transcriberModel_->laneYBounds(hitInfo.LaneInd);

	QColor curMarkerColor(0, 0, 0);
	painter.setPen(curMarkerColor);
	painter.drawLine(hitInfo.DocXInsideLane, laneBnd.Top, hitInfo.DocXInsideLane, laneBnd.Top + laneBnd.Height);
}

void AudioSamplesWidget::drawCursorRange(QPainter& painter, const QRect& viewportRect, float docLeft, float docRight)
{
	std::pair<long, long> cursor = transcriberModel_->cursor();
	if (cursor.second == PticaGovorun::NullSampleInd) // there is no 'range' of samples
		return;

	long curSampleLeft = std::min(cursor.first, cursor.second);
	long curSampleRight = std::max(cursor.first, cursor.second);

	float curLeftDocX = transcriberModel_->sampleIndToDocPosX(curSampleLeft);
	float curRightDocX = transcriberModel_->sampleIndToDocPosX(curSampleRight);

	// cursor is not visible in given document range
	if (curRightDocX < docLeft || curLeftDocX > docRight)
		return;

	// limit cursor boundary to given document range
	// perhaps, shift cursor left sample to the doc left side
	// right cursor - to the doc right side
	float recLeft = std::max(curLeftDocX, docLeft); 
	float recRight = std::min(curRightDocX, docRight);

	curLeftDocX -= docLeft;
	curRightDocX -= docLeft;

	QColor curMarkerColor(0, 0, 0);
	painter.setPen(curMarkerColor);
	painter.setBrush(Qt::lightGray);
	painter.drawRect(curLeftDocX, viewportRect.top(), curRightDocX - curLeftDocX, viewportRect.height());
}

void AudioSamplesWidget::drawPlayingSampleInd(QPainter& painter)
{
	long sampleInd = transcriberModel_->playingSampleInd();
	if (sampleInd == PticaGovorun::NullSampleInd)
		return;
	
	auto sampleDocX = transcriberModel_->sampleIndToDocPosX(sampleInd);
	
	PticaGovorun::ViewportHitInfo hitInfo;
	if (!transcriberModel_->docPosToViewport(sampleDocX, hitInfo))
		return;

	PticaGovorun::RectY yBnds = transcriberModel_->laneYBounds(hitInfo.LaneInd);

	QColor curPlayingSampleColor(36, 96, 46); // dark green, like in Audacity
	painter.setPen(curPlayingSampleColor);
	painter.drawLine(hitInfo.DocXInsideLane, yBnds.Top, hitInfo.DocXInsideLane, yBnds.Top + yBnds.Height);
}

void AudioSamplesWidget::drawMarkers(QPainter& painter, const QRect& viewportRect, float docLeft, float docRight)
{
	int visibleMarkerInd = -1;

	QColor wordMarkerColor(0, 255, 0); // green
	QColor phoneMarkerColor(255, 127, 39); // orange
	int markerHeightMax = viewportRect.height();
	int markerCenterY = viewportRect.top() + markerHeightMax / 2;
	int markerHalfHeightMax = markerHeightMax / 2;
	
	// TODO: perf, enumerate only markers which hit the target region

	const auto& markers = transcriberModel_->frameIndMarkers();
	for (size_t markerInd = 0; markerInd < markers.size(); ++markerInd)
	{
		const PticaGovorun::TimePointMarker& marker = markers[markerInd];

		long markerSampleInd = marker.SampleInd;
		float markerDocX = transcriberModel_->sampleIndToDocPosX(markerSampleInd);
		
		// repaint only markers from invalidated region
		if (markerDocX < docLeft) continue;
		if (markerDocX > docRight) break;

		visibleMarkerInd++;

		// draw marker

		auto x = markerDocX - docLeft;

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

		painter.drawLine(x, viewportRect.top(), x, viewportRect.bottom());

		// draw cross above the marker to indicate that it stops audio player
		if (marker.StopsPlayback)
		{
			const int Dx = 5;
			painter.drawLine(x - Dx, viewportRect.top(), x + Dx, viewportRect.top() + 2 * Dx);
			painter.drawLine(x - Dx, viewportRect.top() + 2 * Dx, x + Dx, viewportRect.top());
		}

		// draw marker id
		if (marker.LevelOfDetail == PticaGovorun::MarkerLevelOfDetail::Phone)
		{
			painter.drawText(x, markerCenterY - markerHalfHeight, QString("%1").arg(marker.Id));
		}

		// draw speech recognition results

		auto markerTextFun = [=, &painter](const PticaGovorun::TimePointMarker& marker, int markerViewportX, int markerViewportY)
		{
			static const int TextHeight = 16;

			if (!marker.TranscripText.isEmpty())
			{
				const int TextPad = 3; // offset text from vertical line
				painter.drawText(markerViewportX + TextPad, markerViewportY + TextHeight, marker.TranscripText);
			}
		};

		// the recognized text from the previous invisible marker may still be visible 
		if (visibleMarkerInd == 0 && markerInd >= 1)
		{
			const auto& prevMarker = markers[markerInd - 1];
			float prevMarkerDocX = transcriberModel_->sampleIndToDocPosX(prevMarker.SampleInd);
			float prevViewpX = prevMarkerDocX - docLeft;
			markerTextFun(prevMarker, prevViewpX, viewportRect.top());
		}
		markerTextFun(marker, x, viewportRect.top());
	}
}

void AudioSamplesWidget::drawShiftedFramesRuler(QPainter& painter, int phonemesBottomLine, int ruleBegSample, int rulerEndSample, int phoneRowHeight, int phoneRowsCount, float laneOffsetDocX, int frameSize, int frameShift)
{
	using namespace PticaGovorun;
	int curY = phonemesBottomLine;
	int phonemeRowsAccum = 0;

	for (long leftSideSampleInd = ruleBegSample; leftSideSampleInd < rulerEndSample; leftSideSampleInd += frameShift)
	{
		long rightSideSampleInd = leftSideSampleInd + frameSize;

		float leftSideDocX = transcriberModel_->sampleIndToDocPosX(leftSideSampleInd);
		float rightSideDocX = transcriberModel_->sampleIndToDocPosX(rightSideSampleInd);

		int x1 = leftSideDocX - laneOffsetDocX;
		int x2 = rightSideDocX - laneOffsetDocX;

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

void AudioSamplesWidget::drawPhoneSeparatorsAndNames(QPainter& painter, long phonesOffsetSampleInd, const std::vector<PticaGovorun::AlignedPhoneme>& markerPhones, float laneOffsetDocX, int markerBottomY, int maxPhoneMarkerHeight, int phoneTextY)
{
	if (markerPhones.empty())
		return;

	int i = 0;
	for (const PticaGovorun::AlignedPhoneme& phone : markerPhones)
	{
		// align to the start of the segment
		long leftSampleInd = phonesOffsetSampleInd + phone.BegSample;
		long rightSampleInd = phonesOffsetSampleInd + phone.EndSample;

		float begSampleDocX = transcriberModel_->sampleIndToDocPosX(leftSampleInd);
		float endSampleDocX = transcriberModel_->sampleIndToDocPosX(rightSampleInd);

		// map to the current viewport
		int x1 = begSampleDocX - laneOffsetDocX;
		int x2 = endSampleDocX - laneOffsetDocX;

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

void AudioSamplesWidget::drawClassifiedPhonesGrid(QPainter& painter, long phonesOffsetSampleInd, const std::vector<PticaGovorun::ClassifiedSpeechSegment>& markerPhones, float laneOffsetDocX, int gridTopY, int gridBottomY)
{
	// y is directed from top to bottom

	if (markerPhones.empty())
		return;

	int gridHeight = gridBottomY - gridTopY;
	int phonesCount = PticaGovorun::phoneMonoCount();
	std::string phoneName = "?";
	const int TextPad = 2; // pixels, prevent phone marker and phone name overlapping 

	auto rowTopY = [=](int rowInd) { return gridTopY + rowInd / (float)phonesCount * gridHeight; };

	{
		// draw title of each row
		long firstFrameSampleInd = phonesOffsetSampleInd + markerPhones[0].BegSample;
		float firstFrameX = transcriberModel_->sampleIndToDocPosX(firstFrameSampleInd);
		firstFrameX -= laneOffsetDocX;

		QColor headerPhoneColor(0, 0, 0); // black
		painter.setPen(headerPhoneColor);

		for (int rowInd = 0; rowInd < phonesCount; ++rowInd)
		{
			float cellTopY = rowTopY(rowInd);
			float cellBotY = rowTopY(rowInd + 1);

			PticaGovorun::phoneIdToByPhoneName(rowInd, phoneName);
			QString text = QString::fromStdString(phoneName);

			const int PhoneNameWidth = 6;
			painter.drawText(firstFrameX - PhoneNameWidth, (cellTopY + cellBotY) / 2 + TextPad, text);
		}
	}

	int i = -1;
	for (const PticaGovorun::ClassifiedSpeechSegment& phone : markerPhones) // classified frames in columns
	{
		i++;

		// align to the start of the segment
		long leftSampleInd = phonesOffsetSampleInd + phone.BegSample;
		long rightSampleInd = phonesOffsetSampleInd + phone.EndSample;

		float begSampleDocX = transcriberModel_->sampleIndToDocPosX(leftSampleInd);
		float endSampleDocX = transcriberModel_->sampleIndToDocPosX(rightSampleInd);

		// map to the current viewport
		int x1 = begSampleDocX - laneOffsetDocX;
		int x2 = endSampleDocX - laneOffsetDocX;

		auto maxIt = std::max_element(std::begin(phone.PhoneLogProbs), std::end(phone.PhoneLogProbs));
		int mostProbPhoneInd = maxIt - std::begin(phone.PhoneLogProbs);

		for (int phoneInd = 0; phoneInd < phonesCount; ++phoneInd) // rows, vertical direction
		{
			float cellTopY = rowTopY(phoneInd);
			float cellBotY = rowTopY(phoneInd + 1);

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

			PticaGovorun::phoneIdToByPhoneName(phoneInd, phoneName);

			int percTens = (int)(prob * 10);
			//QString text = QString::fromStdString(phoneName);
			QString text = QString("%1").arg(percTens);
			painter.drawText((x1 + x2) / 2 - TextPad, (cellTopY + cellBotY) / 2 + TextPad, text);
		}
	}
}

void AudioSamplesWidget::drawWordSeparatorsAndNames(QPainter& painter, long firstWordSampleIndOffset, const std::vector<PticaGovorun::AlignedWord>& wordBounds, float laneOffsetDocX, int separatorTopY, int separatorBotY)
{
	if (wordBounds.empty())
		return;

	int sepHeightMax = separatorBotY - separatorTopY;

	{
		// draw min-max horizontal bars to indicate limits of word confidence
		long leftSampleInd  = firstWordSampleIndOffset + wordBounds[0].BegSample;
		long rightSampleInd = firstWordSampleIndOffset + wordBounds[wordBounds.size()-1].EndSample;

		float begSampleDocX = transcriberModel_->sampleIndToDocPosX(leftSampleInd);
		float endSampleDocX = transcriberModel_->sampleIndToDocPosX(rightSampleInd);

		// map to the current viewport
		int x1 = begSampleDocX - laneOffsetDocX;
		int x2 = endSampleDocX - laneOffsetDocX;

		QColor barColor(120, 120, 120);
		painter.setPen(barColor);
		painter.drawLine(x1, separatorTopY, x2, separatorTopY); // top vertical line
		painter.drawLine(x1, separatorBotY, x2, separatorBotY); // bottom vertical line
	}

	QColor separColor(0, 162, 232); // turquoise
	painter.setPen(separColor);

	for (const PticaGovorun::AlignedWord& wordBnd : wordBounds)
	{
		// align to the start of the segment
		long leftSampleInd = firstWordSampleIndOffset + wordBnd.BegSample;
		long rightSampleInd = firstWordSampleIndOffset + wordBnd.EndSample;

		float begSampleDocX = transcriberModel_->sampleIndToDocPosX(leftSampleInd);
		float endSampleDocX = transcriberModel_->sampleIndToDocPosX(rightSampleInd);

		// map to the current viewport
		int x1 = begSampleDocX - laneOffsetDocX;
		int x2 = endSampleDocX - laneOffsetDocX;

		// height of the bar is proportional to the word confidence
		int segHeight = sepHeightMax*wordBnd.Prob;
		int horizBarY = separatorBotY - segHeight;
		painter.drawLine(x1, horizBarY, x1, separatorBotY); // left vertical line
		painter.drawLine(x2, horizBarY, x2, separatorBotY); // right vertical line
		painter.drawLine(x1, horizBarY, x2, horizBarY); // horiz line connects two vertical lines

		// text

		const int TextPad = 2; // pixels, prevent vertical line and text overlapping 
		painter.drawText((x1 + x2) / 2 - TextPad, horizBarY - TextPad, wordBnd.Name); // name above the horizontal line
	}
}

void AudioSamplesWidget::drawDiagramSegment(QPainter& painter, const QRect& viewportRect, const PticaGovorun::DiagramSegment& diagItem, float laneOffsetDocX)
{
	using namespace PticaGovorun;

	// extend the segment with silence
	long paddedSegBegSample = diagItem.SampleIndBegin;
	long paddedSegEndSample = diagItem.SampleIndEnd;
	if (diagItem.RecogAlignedPhonemeSeqPadded)
	{
		paddedSegBegSample -= transcriberModel_->silencePadAudioFramesCount();
		paddedSegEndSample += transcriberModel_->silencePadAudioFramesCount();
	}

	{
		float canvasHeight = viewportRect.height();
		const int PhoneRowHeight = 10; // height of each phone's delimiter cell

		// make the number of phone rows so that each phone cell do not overlap with neighbour cell in the next column
		const int CellGapXPix = 10; // (in pixels) the width of the empty space between neighbour phone cells in one row
		float pixPerSample = transcriberModel_->pixelsPerSample();
		int PhoneRowsCount = 1 + (FrameSize * pixPerSample + CellGapXPix) / (FrameShift * pixPerSample);

		int phonemesBottomLine = viewportRect.bottom();

		// paint optimization, prevent drawing the phone ruler when audio plays
		if (transcriberModel_->phoneRulerVisible() && !transcriberModel_->soundPlayerIsPlaying())
		{
			QColor rulerColor(155, 155, 155);
			painter.setPen(rulerColor);
			int frameSize = FrameSize;
			int frameShift = FrameShift;
			drawShiftedFramesRuler(painter, phonemesBottomLine, paddedSegBegSample, paddedSegEndSample, PhoneRowHeight, PhoneRowsCount, laneOffsetDocX, frameSize, frameShift);
		}

		// recognized text split into phones

		int sepBotY = viewportRect.bottom();
		int sepHeight = viewportRect.height()*0.3;
		int phoneNameY = phonemesBottomLine - PhoneRowHeight*(PhoneRowsCount + 1); // +1 to jump to the upper line
		drawPhoneSeparatorsAndNames(painter, paddedSegBegSample, diagItem.RecogAlignedPhonemeSeq, laneOffsetDocX, sepBotY, sepHeight, phoneNameY);

		// transcription text split into phones

		int transcripTextMarkerBottomY = viewportRect.top() + canvasHeight*0.69;
		int transcripTextPhoneNameY = transcripTextMarkerBottomY + 16; // +height of text
		int maxPhoneMarkerHeight = canvasHeight*0.3;
		drawPhoneSeparatorsAndNames(painter, paddedSegBegSample, diagItem.TranscripTextPhones.AlignInfo, laneOffsetDocX, transcripTextMarkerBottomY, maxPhoneMarkerHeight, transcripTextPhoneNameY);
	}

	{
		// grid is in the top
		int gridTopY = viewportRect.top() + 20; // skip some space from top to avoid painting over text
		int gridBotY = gridTopY + viewportRect.height()*0.3;
		drawClassifiedPhonesGrid(painter, paddedSegBegSample, diagItem.ClassifiedFrames, laneOffsetDocX, gridTopY, gridBotY);
	}
	
	{
		// draw word boundaries
		float canvasHeight = viewportRect.height();
		int blockTopY = viewportRect.top() + canvasHeight*0.3;
		int blockBotY = viewportRect.top() + canvasHeight*0.7;
		drawWordSeparatorsAndNames(painter, paddedSegBegSample, diagItem.WordBoundaries, laneOffsetDocX, blockTopY, blockBotY);
	}
	
	{
		// draw recognized text

		long frameInd = paddedSegBegSample;
		float frameDocX = transcriberModel_->sampleIndToDocPosX(frameInd);
		auto x = frameDocX - laneOffsetDocX;

		static const int TextHeight = 16;

		if (!diagItem.TextToAlign.isEmpty())
		{
			painter.drawText(x, viewportRect.top() + TextHeight, diagItem.TextToAlign);
		}
		if (!diagItem.RecogSegmentText.isEmpty())
		{
			painter.drawText(x, viewportRect.top() + TextHeight * 2, diagItem.RecogSegmentText);
		}
		if (!diagItem.RecogSegmentWords.isEmpty())
		{
			painter.drawText(x, viewportRect.top() + TextHeight * 3, diagItem.RecogSegmentWords);
		}
	}
}

void AudioSamplesWidget::processVisibleDiagramSegments(QPainter& painter, float visibleDocLeft, float visibleDocRight,
	std::function<void(const PticaGovorun::DiagramSegment& diagItem)> onDiagItem)
{
	wv::slice<const PticaGovorun::DiagramSegment> diagItems = transcriberModel_->diagramSegments();
	for (const PticaGovorun::DiagramSegment diagItem : diagItems)
	{
		auto diagItemDocXBeg = transcriberModel_->sampleIndToDocPosX(diagItem.SampleIndBegin);
		auto diagItemDocXEnd = transcriberModel_->sampleIndToDocPosX(diagItem.SampleIndEnd);
		
		// diagram element is inside the visible range
		bool hitBeg = diagItemDocXBeg >= visibleDocLeft && diagItemDocXBeg <= visibleDocRight;
		bool hitEnd = diagItemDocXEnd >= visibleDocLeft && diagItemDocXEnd <= visibleDocRight;

		// visible range overlaps the diagram element
		bool visBeg = visibleDocLeft >= diagItemDocXBeg && visibleDocLeft <= diagItemDocXEnd;
		bool visEnd = visibleDocRight >= diagItemDocXBeg && visibleDocRight <= diagItemDocXEnd;

		bool visib = hitBeg || hitEnd || visBeg || visEnd;
		if (!visib)
			continue;

		onDiagItem(diagItem);
	}
}

void AudioSamplesWidget::mousePressEvent(QMouseEvent* me)
{
	const QPointF& pos = me->localPos();
	bool isShiftPressed = me->modifiers().testFlag(Qt::ShiftModifier);
	transcriberModel_->setLastMousePressPos(pos, isShiftPressed);
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

