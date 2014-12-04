#ifndef AUDIOSAMPLESWIDGET_H
#define AUDIOSAMPLESWIDGET_H

#include <memory>
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
	void keyPressEvent(QKeyEvent*) override;
private:
	void drawFrameIndMarkers(QPainter& painter, int markerHeight, float visibleDocLeft, float visibleDocRight);
	void drawMarkerRecognizedText(QPainter& painter, const PticaGovorun::TimePointMarker& marker, float frameDocX);
	void drawPhonemesAndPhonemeMarkers(QPainter& painter, int markerHeight, float visibleDocLeft, float visibleDocRight);

	// Draw the delimiter cells to show range of phones.
	// The phones are constructed when audio is split into windows by speech recognition library.
	// Window has width FrameSize. Window 'shifts' on top of audio samples with interval FrameShift.
	// The ruler is drawn above the 'phonemesBottomLine' in X interval (BegSample; EndSample)
	void drawShiftedFramesRuler(QPainter& painter, int phonemesBottomLine, int rulerBegSample, int rulerEndSample, int phoneRowHeight, int phoneRowsCount);

	// phonesOffsetX the origin from which all phones are calculated.
	void drawPhoneMarkersAndNames(QPainter& painter, long phonesOffsetX, const std::vector<PticaGovorun::AlignedPhoneme>& markerPhones, int markerBottomY, int maxPhoneMarkerHeight, int phoneTextY);
private:
	std::shared_ptr<TranscriberViewModel> transcriberModel_;
};

#endif // AUDIOSAMPLESWIDGET_H
