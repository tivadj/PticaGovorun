#include <sstream>
#include <hash_map>
#include <ctime>
#include <cstdlib>
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

	// initialize markers id cache
	usedMarkerIds_.clear(); // clear cache of used marker ids
	for (const auto& m : frameIndMarkers_)
		usedMarkerIds_.insert(m.Id);

	//

	setCurrentFrameInd(0);

	emit audioSamplesLoaded();
}

void TranscriberViewModel::soundPlayerPlay(const short* audioSouce, long startPlayingFrameInd, long finishPlayingFrameInd, bool restoreCurFrameInd)
{
	SoundPlayerData& data = soundPlayerData_;
	data.transcriberViewModel = this;
	data.AudioSouce = audioSouce;
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
		long sampleEnd = std::min((long)data.FinishPlayingFrameInd, sampleInd + (long)framesPerBuffer);
		int availableFramesCount = sampleEnd - sampleInd;
		if (availableFramesCount < framesPerBuffer) // the last buffer
			return paAbort;

		// updates current frame
		transcriberViewModel.setCurrentFrameInd(sampleInd);

		// populate samples buffer

		short *out = (short*)outputBuffer;
		for (unsigned long i = 0; i<availableFramesCount; i++)
		{
			short sampleValue = data.AudioSouce[sampleInd + i];
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
			short sampleValue = data.AudioSouce[sampleInd + i];
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

template <typename MarkerPred>
void TranscriberViewModel::transformMarkersIf(std::vector<MarkerRefToOrigin>& markersOfInterest, MarkerPred canSelectMarker)
{
	markersOfInterest.reserve(frameIndMarkers_.size());
	for (size_t i = 0; i < frameIndMarkers_.size(); ++i)
	{
		const PticaGovorun::TimePointMarker& marker = frameIndMarkers_[i];

		bool include = canSelectMarker(marker);
		if (include)
		{
			MarkerRefToOrigin markerEx{ marker, i };
			markersOfInterest.push_back(markerEx);
		}
	}
}

std::tuple<long, long> TranscriberViewModel::getFrameRangeToPlay(long curFrameInd, SegmentStartFrameToPlayChoice startFrameChoice, int* outLeftMarkerInd)
{
	PG_Assert(!audioSamples_.empty() && "Audio samples must be loaded");

	std::vector<MarkerRefToOrigin>& markers = markersOfInterestCache_;
	markers.clear();
	transformMarkersIf(markers, [](const PticaGovorun::TimePointMarker& m) { return m.StopsPlayback; });

	if (markers.empty())
	{
		// play from the start of audio
		// play to the end of audio
		if (outLeftMarkerInd != nullptr)
			*outLeftMarkerInd = -1;
		return std::make_tuple<long,long>(0,audioSamples_.size() - 1);
	}
	
	int leftMarkerInd = -1;
	int rightMarkerInd = -1;
	auto markerFrameIndSelector = [](const MarkerRefToOrigin& m) { return m.Marker.SampleInd;  };
	bool foundSegOp = findSegmentMarkerInds(markers, markerFrameIndSelector, curFrameInd, true, leftMarkerInd, rightMarkerInd);
	PG_Assert(foundSegOp && "The segment to play must be found");

	if (outLeftMarkerInd != nullptr)
		*outLeftMarkerInd = leftMarkerInd;

	long startPlayFrameInd = -1;
	long endPlayFrameInd = -1;

	if (leftMarkerInd == -1)
	{
		// current frameInd is before the first marker

		if (startFrameChoice == SegmentStartFrameToPlayChoice::CurrentCursor)
			startPlayFrameInd = curFrameInd;
		else if (startFrameChoice == SegmentStartFrameToPlayChoice::SegmentBegin)
			startPlayFrameInd = 0;

		// play to the closest right marker which is the first in the markers collection
		endPlayFrameInd = markers[rightMarkerInd].Marker.SampleInd;
	}
	else
	{
		if (startFrameChoice == SegmentStartFrameToPlayChoice::CurrentCursor)
			startPlayFrameInd = curFrameInd;
		else if (startFrameChoice == SegmentStartFrameToPlayChoice::SegmentBegin)
			startPlayFrameInd = markers[leftMarkerInd].Marker.SampleInd;

		if (rightMarkerInd == -1)
		{
			// the last segment is requested to play; play to the end of audio
			endPlayFrameInd = audioSamples_.size() - 1;
		}
		else
		{
			// play to the frameInd of the closest right marker
			endPlayFrameInd = markers[rightMarkerInd].Marker.SampleInd;
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
	long startFrameInd = std::get<0>(frameRangeToPlay);
	long finishFrameInd = std::get<1>(frameRangeToPlay);

	soundPlayerPlay(audioSamples_.data(), startFrameInd, finishFrameInd, true);
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

	soundPlayerPlay(audioSamples_.data(), curFrameInd, audioSamples_.size() - 1, false);
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

QString TranscriberViewModel::audioMarkupFilePathAbs() const
{
	QFileInfo audioFileInfoObj(audioFilePath());
	QString audioMarkupFileName = audioFileInfoObj.completeBaseName() + ".xml";

	QDir absDir = audioFileInfoObj.absoluteDir();
	QString audioMarkupPathAbs = absDir.absoluteFilePath(audioMarkupFileName);
	return audioMarkupPathAbs;
}

void TranscriberViewModel::loadAudioMarkupFromXml()
{
	QString audioMarkupPathAbs = audioMarkupFilePathAbs();
	
	//
	frameIndMarkers_.clear();
	PticaGovorun::loadAudioMarkupFromXml(audioMarkupPathAbs.toStdWString(), frameIndMarkers_);
}

void TranscriberViewModel::saveAudioMarkupToXml()
{
	QString audioMarkupPathAbs = audioMarkupFilePathAbs();
	
	PticaGovorun::saveAudioMarkupToXml(frameIndMarkers_, audioMarkupPathAbs.toStdWString());
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

void TranscriberViewModel::collectMarkersOfInterest(std::vector<MarkerRefToOrigin>& markersOfInterest, bool processPhoneMarkers)
{
	// remove phone-markers from collection

	markersOfInterest.reserve(frameIndMarkers_.size());
	for (size_t i = 0; i < frameIndMarkers_.size(); ++i)
	{
		const PticaGovorun::TimePointMarker& marker = frameIndMarkers_[i];

		//if (marker.LevelOfDetail == PticaGovorun::MarkerLevelOfDetail::Phone && !processPhoneMarkers)
		//	continue;
		if (!marker.StopsPlayback)
			continue;

		MarkerRefToOrigin markerEx {marker, i};
		markersOfInterest.push_back(markerEx);
	}
}

template <typename Markers, typename FrameIndSelector>
bool TranscriberViewModel::findSegmentMarkerInds(const Markers& markers, FrameIndSelector markerFrameIndSelector, long frameInd, bool acceptOutOfRangeFrameInd, int& leftMarkerInd, int& rightMarkerInd) const
{
	if (markers.empty())
		return false;

	int lastMarkerInd = markers.size() - 1;
	long lastFrameInd = markerFrameIndSelector(markers[lastMarkerInd]);
	if (!acceptOutOfRangeFrameInd && (frameInd < 0 || frameInd >= lastFrameInd)) // out of range of samples
		return false;

	// before the first marker?
	if (frameInd < markerFrameIndSelector(markers[0]))
	{
		leftMarkerInd = -1;
		rightMarkerInd = 0;
		return true;
	}

	// after the last marker?
	if (frameInd >= markerFrameIndSelector(markers[lastMarkerInd]))
	{
		leftMarkerInd = lastMarkerInd;
		rightMarkerInd = -1;
		return true;
	}

	auto hitMarkerIt = PticaGovorun::binarySearch(std::begin(markers), std::end(markers), frameInd, markerFrameIndSelector);
	int hitMarkerInd = std::distance(std::begin(markers), hitMarkerIt);

	leftMarkerInd = hitMarkerInd;
	rightMarkerInd = hitMarkerInd + 1;

	PG_Assert(leftMarkerInd >= 0 && "Marker index is out of range");
	PG_Assert(rightMarkerInd < markers.size() && "Marker index is out of range");
	return true;
}

int TranscriberViewModel::generateMarkerId()
{
	// generate random id
	int result;
	while (true)
	{
		result = 1 + rand() % (frameIndMarkers_.size() * 2); // +1 for id>0
		if (usedMarkerIds_.find(result) == std::end(usedMarkerIds_))
			break;
	}
	usedMarkerIds_.insert(result);

#if PG_DEBUG
	// ensure generated id doesn't collide with id of other markers
	for (size_t i = 0; i < frameIndMarkers_.size(); ++i)
	{
		const auto& marker = frameIndMarkers_[i];
		PG_Assert(marker.Id != result && "Generated marker id which collides with id of another marker");
	}
#endif

	return result;
}

void TranscriberViewModel::insertNewMarkerSafe(int newMarkerInd, const PticaGovorun::TimePointMarker& newMarker)
{
	const auto insPosIt = frameIndMarkers_.cbegin() + newMarkerInd;

#if PG_DEBUG
	// ensure all markers are sorted by FrameInd
	// compare to the right marker
	if (insPosIt != std::cend(frameIndMarkers_))
	{
		int rightFrameInd = insPosIt->SampleInd;
		PG_Assert(newMarker.SampleInd <= rightFrameInd && "New marker is out of FrameInd order");
	}
	// compare to the left marker
	if (insPosIt != std::cbegin(frameIndMarkers_))
	{
		auto leftMarkerIt = insPosIt - 1;
		int leftFrameInd = leftMarkerIt->SampleInd;
		PG_Assert(leftFrameInd <= newMarker.SampleInd && "New marker is out of FrameInd order");
	}
#endif

	frameIndMarkers_.insert(insPosIt, newMarker);
}

void TranscriberViewModel::setTemplateMarkerLevelOfDetail(PticaGovorun::MarkerLevelOfDetail levelOfDetail)
{
	templateMarkerLevelOfDetail_ = levelOfDetail;
}

PticaGovorun::MarkerLevelOfDetail TranscriberViewModel::templateMarkerLevelOfDetail() const
{
	return templateMarkerLevelOfDetail_;
}

void TranscriberViewModel::dragMarkerStart(const QPointF& localPos, int markerInd)
{
	qDebug() << "dragMarkerStart";
	draggingMarkerInd_ = markerInd;
	isDraggingMarker_ = true;
	draggingLastViewportPos_ = localPos;
}

void TranscriberViewModel::dragMarkerStop()
{
	if (isDraggingMarker_)
	{
		qDebug() << "dragMarkerStop";
		draggingMarkerInd_ = -1;
		isDraggingMarker_ = false;
	}
}

void TranscriberViewModel::dragMarkerContinue(const QPointF& localPos)
{
	if (isDraggingMarker_ && draggingLastViewportPos_ != localPos)
	{
		qDebug() << "dragMarkerContinue";

		float mouseMoveDocPosX = docOffsetX_ + localPos.x();
		long frameInd = (long)docPosXToSampleInd(mouseMoveDocPosX);

		frameIndMarkers_[draggingMarkerInd_].SampleInd = frameInd;
		draggingLastViewportPos_ = localPos;

		// redraw current marker
		emit audioSamplesChanged();
	}
}

void TranscriberViewModel::setLastMousePressPos(const QPointF& localPos)
{
	float lastMousePressDocPosX = docOffsetX_ + localPos.x();
	emit lastMouseDocPosXChanged(lastMousePressDocPosX);

	//std::stringstream msg;
	//msg.precision(2);
	//msg << std::fixed;
	//msg << lastMousePressDocPosX;

	//float sampleInd = docPosXToSampleInd(lastMousePressDocPosX);
	//msg << " FrameInd=" << sampleInd;
	//emit nextNotification(QString::fromStdString(msg.str()));

	// choose between dragging the marker or setting the cursor

	auto curFrameInd = (long)docPosXToSampleInd(lastMousePressDocPosX);

	long distToMarker = -1;
	auto markerFrameIndSelector = [](const PticaGovorun::TimePointMarker& m) { return m.SampleInd; };
	int closestMarkerInd = getClosestMarkerInd(frameIndMarkers_, markerFrameIndSelector, curFrameInd, &distToMarker);

	float distToMarkerPix = distToMarker * scale_;
	
	const long AcceptDragDelta = 5; // pix
	if (closestMarkerInd != -1 && distToMarkerPix < AcceptDragDelta)
	{
		dragMarkerStart(localPos, closestMarkerInd);
		// do not change cursor
	}
	else
	{
		// click on empty space in document
		// update cursor
		setCurrentFrameIndInternal(curFrameInd, true);
	}
}

long TranscriberViewModel::currentFrameInd() const
{
	return currentFrameInd_;
}

void TranscriberViewModel::setCurrentFrameInd(long value)
{
	setCurrentFrameIndInternal(value, true);
}

void TranscriberViewModel::setCurrentFrameIndInternal(long value, bool updateCurrentMarkerInd)
{
	if (currentFrameInd_ != value)
	{
		long oldValue = currentFrameInd_;

		currentFrameInd_ = value;

		emit currentFrameIndChanged(oldValue);

		// reset current marker
		if (updateCurrentMarkerInd)
		{
			int curMarkerInd = -1; // reset the marker
			setCurrentMarkerIndInternal(curMarkerInd, false);
		}
	}
}

void TranscriberViewModel::insertNewMarkerAtCursor()
{
	long curFrameInd = currentFrameInd();

	// find the insertion position in the markers collection

	int newMarkerInd = -1;
	int leftMarkerInd;
	int rightMarkerInd;
	auto markerFrameIndSelector = [](const PticaGovorun::TimePointMarker& m) { return m.SampleInd; };
	bool findSegOp = findSegmentMarkerInds(frameIndMarkers_, markerFrameIndSelector, curFrameInd, true, leftMarkerInd, rightMarkerInd);
	if (!findSegOp || leftMarkerInd == -1) // insert the first marker
		newMarkerInd = 0;
	else
		newMarkerInd = leftMarkerInd + 1; // next to the left marker

	PticaGovorun::TimePointMarker newMarker;
	newMarker.Id = generateMarkerId();
	newMarker.SampleInd = curFrameInd;
	newMarker.IsManual = true;
	newMarker.LevelOfDetail = templateMarkerLevelOfDetail_;
	newMarker.StopsPlayback = getDefaultMarkerStopsPlayback(templateMarkerLevelOfDetail_);
	insertNewMarkerSafe(newMarkerInd, newMarker);

	setCurrentMarkerIndInternal(newMarkerInd, true);

	// TODO: repaint only current marker
	emit audioSamplesChanged();
}

void TranscriberViewModel::deleteCurrentMarker()
{
	int markerInd = currentMarkerInd();
	if (markerInd == -1)
		return;

	frameIndMarkers_.erase(frameIndMarkers_.cbegin() + markerInd);

	setCurrentMarkerIndInternal(-1, false);
}

template <typename Markers, typename FrameIndSelector>
int TranscriberViewModel::getClosestMarkerInd(const Markers& markers, FrameIndSelector markerFrameIndSelector, long frameInd, long* distFrames) const
{
	*distFrames = -1;

	int leftMarkerInd = -1;
	int rightMarkerInd = -1;
	if (!findSegmentMarkerInds(markers, markerFrameIndSelector, frameInd, true, leftMarkerInd, rightMarkerInd))
	{
		// there is no closest marker
		return -1;
	}
	
	int closestMarkerInd = -1;
	
	if (leftMarkerInd == -1)
	{
		// current cursor is before the first marker
		closestMarkerInd = rightMarkerInd;
		*distFrames = frameIndMarkers_[rightMarkerInd].SampleInd - frameInd;
	}
	if (rightMarkerInd == -1)
	{
		// current cursor is to the right of the last marker
		closestMarkerInd = leftMarkerInd;
		*distFrames = frameInd - frameIndMarkers_[leftMarkerInd].SampleInd;
	}

	if (closestMarkerInd == -1)
	{
		// there are two candidate (left and right) markers to select, select closest

		long distLeft = frameInd - frameIndMarkers_[leftMarkerInd].SampleInd;
		PG_Assert(distLeft >= 0 && "Error calculating the distance from the current cursor to the left marker");

		long distRight = frameIndMarkers_[rightMarkerInd].SampleInd - frameInd;
		PG_Assert(distRight >= 0 && "Error calculating the distance from the current cursor to the right marker");

		if (distLeft < distRight)
		{
			closestMarkerInd = leftMarkerInd;
			*distFrames = distLeft;
		}
		else
		{
			closestMarkerInd = rightMarkerInd;
			*distFrames = distRight;
		}
	}
	
	PG_Assert(closestMarkerInd != -1 && "Error calculating the closest marker");
	PG_Assert(*distFrames >= 0 && "Error calculating the distance to the closest marker");

	return closestMarkerInd;
}

void TranscriberViewModel::selectMarkerClosestToCurrentCursor()
{
	long curFrameInd = currentFrameInd();

	long distToClosestMarker = -1;
	auto markerFrameIndSelector = [](const PticaGovorun::TimePointMarker& m) { return m.SampleInd; };
	int closestMarkerInd = getClosestMarkerInd(frameIndMarkers_, markerFrameIndSelector, curFrameInd, &distToClosestMarker);

	setCurrentMarkerIndInternal(closestMarkerInd, true);
}

void TranscriberViewModel::setCurrentMarkerIndInternal(int markerInd, bool updateCurrentFrameInd)
{
	if (currentMarkerInd_ != markerInd)
	{
		// updates current marker
		currentMarkerInd_ = markerInd;
		emit currentMarkerIndChanged();

		if (updateCurrentFrameInd && markerInd != -1)
		{
			// udpdate cursor position
			long newCurSampleInd = frameIndMarkers_[markerInd].SampleInd;
			setCurrentFrameIndInternal(newCurSampleInd, false);
		}
	}
}

int TranscriberViewModel::currentMarkerInd() const
{
	return currentMarkerInd_;
}

void TranscriberViewModel::setCurrentMarkerTranscriptText(const QString& text)
{
	if (currentMarkerInd_ == -1)
		return;
	frameIndMarkers_[currentMarkerInd_].TranscripText = text;
}

void TranscriberViewModel::setCurrentMarkerLevelOfDetail(PticaGovorun::MarkerLevelOfDetail levelOfDetail)
{
	if (currentMarkerInd_ == -1)
	{
		// apply user selection as a template for new markers
		templateMarkerLevelOfDetail_ = levelOfDetail;
	}
	else
	{
		// apply the value to current marker
		frameIndMarkers_[currentMarkerInd_].LevelOfDetail = levelOfDetail;

		// TODO: repaint only current marker
		emit audioSamplesChanged();
	}
}

void TranscriberViewModel::setCurrentMarkerStopOnPlayback(bool stopsPlayback)
{
	if (currentMarkerInd_ == -1)
		return;
	frameIndMarkers_[currentMarkerInd_].StopsPlayback = stopsPlayback;
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

	bool insertShortPause = true;
	std::vector<std::string> speechPhones;
	convertTextToPhoneList(leftMarker.TranscripText.toStdWString(), wordToPhoneListFun, insertShortPause, speechPhones);
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

void TranscriberViewModel::playSegmentComposingRecipe(QString recipe)
{
	composedAudio_.clear();

	QStringList lines = recipe.split('\n', QString::SkipEmptyParts);
	for (QString line : lines)
	{
		QStringList lineParts = line.split(';', QString::SkipEmptyParts);

		QString rangeToPlay = lineParts[1];
		QStringList fromToNumStrs = rangeToPlay.split('-', QString::SkipEmptyParts);

		QString fromMarkerIdStr = fromToNumStrs[0];
		QString toMarkerIdStr = fromToNumStrs[1];

		bool parseOp;
		int fromMarkerId = fromMarkerIdStr.toInt(&parseOp);
		if (!parseOp)
		{
			emit nextNotification("Can't parse fromMarkerInd=" + fromMarkerIdStr);
			return;
		}
		int toMarkerId = toMarkerIdStr.toInt(&parseOp);
		if (!parseOp)
		{
			emit nextNotification("Can't parse toMarkerInd=" + fromMarkerIdStr);
			return;
		}

		int fromMarkerInd = markerIndByMarkerId(fromMarkerId);
		int toMarkerInd = markerIndByMarkerId(toMarkerId);

		long fromFrameInd = frameIndMarkers_[fromMarkerInd].SampleInd;
		long toFrameInd = frameIndMarkers_[toMarkerInd].SampleInd;

		std::copy(std::begin(audioSamples_) + fromFrameInd, std::begin(audioSamples_) + toFrameInd, std::back_inserter(composedAudio_));
	}

	// play composed audio
	soundPlayerPlay(composedAudio_.data(), 0, composedAudio_.size(), false);
}

int TranscriberViewModel::markerIndByMarkerId(int markerId)
{
	for (size_t i = 0; i < frameIndMarkers_.size(); ++i)
	{
		const auto& marker = frameIndMarkers_[i];
		if (marker.Id == markerId)
			return i;
	}
	return -1;
}