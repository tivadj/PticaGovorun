#include <sstream>
#include <QStandardPaths>
#include <QPointF>
#include <QDebug>
#include <QDir>
#include <QApplication>
#include "PticaGovorunBackend/SoundUtils.h"
#include "TranscriberViewModel.h"

static const decltype(paComplete) SoundPlayerCompleteOrAbortTechnique = paComplete; // paComplete or paAbort

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

	//
	loadAudioMarkupFromXml();

	//

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

	auto patestCallbackAbort = [](const void *inputBuffer, void *outputBuffer,
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

	auto patestCallbackComplete = [](const void *inputBuffer, void *outputBuffer,
		unsigned long framesPerBuffer,
		const PaStreamCallbackTimeInfo* timeInfo,
		PaStreamCallbackFlags statusFlags,
		void *userData) -> int
	{
		//qDebug() << "patestCallback";

		short *out = (short*)outputBuffer;

		SoundPlayerData& data = *(SoundPlayerData*)userData;
		if (!data.allowPlaying)
		{
			// reset the buffer
			static const int NumChannels = 2; // Stereo=2, Mono=1
			static const int FrameSize = sizeof(decltype(*out)) * NumChannels;
			std::memset(out, 0, FrameSize * framesPerBuffer);
			return paComplete;
		}

		TranscriberViewModel& transcriberViewModel = *data.transcriberViewModel;

		long sampleInd = data.FrameIndToPlay;
		long sampleEnd = std::min((long)transcriberViewModel.audioSamples_.size(), sampleInd + (long)framesPerBuffer);
		int availableFramesCount = sampleEnd - sampleInd;

		// updates current frame
		transcriberViewModel.setCurrentFrameInd(sampleInd);

		// populate samples buffer

		for (unsigned long i = 0; i<availableFramesCount; i++)
		{
			short sampleValue = transcriberViewModel.audioSamples_[sampleInd + i];
			*out++ = sampleValue;  // left
			*out++ = sampleValue;  // right
		}

		// trailing bytes are zero
		for (unsigned long i = availableFramesCount; i<framesPerBuffer; i++)
		{
			short sampleValue = transcriberViewModel.audioSamples_[sampleInd + i];
			*out++ = 0;  // left
			*out++ = 0;  // right
		}

		// set the frame to play on the next request of audio samples
		data.FrameIndToPlay = sampleEnd;
		
		if (availableFramesCount < framesPerBuffer) // the last buffer
			return paComplete;

		return paContinue;
	};

	typedef int(*PaStreamCallbackFun)(
		const void *input, void *output,
		unsigned long frameCount,
		const PaStreamCallbackTimeInfo* timeInfo,
		PaStreamCallbackFlags statusFlags,
		void *userData);

	auto patestCallback = (SoundPlayerCompleteOrAbortTechnique == paComplete) ? (PaStreamCallbackFun)patestCallbackComplete : (PaStreamCallbackFun)patestCallbackAbort;

	// choose the buffer so the current frame is redrawn each N pixels
	//int FRAMES_PER_BUFFER = 8;
	static const int CurSampleJumpLatencyPix = 2; // N pixels
	int FRAMES_PER_BUFFER = int(CurSampleJumpLatencyPix / scale_);
	PaStream *stream;
	PaError err = Pa_OpenStream(
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
		qDebug() << "StreamFinished";
		SoundPlayerData& data = *(SoundPlayerData*)userData;

		// stream is closed when the client stops the stream
		// when data finishes and paAbort    is used, the client must close the stream
		// when data finishes and paComplete is used, the stream is closed automatically

		if (SoundPlayerCompleteOrAbortTechnique == paAbort)
		{
			qDebug() << "Closing stream";
			PaStream* stream = data.stream;
			PaError err = Pa_CloseStream(stream);
			if (err != paNoError)
			{
				qDebug() << "Error in Pa_CloseStream: " << Pa_GetErrorText(err);
				return;
			}
		}

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

void TranscriberViewModel::loadAudioMarkupFromXml()
{
	QFileInfo audioFileInfoObj(audioFilePath());
	QString audioMarkupFileName = audioFileInfoObj.completeBaseName() + ".xml";

	QDir absDir = audioFileInfoObj.absoluteDir();
	QString audioMarkupPathAbs = absDir.absoluteFilePath(audioMarkupFileName);
	
	//
	frameIndMarkers_.clear();
	PticaGovorun::loadAudioMarkupFromXml(audioMarkupPathAbs.toStdWString(), frameIndMarkers_);
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

const std::vector<PticaGovorun::TimePointMarker>& TranscriberViewModel::frameIndMarkers() const
{
	return frameIndMarkers_;
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
		long oldValue = currentFrameInd_;

		currentFrameInd_ = value;

		emit currentFrameIndChanged(oldValue);
	}
}