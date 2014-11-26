#include "TranscriberViewModel.h"
#include <QStandardPaths>
#include "PticaGovorunBackend/SoundUtils.h"

TranscriberViewModel::TranscriberViewModel()
{
    QString fileName = QString::fromStdWString(L"PticaGovorun/2011-04-pynzenyk-q_11.wav");
    audioFilePathAbs_ = QStandardPaths::locate(QStandardPaths::DocumentsLocation, fileName, QStandardPaths::LocateFile);
}

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

	const float diagWidth = 1000;
	const int SampleRate = 22050;
	float seg3sec = SampleRate * 1.5f;
	scale_ = diagWidth / seg3sec;

	emit audioSamplesChanged();
}

QString TranscriberViewModel::audioFilePath() const
{
    return audioFilePathAbs_;
}

void TranscriberViewModel::setAudioFilePath(const QString& filePath)
{
    audioFilePathAbs_ = filePath;
}

const std::vector<short>& TranscriberViewModel::audioSamples() const
{
	return audioSamples_;
}