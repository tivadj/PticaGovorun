#include <sstream>
#include <QStandardPaths>
#include <QPointF>
#include <QDebug>
#include <QApplication>
#include "PticaGovorunBackend/SoundUtils.h"
#include "TranscriberViewModel.h"

TranscriberViewModel::TranscriberViewModel()
{
    QString fileName = QString::fromStdWString(L"PticaGovorun/2011-04-pynzenyk-q_11.wav");
    audioFilePathAbs_ = QStandardPaths::locate(QStandardPaths::DocumentsLocation, fileName, QStandardPaths::LocateFile);
}

const int SampleRate = 22050;

void TranscriberViewModel::loadAudioFile()
{
	audioSamples_.clear();
    auto readOp = PticaGovorun::readAllSamples(audioFilePathAbs_.toStdString(), audioSamples_);
	if (!std::get<0>(readOp))
	{
		auto msg = std::get<1>(readOp);
		emit nextNotification(QString::fromStdString(msg));
		return;
    }

	//

	docOffsetX_ = 0;

	const float diagWidth = 1000;
	float seg3sec = SampleRate * 1.5f;
	scale_ = diagWidth / seg3sec;

	emit audioSamplesChanged();

	std::stringstream msg;
	msg << "Loaded: SamplesCount=" << audioSamples_.size();
	emit nextNotification(QString::fromStdString(msg.str()));
}

void TranscriberViewModel::togglePlayPause()
{
}

//void TranscriberViewModel::soundPlayerPlay()
//{
//	int sampleInd = (int)docPosXToSampleInd(lastMousePressDocPosX_);
//
//	if (sampleInd >= 0 && sampleInd < audioSamples_.size())
//	{
//		int len = audioSamples_.size() - sampleInd;
//		bool loadSamplesOp = soundBuffer_.loadFromSamples(&audioSamples_[sampleInd], len, 1, SampleRate);
//		if (!loadSamplesOp)
//			return;
//
//		soundObj_.setBuffer(soundBuffer_);
//		soundObj_.play();
//	}
//}
void TranscriberViewModel::soundPlayerPlay()
{
	// stop player
	while (soundObj_.getStatus() != sf::SoundSource::Stopped)
		soundObj_.stop();

	int sampleInd = (int)docPosXToSampleInd(lastMousePressDocPosX_);

	if (sampleInd >= 0 && sampleInd < audioSamples_.size())
	{
		const int packSize = 100;
		int i = sampleInd;
		while (true)
		{
			int endSample = i + packSize;
			endSample = std::min(endSample, (int)audioSamples_.size());
			//int len = audioSamples_.size() - endSample;
			int len = endSample - i;
			if (len <= 0)
				break;
			bool loadSamplesOp = soundBuffer_.loadFromSamples(&audioSamples_[i], len, 1, SampleRate);
			if (!loadSamplesOp)
				return;

			i = endSample;

			soundObj_.setBuffer(soundBuffer_);
			qDebug() << "push";
			soundObj_.play();

			while (soundObj_.getStatus() == sf::SoundSource::Playing)
				QApplication::processEvents();
		}		
	}
}

void TranscriberViewModel::soundPlayerPause()
{
	qDebug() << "Pause";
	soundObj_.pause();
}

QString TranscriberViewModel::audioFilePath() const
{
    return audioFilePathAbs_;
}

void TranscriberViewModel::setAudioFilePath(const QString& filePath)
{
    audioFilePathAbs_ = filePath;
}

float TranscriberViewModel::pixelsPerSample() const
{
	return scale_;
}

float TranscriberViewModel::docPaddingX() const
{
	return docPaddingPix_;
}

float TranscriberViewModel::docOffsetX() const
{
    return docOffsetX_;
}

void TranscriberViewModel::setDocOffsetX(float value)
{
    if (docOffsetX_ != value)
    {
        docOffsetX_ = value;
		emit docOffsetXChanged();
    }
}

float TranscriberViewModel::docWidthPix() const
{
    return audioSamples_.size() * scale_ + 2 * docPaddingPix_;
}

float TranscriberViewModel::firstVisibleSampleInd() const
{
	return docPosXToSampleInd(docOffsetX_);
}

float TranscriberViewModel::docPosXToSampleInd(float docPosX) const
{
	return (docPosX - docPaddingPix_) / scale_;
}

float TranscriberViewModel::sampleIndToDocPosX(int sampleInd) const
{
	return docPaddingPix_ + sampleInd * scale_;
}

const std::vector<short>& TranscriberViewModel::audioSamples() const
{
	return audioSamples_;
}

void TranscriberViewModel::setLastMousePressPos(const QPointF& localPos)
{
	float lastMousePressDocPosX = docOffsetX_ + localPos.x();
	lastMousePressDocPosX_ = lastMousePressDocPosX;
	emit lastMouseDocPosXChanged(lastMousePressDocPosX);
}