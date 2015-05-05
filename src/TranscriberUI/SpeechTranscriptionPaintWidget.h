#ifndef AUDIOSAMPLESWIDGET_H
#define AUDIOSAMPLESWIDGET_H

#include <memory>
#include <functional>
#include <QWidget>
#include <QMouseEvent>
#include "SpeechTranscriptionViewModel.h"

class SpeechTranscriptionPaintWidget : public QWidget
{
    Q_OBJECT
public:
    explicit SpeechTranscriptionPaintWidget(QWidget *parent = 0);

	void setModel(std::shared_ptr<PticaGovorun::SpeechTranscriptionViewModel> transcriberModel);


public:
	signals:

public slots:

protected:
	void paintEvent(QPaintEvent*) override;
	void mousePressEvent(QMouseEvent*) override;
	void mouseReleaseEvent(QMouseEvent*) override;
	void mouseMoveEvent(QMouseEvent*) override;
private:
	// Draw entire model in a given viewport rectangle.
	void drawModel(QPainter& painter, const QRect& viewportRect, const QRect& invalidRect);

	// Draw amplitudes of samples. Horizontal axis is time.
	void drawWaveform(QPainter& painter, const QRect& viewportRect, float docLeft, float docRight);

	void drawCursorSingle(QPainter& painter, const QRect& viewportRect, float docLeft);
	void drawCursorRange(QPainter& painter, const QRect& viewportRect, float docLeft, float docRight);
	void drawMarkers(QPainter& painter, const QRect& viewportRect, float docLeft, float docRight);
	
	// Draw current playing sample
	void drawPlayingSampleInd(QPainter& painter);

	// Draw the delimiter cells to show range of phones.
	// The phones are constructed when audio is split into windows by speech recognition library.
	// Window has width FrameSize. Window 'shifts' on top of audio samples with interval FrameShift.
	// The ruler is drawn above the 'phonemesBottomLine' in X interval (BegSample; EndSample)
	void drawShiftedFramesRuler(QPainter& painter, int phonemesBottomLine, int rulerBegSample, int rulerEndSample, int phoneRowHeight, int phoneRowsCount, float laneOffsetDocX, int frameSize, int frameShift);

	// phonesOffsetSampleInd the origin from which all phones are calculated.
	void drawPhoneSeparatorsAndNames(QPainter& painter, long phonesOffsetSampleInd, const std::vector<PticaGovorun::AlignedPhoneme>& markerPhones, float laneOffsetDocX, int markerBottomY, int maxPhoneMarkerHeight, int phoneTextY);

	// Draw probability of each phone for each frame in the current segment.
	void drawClassifiedPhonesGrid(QPainter& painter, long phonesOffsetSampleInd, const std::vector<PticaGovorun::ClassifiedSpeechSegment>& markerPhones, float laneOffsetDocX, int gridTopY, int gridBottomY);

	// Draw recognized words boundaries.
	void drawWordSeparatorsAndNames(QPainter& painter, long firstWordSampleIndOffset, const std::vector<PticaGovorun::AlignedWord>& wordBounds, float laneOffsetDocX, int separatorTopY, int separatorBotY);

	// Draws visual elements associated with the segment of samples.
	void drawDiagramSegment(QPainter& painter, const QRect& viewportRect, const PticaGovorun::DiagramSegment& diagItem, float laneOffsetDocX);

	// Draw diagram elements, which are associated with the range of samples.
	void processVisibleDiagramSegments(QPainter& painter, float visibleDocLeft, float visibleDocRight, std::function<void(const PticaGovorun::DiagramSegment& diagItem)> onDiagItem);
private:
	std::shared_ptr<PticaGovorun::SpeechTranscriptionViewModel> transcriberModel_;
};

#endif // AUDIOSAMPLESWIDGET_H
