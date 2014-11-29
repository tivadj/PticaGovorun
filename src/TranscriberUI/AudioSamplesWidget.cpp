#include "AudioSamplesWidget.h"
#include <QPainter>
#include <QBrush>
#include <QDebug>
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

	QColor markerColor(0, 255, 0);
	painter.setPen(markerColor);
	drawFrameIndMarkers(painter, canvasHeight, visibleDocLeft, visibleDocRight);

	// draw current frame adornment

	QColor curMarkerColor(0, 0, 0);
	painter.setPen(curMarkerColor);
	
	long currentFrameInd = transcriberModel_->currentFrameInd();
	float curFrameDocX = transcriberModel_->sampleIndToDocPosX(currentFrameInd);
	curFrameDocX -= transcriberModel_->docOffsetX();
	painter.drawLine(curFrameDocX, 0, curFrameDocX, canvasHeight);
}

void AudioSamplesWidget::drawFrameIndMarkers(QPainter& painter, int markerHeight, float visibleDocLeft, float visibleDocRight)
{
	auto docOffsetX = transcriberModel_->docOffsetX();

	for (const PticaGovorun::TimePointMarker& marker : transcriberModel_->frameIndMarkers())
	{
		long frameInd = marker.SampleInd;
		float frameDocX = transcriberModel_->sampleIndToDocPosX(frameInd);
		
		// repaint only markers from invalidated region
		if (frameDocX < visibleDocLeft) continue;
		if (frameDocX > visibleDocRight) break;

		auto x = frameDocX - docOffsetX;

		painter.drawLine(x, 0, x, markerHeight);
	}
}

void AudioSamplesWidget::mousePressEvent(QMouseEvent* me)
{
	const QPointF& pos = me->localPos();
	transcriberModel_->setLastMousePressPos(pos);
}

void AudioSamplesWidget::keyPressEvent(QKeyEvent* ke)
{
	if (ke->key() == Qt::Key_C)
		transcriberModel_->soundPlayerPlay();
	else if (ke->key() == Qt::Key_F)
		transcriberModel_->togglePlayPause();
	else if (ke->key() == Qt::Key_Space)
		transcriberModel_->soundPlayerPause();
	else
		QWidget::keyPressEvent(ke);
}