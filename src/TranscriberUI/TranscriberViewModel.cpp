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

	setCurrentFrameInd(0);

	emit audioSamplesLoaded();
}

void TranscriberViewModel::togglePlayPause()
{
	qDebug() << "togglePlayPause{ isPlaying=" <<isPlaying_;
	if (isPlaying_)
		soundPlayerPause();
	else
		soundPlayerPlay();
	qDebug() << "togglePlayPause} isPlaying=" << isPlaying_;
}

void TranscriberViewModel::soundPlayerPlay()
{
	auto frameInd = currentFrameInd();
	if (frameInd == -1)
		return;

	SoundPlayerData& data = soundPlayerData_;
	data.transcriberViewModel = this;
	data.FrameIndToPlay = frameInd;

	//
	PaError err = Pa_Initialize();
	if (err != paNoError)
		return;

	PaDeviceIndex deviceIndex = Pa_GetDefaultOutputDevice(); /* default output device */
	if (deviceIndex == paNoDevice) {
		qDebug() <<"Error: No default output device";
		return;
	}

	PaStreamParameters outputParameters;
	outputParameters.device = deviceIndex;
	outputParameters.channelCount = 2;       /* stereo output */
	outputParameters.sampleFormat = paInt16; /* 32 bit floating point output */
	outputParameters.suggestedLatency = Pa_GetDeviceInfo(deviceIndex)->defaultLowOutputLatency;
	outputParameters.hostApiSpecificStreamInfo = nullptr;

	auto patestCallback = [](const void *inputBuffer, void *outputBuffer,
		unsigned long framesPerBuffer,
		const PaStreamCallbackTimeInfo* timeInfo,
		PaStreamCallbackFlags statusFlags,
		void *userData) -> int
	{
		//qDebug() << "patestCallback";
		SoundPlayerData& data = *(SoundPlayerData*)userData;
		if (!data.allowPlaying)
			return paAbort;

		TranscriberViewModel& transcriberViewModel = *data.transcriberViewModel;

		long sampleInd = data.FrameIndToPlay;
		long sampleEnd = std::min((long)transcriberViewModel.audioSamples_.size(), sampleInd + (long)framesPerBuffer);
		int availableFramesCount = sampleEnd - sampleInd;
		if (availableFramesCount < framesPerBuffer) // the last buffer
			return paAbort;

		// updates current frame
		transcriberViewModel.setCurrentFrameInd(sampleInd);

		// populate samples buffer

		short *out = (short*)outputBuffer;
		for (unsigned long i = 0; i<availableFramesCount; i++)
		{
			short sampleValue = transcriberViewModel.audioSamples_[sampleInd + i];
			*out++ = sampleValue;  // left
			*out++ = sampleValue;  // right
		}

		// set the frame to play on the next request of audio samples
		data.FrameIndToPlay = sampleEnd;
		return paContinue;
	};

	//int FRAMES_PER_BUFFER = 8;
	int FRAMES_PER_BUFFER = int(1 / scale_) + 1;
	PaStream *stream;
	err = Pa_OpenStream(
		&stream,
		nullptr, /* no input */
		&outputParameters,
		SampleRate,
		FRAMES_PER_BUFFER,
		paClipOff,      /* we won't output out of range samples so don't bother clipping them */
		patestCallback,
		&data);
	if (err != paNoError)
		return;

	data.stream = stream;

	auto StreamFinished = [](void* userData) -> void
	{
		// stream is closed when audio data finishes or
		// when the client stops the stream
		qDebug() << "StreamFinished";
		SoundPlayerData& data = *(SoundPlayerData*)userData;

		qDebug() << "Closing stream";
		PaStream* stream = data.stream;
		PaError err = Pa_CloseStream(stream);
		if (err != paNoError)
		{
			qDebug() << "Error in Pa_CloseStream: " << Pa_GetErrorText(err);
			return;
		}

		qDebug() << "Pa_Terminate";

		err = Pa_Terminate();
		if (err != paNoError)
			qDebug() << "Pa_Terminate failed";
		data.transcriberViewModel->isPlaying_ = false;
	};

	err = Pa_SetStreamFinishedCallback(stream, StreamFinished);
	if (err != paNoError)
		return;
	
	// play

	soundPlayerData_.allowPlaying = true;
	isPlaying_ = true;

	err = Pa_StartStream(stream);
	if (err != paNoError)
		return;

	// Play for %d seconds
	//Pa_Sleep(1000);


}

void TranscriberViewModel::soundPlayerPause()
{
	qDebug() << "Pause. Stopping stream";
	soundPlayerData_.allowPlaying = false;

	PaStream *stream = soundPlayerData_.stream;
	PaError err = Pa_StopStream(stream);
	if (err != paNoError)
		return;

	// note, stream is closed in the Pa_SetStreamFinishedCallback
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

float TranscriberViewModel::sampleIndToDocPosX(long sampleInd) const
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

	emit lastMouseDocPosXChanged(lastMousePressDocPosX);

	//
	auto curFrameInd_ = (long)docPosXToSampleInd(lastMousePressDocPosX);
	setCurrentFrameInd(curFrameInd_);
}

long TranscriberViewModel::currentFrameInd() const
{
	return currentFrameInd_;
}

void TranscriberViewModel::setCurrentFrameInd(long value)
{
	if (currentFrameInd_ != value)
	{
		currentFrameInd_ = value;
		emit currentFrameIndChanged();
	}
}