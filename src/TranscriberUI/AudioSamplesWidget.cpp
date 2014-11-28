#include "AudioSamplesWidget.h"
#include <QPainter>
#include <QBrush>
#include <QDebug>
AudioSamplesWidget::AudioSamplesWidget(QWidget *parent) :
    QWidget(parent)
{
	setFocusPolicy(Qt::WheelFocus);
}

void AudioSamplesWidget::setModel(std::shared_ptr<TranscriberViewModel> transcriberModel)
{
	transcriberModel_ = transcriberModel;
}

void AudioSamplesWidget::paintEvent(QPaintEvent* pe)
{
	//qDebug() << "paintEvent";
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

	float canvasHeightHalf = height() / 2.0f;

	QColor sampleColor(0, 0, 0);
	painter.setPen(sampleColor);

	const float XNull = -1;
	float prevX = XNull;
	float prevY = XNull;

	// find first sample to draw
	int firstSampleInd = (int)transcriberModel_->firstVisibleSampleInd();
	firstSampleInd = std::max(0, firstSampleInd); // make it >=0

	for (size_t sampleInd = firstSampleInd; sampleInd < audioSamples.size(); ++sampleInd)
	{
		// passed beside the right side of the viewport
		if (prevX > width())
			break;

		float xPix = transcriberModel_->sampleIndToDocPosX(sampleInd);
		xPix -= transcriberModel_->docOffsetX(); // compensate for docOffsetX

		// the optimization to avoid drawing lines from previous sample to
		// the current sample, when both are squeezed in the same pixel
		if (prevX != XNull && prevX == xPix)
			continue;

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

	// draw current frame
	unsigned long currentFrameInd = transcriberModel_->currentFrameInd();
	float curFrameDocX = transcriberModel_->sampleIndToDocPosX(currentFrameInd);
	curFrameDocX -= transcriberModel_->docOffsetX();
	painter.drawLine(curFrameDocX, 0, curFrameDocX, height());
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
	else if (ke->key() == Qt::Key_Space)
		transcriberModel_->soundPlayerPause();
	else
		QWidget::keyPressEvent(ke);
}