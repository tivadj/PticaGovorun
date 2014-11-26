#include "AudioSamplesWidget.h"
#include <QPainter>
#include <QBrush>
AudioSamplesWidget::AudioSamplesWidget(QWidget *parent) :
    QWidget(parent)
{
}

void AudioSamplesWidget::setModel(std::shared_ptr<TranscriberViewModel> transcriberModel)
{
	transcriberModel_ = transcriberModel;
}

void AudioSamplesWidget::paintEvent(QPaintEvent* pe)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QColor marginalRecColor(255, 0, 0);

	painter.setPen(marginalRecColor);
    painter.setBrush(Qt::white);
    //painter.setBackgroundMode(Qt::OpaqueMode);
	//painter.setBackground(QBrush(QColor(255, 255, 0), Qt::SolidPattern));
    painter.drawRect(5,5,width()-5, height()-5);

	//

	if (transcriberModel_ == nullptr)
		return;

	const auto& audioSamples = transcriberModel_->audioSamples();
	if (audioSamples.empty())
		return;

	float pixelsPerSample = transcriberModel_->scale_;

	float canvasHeightHalf = height() / 2.0f;

	QColor sampleColor(0, 0, 0);
	painter.setPen(sampleColor);

	const float XNull = -1;
	float prevX = XNull;
	float prevY = XNull;
	for (size_t sampleInd = 0; sampleInd < audioSamples.size(); ++sampleInd)
	{
		if (prevX > width())
			return;

		short sampleValue = audioSamples[sampleInd];
		
		float x = sampleInd * pixelsPerSample;

		float sampleYPerc = sampleValue / (float)std::numeric_limits<short>::max();
		float y = canvasHeightHalf - canvasHeightHalf * sampleYPerc * 0.8f;

		if (prevX != XNull)
		{
			painter.drawLine(QPointF(prevX, prevY), QPointF(x, y));
		}

		prevX = x;
		prevY = y;
	}

}
