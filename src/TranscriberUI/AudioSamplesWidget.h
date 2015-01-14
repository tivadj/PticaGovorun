#ifndef AUDIOSAMPLESWIDGET_H
#define AUDIOSAMPLESWIDGET_H

#include <memory>
#include <functional>
#include <QWidget>
#include <QMouseEvent>
#include "TranscriberViewModel.h"

class AudioSamplesWidget : public QWidget
{
    Q_OBJECT
public:
    explicit AudioSamplesWidget(QWidget *parent = 0);

	void setModel(std::shared_ptr<TranscriberViewModel> transcriberModel);


public:
	signals:

public slots:

protected:
	void paintEvent(QPaintEvent*) override;
	void mousePressEvent(QMouseEvent*) override;
	void mouseReleaseEvent(QMouseEvent*) override;
	void mouseMoveEvent(QMouseEvent*) override;
private:
	// This method draws cursor or samples range.
	// inForeground, true if this method called after all content is drawn; false, if before.
	void drawCursor(QPainter& painter, bool inForeground, float topY, float bottomY);
	void drawMarkers(QPainter& painter, float visibleDocLeft, float visibleDocRight, int markerTopY, int markerBotY);

	// Draw the delimiter cells to show range of phones.
	// The phones are constructed when audio is split into windows by speech recognition library.
	// Window has width FrameSize. Window 'shifts' on top of audio samples with interval FrameShift.
	// The ruler is drawn above the 'phonemesBottomLine' in X interval (BegSample; EndSample)
	void drawShiftedFramesRuler(QPainter& painter, int phonemesBottomLine, int rulerBegSample, int rulerEndSample, int phoneRowHeight, int phoneRowsCount);

	// phonesOffsetSampleInd the origin from which all phones are calculated.
	void drawPhoneSeparatorsAndNames(QPainter& painter, long phonesOffsetSampleInd, const std::vector<PticaGovorun::AlignedPhoneme>& markerPhones, int markerBottomY, int maxPhoneMarkerHeight, int phoneTextY);

	// Draw probability of each phone for each frame in the current segment.
	void drawClassifiedPhonesGrid(QPainter& painter, long phonesOffsetSampleInd, const std::vector<PticaGovorun::ClassifiedSpeechSegment>& markerPhones, int gridTopY, int gridBottomY);

	// Draws visual elements associated with the segment of samples.
	void drawDiagramSegment(QPainter& painter, const DiagramSegment& diagItem, int canvasHeight);

	// Draw diagram elements, which are associated with the range of samples.
	void processVisibleDiagramSegments(QPainter& painter, float visibleDocLeft, float visibleDocRight, std::function<void(const DiagramSegment& diagItem)> onDiagItem);
private:
	std::shared_ptr<TranscriberViewModel> transcriberModel_;
};

#endif // AUDIOSAMPLESWIDGET_H
