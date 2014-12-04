#include <sstream>
#include <hash_map>
#include <ctime>
#include <QStandardPaths>
#include <QPointF>
#include <QDebug>
#include <QDir>
#include <QApplication>
#include <QTextCodec>
#include "PticaGovorunBackend/SoundUtils.h"
#include "TranscriberViewModel.h"
#include "PticaGovorunCore.h"
#include "ClnUtils.h"

static const decltype(paComplete) SoundPlayerCompleteOrAbortTechnique = paComplete; // paComplete or paAbort

TranscriberViewModel::TranscriberViewModel()
{
    QString fileName = QString::fromStdWString(L"PticaGovorun/2011-04-pynzenyk-q_11.wav");
    audioFilePathAbs_ = QStandardPaths::locate(QStandardPaths::DocumentsLocation, fileName, QStandardPaths::LocateFile);

	curRecognizerName_ = "shrekky";
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

	//
	docOffsetX_ = 0;

	const float diagWidth = 1000;
	float seg3sec = SampleRate * 1.5f;
	scale_ = diagWidth / seg3sec;

	// scale
	// 0.0302 for word analysis
	// *2 for phone analysis
	scale_ *= 2;

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

void TranscriberViewModel::soundPlayerPlay(long startPlayingFrameInd, long finishPlayingFrameInd, bool restoreCurFrameInd)
{
	SoundPlayerData& data = soundPlayerData_;
	data.transcriberViewModel = this;
#if PG_DEBUG
	data.StartPlayingFrameInd = startPlayingFrameInd;
#endif
	data.FinishPlayingFrameInd = finishPlayingFrameInd;
	data.CurPlayingFrameInd = startPlayingFrameInd;
	data.RestoreCurrentFrameInd = restoreCurFrameInd ? currentFrameInd() : PticaGovorun::PGFrameIndNull;

	//
	PaDeviceIndex deviceIndex = Pa_GetDefaultOutputDevice(); /* default output device */
	if (deviceIndex == paNoDevice) {
		qDebug() << "Error: No default output device";
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

		long sampleInd = data.CurPlayingFrameInd;
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
		data.CurPlayingFrameInd = sampleEnd;
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

		long sampleInd = data.CurPlayingFrameInd;
		long sampleEnd = std::min(data.FinishPlayingFrameInd, sampleInd + (long)framesPerBuffer);
		int availableFramesCount = sampleEnd - sampleInd;

		// updates current frame in UI
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
			*out++ = 0;  // left
			*out++ = 0;  // right
		}

		// set the frame to play on the next request of audio samples
		data.CurPlayingFrameInd = sampleEnd;

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

		// updates current frame in UI
		if (data.RestoreCurrentFrameInd != PticaGovorun::PGFrameIndNull)
			data.transcriberViewModel->setCurrentFrameInd(data.RestoreCurrentFrameInd);

		// parts of UI are not painted when cursor moves
		// redraw entire UI when playing completes
		emit data.transcriberViewModel->audioSamplesLoaded();

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
}

std::tuple<long, long> TranscriberViewModel::getFrameRangeToPlay(long curFrameInd, SegmentStartFrameToPlayChoice startFrameChoice, int* outLeftMarkerInd)
{
	PG_Assert(!audioSamples_.empty() && "Audio samples must be loaded");

	if (frameIndMarkers_.empty())
	{
		// play from the start of audio
		// play to the end of audio
		if (outLeftMarkerInd != nullptr)
			*outLeftMarkerInd = -1;
		return std::make_tuple<long,long>(0,audioSamples_.size() - 1);
	}
	
	long startPlayFrameInd = -1;
	long endPlayFrameInd = -1;

	int leftMarkerInd = findLeftCloseMarkerInd(curFrameInd);

	if (outLeftMarkerInd != nullptr)
		*outLeftMarkerInd = leftMarkerInd;

	if (leftMarkerInd == -1)
	{
		// current frameInd is before the first marker

		if (startFrameChoice == SegmentStartFrameToPlayChoice::CurrentCursor)
			startPlayFrameInd = curFrameInd;
		else if (startFrameChoice == SegmentStartFrameToPlayChoice::SegmentBegin)
			startPlayFrameInd = 0;

		// play to the closest right marker which is the first in the markers collection
		endPlayFrameInd = frameIndMarkers_[0].SampleInd;
	}
	else
	{
		if (startFrameChoice == SegmentStartFrameToPlayChoice::CurrentCursor)
			startPlayFrameInd = curFrameInd;
		else if (startFrameChoice == SegmentStartFrameToPlayChoice::SegmentBegin)
			startPlayFrameInd = frameIndMarkers_[leftMarkerInd].SampleInd;

		long rightMarkerInd = leftMarkerInd + 1;
		if (rightMarkerInd >= frameIndMarkers_.size())
		{
			// the last segment is requested to play; play to the end of audio
			endPlayFrameInd = audioSamples_.size() - 1;
		}
		else
		{
			// play to the frameInd of the closest right marker
			endPlayFrameInd = frameIndMarkers_[rightMarkerInd].SampleInd;
		}
	}
	
	PG_Assert(startPlayFrameInd != -1 && "Must be valid frameInd");
	PG_Assert(endPlayFrameInd != -1 && "Must be valid frameInd");
	PG_Assert(startPlayFrameInd < endPlayFrameInd && "Must be valid range of frames");

	//return std::make_tuple<long,long>(startPlayFrameInd, endPlayFrameInd); // TODO: error C2664: 'std::tuple<long,long> std::make_tuple<long,long>(long &&,long &&)' : cannot convert argument 1 from 'long' to 'long &&'
	return std::tuple<long,long>(startPlayFrameInd, endPlayFrameInd);
}

void TranscriberViewModel::soundPlayerPlayCurrentSegment(SegmentStartFrameToPlayChoice startFrameChoice)
{
	if (audioSamples_.empty())
		return;

	auto curFrameInd = currentFrameInd();
	if (curFrameInd < 0)
		curFrameInd = 0;


	// determine the frameInd to which to play audio
	auto frameRangeToPlay = getFrameRangeToPlay(curFrameInd, startFrameChoice);
	soundPlayerPlay(std::get<0>(frameRangeToPlay), std::get<1>(frameRangeToPlay), true);
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

void TranscriberViewModel::soundPlayerPlay()
{
	if (audioSamples_.empty())
		return;

	auto curFrameInd = currentFrameInd();
	if (curFrameInd == PticaGovorun::PGFrameIndNull)
		return;

	soundPlayerPlay(curFrameInd, audioSamples_.size() - 1, false);
}

void TranscriberViewModel::soundPlayerTogglePlayPause()
{
	qDebug() << "soundPlayerTogglePlayPause{ isPlaying=" << isPlaying_;
	if (isPlaying_)
		soundPlayerPause();
	else
		soundPlayerPlay();
	qDebug() << "soundPlayerTogglePlayPause} isPlaying=" << isPlaying_;
}

bool TranscriberViewModel::soundPlayerIsPlaying() const
{
	return isPlaying_;
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

int TranscriberViewModel::findLeftCloseMarkerInd(long frameInd)
{
	if (frameIndMarkers_.empty())
		return -1;

	// before the first marker?
	if (frameInd < frameIndMarkers_[0].SampleInd)
		return -1;

	// after the last marker?
	int lastFrameInd = frameIndMarkers_.size() - 1;
	if (frameInd >= frameIndMarkers_[lastFrameInd].SampleInd)
		return lastFrameInd;

	auto markerFrameIndSelector = [](const PticaGovorun::TimePointMarker& m) { return m.SampleInd; };
	auto hitMarkerIt = PticaGovorun::binarySearch(std::begin(frameIndMarkers_), std::end(frameIndMarkers_), frameInd, markerFrameIndSelector);
	int hitMarkerInd = std::distance(std::begin(frameIndMarkers_), hitMarkerIt);
	return hitMarkerInd;
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

void TranscriberViewModel::ensureRecognizerIsCreated()
{
	using namespace PticaGovorun;

	std::string recogName = curRecognizerName_.toStdString();

	// initialize the recognizer lazily
	if (recognizer_ == nullptr)
	{
		RecognizerSettings rs;
		if (recogName == "shrekky")
		{
			rs.FrameSize = FrameSize;
			rs.FrameShift = FrameShift;
			rs.SampleRate = SampleRate;
			rs.UseWsp = false;

			rs.DictionaryFilePath =    R"path(C:\devb\PticaGovorunProj\data\shrekky\shrekkyDic.voca)path";
			rs.LanguageModelFilePath = R"path(C:\devb\PticaGovorunProj\data\shrekky\shrekkyLM.blm)path";
			rs.AcousticModelFilePath = R"path(C:\devb\PticaGovorunProj\data\shrekky\shrekkyAM.bam)path";

			rs.LogFile = recogName + "-LogFile.txt";
			rs.FileListFileName = recogName + "-FileList.txt";
			rs.TempSoundFile = recogName + "-TmpAudioFile.wav";
			rs.CfgFileName = recogName + "-Config.txt";
			rs.CfgHeaderFileName = recogName + "-ConfigHeader.txt";
		}
		else if (recogName == "persian")
		{
			rs.FrameSize = FrameSize;
			rs.FrameShift = FrameShift;
			rs.SampleRate = SampleRate;
			rs.UseWsp = false; //?

			rs.DictionaryFilePath =    R"path(C:\progs\cygwin\home\mmore\voxforge_p111-117\lexicon\voxforge_lexicon)path";
			rs.LanguageModelFilePath = R"path(C:\progs\cygwin\home\mmore\voxforge_p111-117\auto\persian.ArpaLM.bi.bin)path";
			rs.AcousticModelFilePath = R"path(C:\progs\cygwin\home\mmore\voxforge_p111-117\auto\acoustic_model_files\hmmdefs)path";
			rs.TiedListFilePath =      R"path(C:\progs\cygwin\home\mmore\voxforge_p111-117\auto\acoustic_model_files\tiedlist)path";

			rs.LogFile = recogName + "-LogFile.txt";
			rs.FileListFileName = recogName + "-FileList.txt";
			rs.TempSoundFile = recogName + "-TmpAudioFile.wav";
			rs.CfgFileName = recogName + "-Config.txt";
			rs.CfgHeaderFileName = recogName + "-ConfigHeader.txt";
		}

		QTextCodec* pTextCodec = QTextCodec::codecForName(PGEncodingStr);
		auto textCodec = std::unique_ptr<QTextCodec, NoDeleteFunctor<QTextCodec>>(pTextCodec);

		std::tuple<bool, std::string, std::unique_ptr<JuliusToolWrapper>> newRecogOp =
			createJuliusRecognizer(rs, std::move(textCodec));
		if (!std::get<0>(newRecogOp))
		{
			auto err = std::get<1>(newRecogOp);
			emit nextNotification(QString::fromStdString(err));
			return;
		}

		recognizer_ = std::move(std::get<2>(newRecogOp));
	}
}

void TranscriberViewModel::recognizeCurrentSegment()
{
	ensureRecognizerIsCreated();

	if (recognizer_ == nullptr)
		return;

	int outLeftMarkerInd;
	auto curSeg = getFrameRangeToPlay(currentFrameInd(), SegmentStartFrameToPlayChoice::SegmentBegin, &outLeftMarkerInd);
	if (outLeftMarkerInd == -1) // not marker to associate recognized text with
		return;

	long curSegBeg = std::get<0>(curSeg);
	long curSegEnd = std::get<1>(curSeg);
	auto len = curSegEnd - curSegBeg;

	// pad the audio with silince
	PticaGovorun::padSilence(&audioSamples_[curSegBeg], len, silencePadAudioFramesCount_, audioSegmentBuffer_);

	PticaGovorun::JuiliusRecognitionResult recogResult;
	auto recogOp = recognizer_->recognize(FrameToSamplePicker, audioSegmentBuffer_.data(), audioSegmentBuffer_.size(), recogResult);
	if (!std::get<0>(recogOp))
	{
		auto err = std::get<1>(recogOp);
		emit nextNotification(QString::fromStdString(err));
		return;
	}

	if (!recogResult.AlignmentErrorMessage.empty())
		emit nextNotification(QString::fromStdString(recogResult.AlignmentErrorMessage));

#if PG_DEBUG
	long markerOffset = frameIndMarkers_[outLeftMarkerInd].SampleInd;
	for (const PticaGovorun::AlignedPhoneme& phone : recogResult.AlignedPhonemeSeq)
	{
		qDebug() << "[" << phone.BegFrame << "," << phone.EndFrameIncl << "] [" 
			<< markerOffset + phone.BegSample << "," << markerOffset + phone.EndSample << "]";
	}
#endif

	//
	PticaGovorun::TimePointMarker& leftMarker = frameIndMarkers_[outLeftMarkerInd];
	
	std::wstringstream recogText;
	for (const std::wstring& word : recogResult.TextPass1)
	{
		recogText << word << " ";
	}
	leftMarker.RecogSegmentText = QString::fromStdWString(recogText.str());

	recogText.str(L"");
	for (const std::wstring& word : recogResult.WordSeq)
	{
		recogText << word << " ";
	}
	leftMarker.RecogSegmentWords = QString::fromStdWString(recogText.str());

	leftMarker.RecogAlignedPhonemeSeq = recogResult.AlignedPhonemeSeq;
}

void TranscriberViewModel::ensureWordToPhoneListVocabularyLoaded()
{
	// TODO: vocabulary is recognizer dependent
	if (!wordToPhoneListDict_.empty())
		return;

	QString dicPath = QString::fromStdString(recognizer_->settings().DictionaryFilePath);
	QTextCodec* pTextCodec = QTextCodec::codecForName(PGEncodingStr);
	PG_Assert(pTextCodec != nullptr && "Can't load text encoding");

	clock_t startTime = std::clock();
	PticaGovorun::loadWordToPhoneListVocabulary(dicPath.toStdWString(), wordToPhoneListDict_, *pTextCodec);
	double elapsedTime1 = static_cast<double>(clock() - startTime) / CLOCKS_PER_SEC;
	emit nextNotification(QString("Loaded dictionary in %1 secs").arg(elapsedTime1, 2));
}

void TranscriberViewModel::alignPhonesForCurrentSegment()
{
	using namespace PticaGovorun;
	ensureRecognizerIsCreated();

	if (recognizer_ == nullptr)
		return;

	ensureWordToPhoneListVocabularyLoaded();

	int outLeftMarkerInd;
	auto curSeg = getFrameRangeToPlay(currentFrameInd(), SegmentStartFrameToPlayChoice::SegmentBegin, &outLeftMarkerInd);
	if (outLeftMarkerInd == -1) // not marker to associate recognized text with
		return;

	long curSegBeg = std::get<0>(curSeg);
	long curSegEnd = std::get<1>(curSeg);
	auto len = curSegEnd - curSegBeg;

	// convert
	auto& leftMarker = frameIndMarkers_[outLeftMarkerInd];
	if (leftMarker.TranscripText.isEmpty())
		return;

	auto wordToPhoneListFun = [this](const std::wstring& word, std::vector<std::string>& wordPhones)->void
	{
		const auto& wordDict = wordToPhoneListDict_;

		auto it = wordDict.find(word);
		if (it == std::end(wordDict))
			return;

		const std::vector<std::string>& dictWordPhones = it->second;
		std::copy(std::begin(dictWordPhones), std::end(dictWordPhones), std::back_inserter(wordPhones));
	};

	std::vector<std::string> speechPhones;
	convertTextToPhoneList(leftMarker.TranscripText.toStdWString(), wordToPhoneListFun, speechPhones);
	if (speechPhones.empty())
		return;

	// pad the audio with silince
	padSilence(&audioSamples_[curSegBeg], len, silencePadAudioFramesCount_, audioSegmentBuffer_);

	PticaGovorun::AlignmentParams alignmentParams;
	alignmentParams.FrameSize = recognizer_->settings().FrameSize;
	alignmentParams.FrameShift = recognizer_->settings().FrameShift;
	alignmentParams.TakeSample = FrameToSamplePicker;

	int phoneStateDistrributionTailSize = 5;
	PticaGovorun::PhoneAlignmentInfo alignmentResult;
	recognizer_->alignPhones(audioSegmentBuffer_.data(), audioSegmentBuffer_.size(), speechPhones, alignmentParams, phoneStateDistrributionTailSize, alignmentResult);

	leftMarker.TranscripTextPhones = alignmentResult;
}

size_t TranscriberViewModel::silencePadAudioFramesCount() const
{
	return silencePadAudioFramesCount_;
}

QString TranscriberViewModel::recognizerName() const
{
	return curRecognizerName_;
}

void TranscriberViewModel::setRecognizerName(const QString& filePath)
{
	curRecognizerName_ = filePath;
}