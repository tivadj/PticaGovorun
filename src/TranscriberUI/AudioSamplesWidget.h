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
private:
	std::shared_ptr<TranscriberViewModel> transcriberModel_;
};

#endif // AUDIOSAMPLESWIDGET_H
