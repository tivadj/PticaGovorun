#include <sstream>
#include <hash_map>
#include <ctime>
#include <cstdlib>
#include <memory>
#include <array>

#include <QStandardPaths>
#include <QPointF>
#include <QDebug>
#include <QDir>
#include <QDateTime>
#include <QTextCodec>
#include <QSettings>

#include "WavUtils.h"
#include "TranscriberViewModel.h"
#include "PticaGovorunCore.h"
#include "ClnUtils.h"
//#include "algos_amp.cpp"
#include "InteropPython.h"
#include "SpeechProcessing.h"

#include <samplerate.h>

#if HAS_POCKETSPHINX
#include <pocketsphinx.h>
#include <pocketsphinx_internal.h> // ps_decoder_s
#include <fe_internal.h> // fe_s
#endif


#if HAS_MATLAB
#include "matrix.h" // mxArray, mxCreateLogicalArray
#include "PticaGovorunInteropMatlab.h"
#endif

const char* IniFileName = "TranscriberUI.ini"; // where to store settings
const char* WavFilePath = "WavFilePath"; // last opened wav file path
static const decltype(paComplete) SoundPlayerCompleteOrAbortTechnique = paComplete; // paComplete or paAbort

TranscriberViewModel::TranscriberViewModel()
{
}

void TranscriberViewModel::loadStateSettings()
{
	QSettings settings(IniFileName, QSettings::IniFormat);
	QString speechWavDirVar = settings.value(WavFilePath, QString("")).toString();
	audioFilePathAbs_ = speechWavDirVar;

	curRecognizerName_ = "shrekky";
}

void TranscriberViewModel::saveStateSettings()
{
	QSettings settings(IniFileName, QSettings::IniFormat);
	settings.setValue(WavFilePath, audioFilePathAbs_);
	settings.sync();
}

void TranscriberViewModel::loadAudioFileRequest()
{
	using namespace PticaGovorun;
	audioSamples_.clear();
	diagramSegments_.clear();

	auto readOp = PticaGovorun::readAllSamples(audioFilePathAbs_.toStdString(), audioSamples_, &audioFrameRate_);
	if (!std::get<0>(readOp))
	{
		auto msg = std::get<1>(readOp);
		emit nextNotification(QString::fromStdString(msg));
		return;
    }
	if (audioFrameRate_ != SampleRate)
		emit nextNotification("WARN: FrameRate != 22050. Perhaps other parameters (FrameSize, FrameShift) should be changed");

	//
	docOffsetX_ = 0;

	const float diagWidth = 1000;
	float seg3sec = audioFrameRate_ * 1.5f;
	scale_ = diagWidth / seg3sec;

	// scale
	// 0.0302 for word analysis
	// *2 for phone analysis
	scale_ *= 2;

	emit audioSamplesChanged();

	QString msg = QString("Loaded '%1' FrameRate=%2 FramesCount=%3").arg(audioFilePathAbs_).arg(audioFrameRate_).arg(audioSamples_.size());
	emit nextNotification(msg);

	//
	loadAudioMarkupFromXml();

	setCursorInternal(0, true ,false);

	emit audioSamplesLoaded();
}

void TranscriberViewModel::soundPlayerPlay(const short* audioSouce, long startPlayingFrameInd, long finishPlayingFrameInd, bool restoreCurFrameInd)
{
	using namespace PticaGovorun;
	assert(startPlayingFrameInd <= finishPlayingFrameInd && "Must play start <= end");

	if (soundPlayerIsPlaying())
		return;

	SoundPlayerData& data = soundPlayerData_;
	data.transcriberViewModel = this;
	data.AudioSouce = audioSouce;
#if PG_DEBUG
	data.StartPlayingFrameInd = startPlayingFrameInd;
#endif
	data.FinishPlayingFrameInd = finishPlayingFrameInd;
	data.CurPlayingFrameInd = startPlayingFrameInd;
	data.MoveCursorToLastPlayingPosition = !restoreCurFrameInd;

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
		transcriberViewModel.setCursorInternal(sampleInd, false, false);

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

		// updates playing sample in UI
		transcriberViewModel.setPlayingSampleInd(sampleInd, true);

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
		audioFrameRate_,
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

		if (data.MoveCursorToLastPlayingPosition)
		{
			long cursor = data.CurPlayingFrameInd;
			data.transcriberViewModel->setCursorInternal(cursor, true, false);
		}

		// hides current playing sample in UI
		data.transcriberViewModel->setPlayingSampleInd(PticaGovorun::NullSampleInd, false);

		// parts of UI are not painted when cursor moves
		// redraw entire UI when playing completes
		emit data.transcriberViewModel->audioSamplesChanged();

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
void TranscriberViewModel::transformMarkersIf(const std::vector<PticaGovorun::TimePointMarker>& markers, std::vector<MarkerRefToOrigin>& markersOfInterest, MarkerPred canSelectMarker)
{
	markersOfInterest.reserve(markers.size());
	for (size_t i = 0; i < markers.size(); ++i)
	{
		const PticaGovorun::TimePointMarker& marker = markers[i];

		bool include = canSelectMarker(marker);
		if (include)
		{
			MarkerRefToOrigin markerEx{ marker, i };
			markersOfInterest.push_back(markerEx);
		}
	}
}

std::tuple<long, long> TranscriberViewModel::getSampleRangeToPlay(long curSampleInd, SegmentStartFrameToPlayChoice startFrameChoice, int* outLeftMarkerInd)
{
	PG_Assert(!audioSamples_.empty() && "Audio samples must be loaded");

	auto cur = cursor();
	
	// if range is selected, then always operate on it
	if (cur.second != PticaGovorun::NullSampleInd)
	{
		long left = std::min(cur.first, cur.second);
		long right = std::max(cur.first, cur.second);
		return std::make_tuple(left, right);
	}

	std::vector<MarkerRefToOrigin>& markerRefs = markersOfInterestCache_;
	markerRefs.clear();
	transformMarkersIf(speechAnnot_.markers(), markerRefs, [](const PticaGovorun::TimePointMarker& m) { return m.StopsPlayback; });

	if (markerRefs.empty())
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
	bool foundSegOp = PticaGovorun::findSegmentMarkerInds(markerRefs, markerFrameIndSelector, curSampleInd, true, leftMarkerInd, rightMarkerInd);
	PG_Assert(foundSegOp && "The segment to play must be found");

	if (outLeftMarkerInd != nullptr)
	{
		if (leftMarkerInd == -1)
			*outLeftMarkerInd = -1;
		else
			*outLeftMarkerInd = markerRefs[leftMarkerInd].MarkerInd;
	}

	long startPlayFrameInd = -1;
	long endPlayFrameInd = -1;

	if (leftMarkerInd == -1)
	{
		// current frameInd is before the first marker

		if (startFrameChoice == SegmentStartFrameToPlayChoice::CurrentCursor)
			startPlayFrameInd = curSampleInd;
		else if (startFrameChoice == SegmentStartFrameToPlayChoice::SegmentBegin)
			startPlayFrameInd = 0;

		// play to the closest right marker which is the first in the markers collection
		endPlayFrameInd = markerRefs[rightMarkerInd].Marker.SampleInd;
	}
	else
	{
		if (startFrameChoice == SegmentStartFrameToPlayChoice::CurrentCursor)
			startPlayFrameInd = curSampleInd;
		else if (startFrameChoice == SegmentStartFrameToPlayChoice::SegmentBegin)
			startPlayFrameInd = markerRefs[leftMarkerInd].Marker.SampleInd;

		if (rightMarkerInd == -1)
		{
			// the last segment is requested to play; play to the end of audio
			endPlayFrameInd = audioSamples_.size() - 1;
		}
		else
		{
			// play to the frameInd of the closest right marker
			endPlayFrameInd = markerRefs[rightMarkerInd].Marker.SampleInd;
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

	auto curFrameInd = currentSampleInd();
	if (curFrameInd < 0)
		curFrameInd = 0;


	// determine the frameInd to which to play audio
	long curSegBeg;
	long curSegEnd;
	std::tie(curSegBeg, curSegEnd) = getSampleRangeToPlay(curFrameInd, startFrameChoice);

	soundPlayerPlay(audioSamples_.data(), curSegBeg, curSegEnd, true);
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

	auto curFrameInd = currentSampleInd();
	if (curFrameInd == PticaGovorun::NullSampleInd)
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

long TranscriberViewModel::playingSampleInd() const
{
	return playingSampleInd_;
}

void TranscriberViewModel::setPlayingSampleInd(long value, bool updateViewportOffset)
{
	if (playingSampleInd_ != value)
	{
		long oldValue = playingSampleInd_;
		playingSampleInd_ = value;
		emit playingSampleIndChanged(oldValue);

		if (updateViewportOffset)
		{
			float caretDocX = sampleIndToDocPosX(value);

			float rightVisDocX = docOffsetX_ + viewportSize_.width() * lanesCount_;

			bool outOfView = caretDocX > rightVisDocX;
			if (outOfView)
			{
				const float PadX = 50; // some pixels to keep to the left from caret when viewport shifts
				long newDocOffset = rightVisDocX - PadX;
				setDocOffsetX(newDocOffset);
			}
		}
	}
}

QString TranscriberViewModel::audioMarkupFilePathAbs() const
{
	QSettings settings(IniFileName, QSettings::IniFormat);
	QString speechWavDirStr = settings.value("SpeechWavDir", QString(".\\")).toString();
	QString speechAnnotDirStr = settings.value("SpeechAnnotDir", QString(".\\")).toString();

	std::wstring audioFilePathW = audioFilePath().toStdWString();
	std::wstring annotPathAbsW = PticaGovorun::speechAnnotationFilePathAbs(audioFilePathW, speechWavDirStr.toStdWString(), speechAnnotDirStr.toStdWString());
	QString annotPathAbs = QString::fromStdWString(annotPathAbsW);
	return annotPathAbs;
}

void TranscriberViewModel::loadAudioMarkupFromXml()
{
	QString audioMarkupPathAbs = audioMarkupFilePathAbs();
	if (!QFileInfo::exists(audioMarkupPathAbs))
	{
		emit nextNotification(QString("Can' find annotation file '%1'").arg(audioMarkupPathAbs));
		return;
	}
	
	//
	speechAnnot_.clear();
	bool loadOp;
	const char* errMsg;
	std::tie(loadOp, errMsg) = PticaGovorun::loadAudioMarkupFromXml(audioMarkupPathAbs.toStdWString(), speechAnnot_);
	if (!loadOp)
	{
		emit nextNotification(errMsg);
		return;
	}
}

void TranscriberViewModel::saveAudioMarkupToXml()
{
	std::stringstream msgValidate;
	speechAnnot_.validateMarkers(msgValidate);
	if (!msgValidate.str().empty())
		emit nextNotification(QString::fromStdString(msgValidate.str()));

	QString audioMarkupPathAbs = audioMarkupFilePathAbs();
	
	PticaGovorun::saveAudioMarkupToXml(speechAnnot_, audioMarkupPathAbs.toStdWString());
	
	QString msg = QString("[%1]: Saved xml markup.").arg(QTime::currentTime().toString("hh:mm:ss"));
	emit nextNotification(msg);
}

void TranscriberViewModel::saveCurrentRangeAsWavRequest()
{
	long frameLeft;
	long frameRight;
	std::tie(frameLeft, frameRight) = getSampleRangeToPlay(cursor_.first, SegmentStartFrameToPlayChoice::SegmentBegin, nullptr);

	long len = frameRight - frameLeft;
	
	audioSegmentBuffer_.resize(len);
	std::copy_n(&audioSamples_[frameLeft], len, audioSegmentBuffer_.data());
	bool writeOp;
	std::string msg;
	tie(writeOp, msg) = PticaGovorun::writeAllSamplesWav(&audioSamples_[frameLeft], len, "currentRange.wav", audioFrameRate_);
	if (!writeOp)
	{
		emit nextNotification(QString::fromStdString(msg));
		return;
	}
	emit nextNotification(QString("Wrote %1 frames to file").arg(len));
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

bool TranscriberViewModel::phoneRulerVisible() const
{
	return phoneRulerVisible_;
}

void TranscriberViewModel::scrollDocumentEndRequest()
{
	float maxDocWidth = docWidthPix();\
	scrollPageWithLimits(maxDocWidth);
}

void TranscriberViewModel::scrollDocumentStartRequest()
{
	scrollPageWithLimits(0);
}
\
void TranscriberViewModel::scrollPageForwardRequest()
{
	static const float PageWidthPix = 500;
	scrollPageWithLimits(docOffsetX_ + PageWidthPix);
}

void TranscriberViewModel::scrollPageBackwardRequest()
{
	static const float PageWidthPix = 500;
	scrollPageWithLimits(docOffsetX_ - PageWidthPix);
}

void TranscriberViewModel::scrollPageWithLimits(float newDocOffsetX)
{
	float maxDocWidth = docWidthPix();
	if (newDocOffsetX > maxDocWidth)
		newDocOffsetX = maxDocWidth;
	else if (newDocOffsetX < 0)
		newDocOffsetX = newDocOffsetX;
	setDocOffsetX(newDocOffsetX);
}

int TranscriberViewModel::lanesCount() const
{
	return lanesCount_;
}

void TranscriberViewModel::setLanesCount(int lanesCount)
{
	if (lanesCount_ != lanesCount && lanesCount > 0)
	{
		lanesCount_ = lanesCount;
		emit audioSamplesChanged();
	}
}

void TranscriberViewModel::increaseLanesCountRequest()
{
	int newLanesCount = lanesCount() + 1;
	setLanesCount(newLanesCount);
}
void TranscriberViewModel::decreaseLanesCountRequest()
{
	int newLanesCount = lanesCount() - 1;
	setLanesCount(newLanesCount);
}


QSizeF TranscriberViewModel::viewportSize() const
{
	return viewportSize_;
}

void TranscriberViewModel::setViewportSize(float viewportWidth, float viewportHeight)
{
	viewportSize_ = QSizeF(viewportWidth, viewportHeight);
}

float TranscriberViewModel::docPosXToSampleInd(float docPosX) const
{
	return (docPosX - docPaddingPix_) / scale_;
}

float TranscriberViewModel::sampleIndToDocPosX(long sampleInd) const
{
	return docPaddingPix_ + sampleInd * scale_;
}

bool TranscriberViewModel::viewportPosToLocalDocPosX(const QPointF& pos, float& docPosX) const
{
	if (pos.x() < 0 || pos.y() < 0 || pos.x() >= viewportSize_.width() || pos.y() >= viewportSize_.height())
		return false;

	// determine lane
	float laneHeight = viewportSize_.height() / lanesCount();
	int laneInd = (int)(pos.y() / laneHeight);

	// whole number of lanes
	docPosX = laneInd * viewportSize_.width(); 

	// portion on the last lane
	docPosX += pos.x();
	return true;
}

bool TranscriberViewModel::docPosToViewport(float docX, ViewportHitInfo& hitInfo) const
{
	// to the left of viewport?
	if (docX < docOffsetX_)
		return false;

	float viewportDocX = docX - docOffsetX_;

	// multiple lanes may be shown in a viewport
	float viewportDocWidth = viewportSize_.width() * lanesCount();

	// to the right of viewport?
	if (viewportDocX >= viewportDocWidth)
		return false;

	// determine lane
	float laneWidth = viewportSize_.width();
	int laneInd = (int)(viewportDocX / laneWidth);
	assert(laneInd < lanesCount_ && "DocPos must hit some lane");

	float insideLaneOffset = viewportDocX - laneInd * viewportSize_.width();
	
	hitInfo.LaneInd = laneInd;
	hitInfo.DocXInsideLane = insideLaneOffset;
	return true;
}

RectY TranscriberViewModel::laneYBounds(int laneInd) const
{
	float laneHeight = viewportSize_.height() / lanesCount();

	RectY bnd;
	bnd.Top = laneInd * laneHeight;
	bnd.Height = laneHeight;

	return bnd;
}

void TranscriberViewModel::deleteRequest(bool isControl)
{
	// delete current marker?

	if (isControl)
	{
		if (cursorKind() == TranscriberCursorKind::Single)
		{
			int markerInd = currentMarkerInd();
			bool delMarkerOp = deleteMarker(markerInd);
			if (delMarkerOp)
				return;
		}
		else if (cursorKind() == TranscriberCursorKind::Range)
		{
			long start;
			long end;
			std::tie(start, end) = cursorOrdered();

			// delete markers covered by cursor
			for (int markerInd = speechAnnot_.markers().size() - 1; markerInd >= 0; --markerInd)
			{
				long markerFrameInd = speechAnnot_.marker(markerInd).SampleInd;
				if (markerFrameInd >= start && markerFrameInd < end)
				{
					bool delMarkerOp = deleteMarker(markerInd);
					assert(delMarkerOp);
				}
			}
		}
	}

	std::pair<long, long> samplesRange = cursor();
	bool delDiagOp = deleteDiagramSegmentsAtCursor(samplesRange);
	if (delDiagOp)
		return;
}

void TranscriberViewModel::refreshRequest()
{
	loadAuxiliaryPhoneticDictionaryRequest();
}

const std::vector<short>& TranscriberViewModel::audioSamples() const
{
	return audioSamples_;
}

const wv::slice<const DiagramSegment> TranscriberViewModel::diagramSegments() const
{
	//return wv::make_view(diagramSegments_);
	return wv::make_view(diagramSegments_.data(), diagramSegments_.size());
}

void TranscriberViewModel::collectDiagramSegmentIndsOverlappingRange(std::pair<long, long> samplesRange, std::vector<int>& diagElemInds)
{
	int i = -1;
	for (const DiagramSegment diagItem : diagramSegments_)
	{
		++i;

		auto diagItemBeg = diagItem.SampleIndBegin;
		auto diagItemEnd = diagItem.SampleIndEnd;

		// diagram element is inside the visible range
		bool hitBeg = diagItemBeg >= samplesRange.first && diagItemBeg <= samplesRange.second;
		bool hitEnd = diagItemEnd >= samplesRange.first && diagItemEnd <= samplesRange.second;

		// visible range overlaps the diagram element
		bool visBeg = samplesRange.first >= diagItemBeg && samplesRange.first <= diagItemEnd;
		bool visEnd = samplesRange.second >= diagItemBeg && samplesRange.second <= diagItemEnd;

		bool visib = hitBeg || hitEnd || visBeg || visEnd;
		if (visib)
			diagElemInds.push_back(i);
	}
}

bool TranscriberViewModel::deleteDiagramSegmentsAtCursor(std::pair<long, long> samplesRange)
{
	if (samplesRange.first == PticaGovorun::NullSampleInd)
		return false;

	if (samplesRange.second == PticaGovorun::NullSampleInd)
		samplesRange.second = samplesRange.first;

	std::vector<int> diagElemInds;
	collectDiagramSegmentIndsOverlappingRange(samplesRange, diagElemInds);
	if (diagElemInds.empty())
		return false;

	for (auto it = diagElemInds.rbegin(); it != diagElemInds.rend(); ++it)
	{
		int indToRemove = *it;
		diagramSegments_.erase(diagramSegments_.begin() + indToRemove);
	}

	// redraw everything
	emit audioSamplesChanged();

	return true;
}

const std::vector<PticaGovorun::TimePointMarker>& TranscriberViewModel::frameIndMarkers() const
{
	return speechAnnot_.markers();
}

void TranscriberViewModel::setTemplateMarkerLevelOfDetail(PticaGovorun::MarkerLevelOfDetail levelOfDetail)
{
	templateMarkerLevelOfDetail_ = levelOfDetail;
}

PticaGovorun::MarkerLevelOfDetail TranscriberViewModel::templateMarkerLevelOfDetail() const
{
	return templateMarkerLevelOfDetail_;
}

PticaGovorun::SpeechLanguage TranscriberViewModel::templateMarkerSpeechLanguage() const
{
	return templateMarkerSpeechLanguage_;
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

		float mouseMoveLocalDocPosX = -1;
		if (!viewportPosToLocalDocPosX(localPos, mouseMoveLocalDocPosX))
			return;

		float mouseMoveDocPosX = docOffsetX_ + mouseMoveLocalDocPosX;
		long frameInd = (long)docPosXToSampleInd(mouseMoveDocPosX);

		speechAnnot_.marker(draggingMarkerInd_).SampleInd = frameInd;
		draggingLastViewportPos_ = localPos;

		// redraw current marker
		emit audioSamplesChanged();
	}
}

void TranscriberViewModel::setLastMousePressPos(const QPointF& localPos, bool isShiftPressed)
{
	float mousePressDocPosX = -1;
	if (!viewportPosToLocalDocPosX(localPos, mousePressDocPosX))
		return;

	mousePressDocPosX += docOffsetX_;

	// choose between dragging the marker or setting the cursor

	auto curSampleInd = (long)docPosXToSampleInd(mousePressDocPosX);

	long distToMarker = -1;
	int closestMarkerInd = speechAnnot_.getClosestMarkerInd(curSampleInd, &distToMarker);

	float distToMarkerPix = distToMarker * scale_;
	
	// determine if user wants to drag the marker

	const long AcceptDragDelta = 5; // pix
	bool doDragging = closestMarkerInd != -1 && distToMarkerPix < AcceptDragDelta;
	if (doDragging)
	{
		// if user clicks two times in the same position, then he doesn't want to drag
		// and wants to move the cursor
		if (lastMousePressDocPosX_ == mousePressDocPosX)
			doDragging = false;
	}
	
	if (doDragging)
	{

		dragMarkerStart(localPos, closestMarkerInd);
		// do not change cursor
	}
	else
	{
		auto newCursor = cursor_;

		// click on empty space in document
		if (isShiftPressed)
		{
			// update current samples range
			// retain the current cursor, but change the second cursor
			newCursor.second = curSampleInd;
		}
		else
		{
			// update cursor
			newCursor.first = curSampleInd;
			newCursor.second = PticaGovorun::NullSampleInd;
		}
		setCursorInternal(newCursor, true, false);
	}

	lastMousePressDocPosX_ = mousePressDocPosX;
	emit lastMouseDocPosXChanged(mousePressDocPosX);
}

long TranscriberViewModel::currentSampleInd() const
{
	return cursor_.first;
}

void TranscriberViewModel::setCursorInternal(long curSampleInd, bool updateCurrentMarkerInd, bool updateViewportOffset)
{
	auto cursor = cursor_;
	cursor.first = curSampleInd;
	cursor.second = PticaGovorun::NullSampleInd;

	setCursorInternal(cursor, updateCurrentMarkerInd, updateViewportOffset);
}

void TranscriberViewModel::setCursorInternal(std::pair<long,long> value, bool updateCurrentMarkerInd, bool updateViewportOffset)
{
	if (cursor_ != value)
	{
		auto oldValue = cursor_;

		cursor_ = value;
		emit cursorChanged(oldValue);

		// reset current marker
		if (updateCurrentMarkerInd)
		{
			int curMarkerInd = -1;
			if (cursorKind() == TranscriberCursorKind::Single || cursorKind() == TranscriberCursorKind::Range)
			{
				// keep active current segment, associated with the marker left to the cursor

				std::vector<MarkerRefToOrigin>& markerRefs = markersOfInterestCache_;
				markerRefs.clear();
				transformMarkersIf(speechAnnot_.markers(), markerRefs, [](const PticaGovorun::TimePointMarker& m) { return m.StopsPlayback; });

				int leftMarkerInd = -1;
				int rightMarkerInd = -1;
				auto markerFrameIndSelector = [](const MarkerRefToOrigin& m) { return m.Marker.SampleInd;  };
				bool foundSegOp = PticaGovorun::findSegmentMarkerInds(markerRefs, markerFrameIndSelector, cursor_.first, true, leftMarkerInd, rightMarkerInd);
				if (foundSegOp && leftMarkerInd != -1)
					curMarkerInd = markerRefs[leftMarkerInd].MarkerInd;
			}
			else
				curMarkerInd = -1; // reset the marker

			setCurrentMarkerIndInternal(curMarkerInd, false, updateViewportOffset);
		}

		if (updateViewportOffset && cursor_.first != PticaGovorun::NullSampleInd)
		{
			float cursorFirstDocX = sampleIndToDocPosX(cursor_.first);
			float rightVisDocX = docOffsetX_ + viewportSize_.width() * lanesCount_;
			bool cursorVisible = cursorFirstDocX >= docOffsetX_ && cursorFirstDocX < rightVisDocX;
			if (!cursorVisible)
			{
				// move docOffsetX to cursor
				const int LeftDxPix = 50;
				float samplePix = sampleIndToDocPosX(value.first);
				long newDocOffsetX = samplePix - LeftDxPix;
				setDocOffsetX(newDocOffsetX);
			}
		}
	}
}

std::pair<long, long> TranscriberViewModel::cursor() const
{
	return cursor_;
}

std::pair<long, long> TranscriberViewModel::cursorOrdered() const
{
	assert(cursorKind() == TranscriberCursorKind::Range);
	long start = std::min(cursor_.first, cursor_.second);
	long end   = std::max(cursor_.first, cursor_.second);
	return std::make_pair(start,end);
}

TranscriberCursorKind TranscriberViewModel::cursorKind() const
{
	if (cursor_.first != PticaGovorun::NullSampleInd)
	{
		if (cursor_.second != PticaGovorun::NullSampleInd)
			return TranscriberCursorKind::Range;
		else
			return TranscriberCursorKind::Single;
	}
	else 
	{
		assert(cursor_.second == PticaGovorun::NullSampleInd && "Cursor invariant: when first position is empty, the second position must be empty too");
		return TranscriberCursorKind::Empty;
	}
}

void TranscriberViewModel::insertNewMarkerAtCursorRequest()
{
	long curFrameInd = currentSampleInd();
	if (curFrameInd == PticaGovorun::NullSampleInd)
		return;

	PticaGovorun::TimePointMarker newMarker;
	newMarker.Id = PticaGovorun::NullSampleInd;
	newMarker.SampleInd = curFrameInd;
	newMarker.IsManual = true;
	newMarker.LevelOfDetail = templateMarkerLevelOfDetail_;
	newMarker.StopsPlayback = getDefaultMarkerStopsPlayback(templateMarkerLevelOfDetail_);
	insertNewMarker(newMarker, true, false);
}

void TranscriberViewModel::insertNewMarker(const PticaGovorun::TimePointMarker& marker, bool updateCursor, bool updateViewportOffset)
{
	int newMarkerInd = speechAnnot_.insertMarker(marker);

	setCurrentMarkerIndInternal(newMarkerInd, updateCursor, updateViewportOffset);

	// TODO: repaint only current marker
	emit audioSamplesChanged();
}

bool TranscriberViewModel::deleteMarker(int markerInd)
{
	bool delOp = speechAnnot_.deleteMarker(markerInd);
	if (delOp)
		setCurrentMarkerIndInternal(-1, false, false);

	return delOp;
}

void TranscriberViewModel::selectMarkerClosestToCurrentCursorRequest()
{
	long curFrameInd = currentSampleInd();

	long distToClosestMarker = -1;
	int closestMarkerInd = speechAnnot_.getClosestMarkerInd(curFrameInd, &distToClosestMarker);

	setCurrentMarkerIndInternal(closestMarkerInd, true, false);
}

void TranscriberViewModel::selectMarkerForward()
{
	selectMarkerInternal(true);
}

void TranscriberViewModel::selectMarkerBackward()
{
	selectMarkerInternal(false);
}

void TranscriberViewModel::selectMarkerInternal(bool moveForward)
{
	long curSampleInd = cursor().first;
	if (curSampleInd == PticaGovorun::NullSampleInd)
		return;

	int leftMarkerInd = -1;
	int rightMarkerInd = -1;
	auto markerFrameIndSelector = [](const PticaGovorun::TimePointMarker& m) { return m.SampleInd;  };
	bool foundSegOp = PticaGovorun::findSegmentMarkerInds(speechAnnot_.markers(), markerFrameIndSelector, curSampleInd, true, leftMarkerInd, rightMarkerInd);
	if (!foundSegOp)
		return;

	int nextMarkerInd = -1;
	if (moveForward && rightMarkerInd != -1)
	{
		assert(rightMarkerInd < speechAnnot_.markers().size());
		nextMarkerInd = rightMarkerInd;
	}
	else if (!moveForward && leftMarkerInd != -1)
	{
		// inside the segment?
		long markerSampleInd = speechAnnot_.marker(leftMarkerInd).SampleInd;
		if (markerSampleInd != curSampleInd)
			nextMarkerInd = leftMarkerInd;
		else
		{
			if (leftMarkerInd - 1 >= 0)
				nextMarkerInd = leftMarkerInd - 1;
		}
	}

	if (nextMarkerInd != -1)
		setCurrentMarkerIndInternal(nextMarkerInd, true, true);
}

void TranscriberViewModel::setCurrentMarkerIndInternal(int markerInd, bool updateCursor, bool updateViewportOffset)
{
	if (currentMarkerInd_ != markerInd)
	{
		// updates current marker
		currentMarkerInd_ = markerInd;
		emit currentMarkerIndChanged();

		if (updateCursor && markerInd != -1)
		{
			// udpdate cursor position
			long newCurSampleInd = speechAnnot_.marker(markerInd).SampleInd;
			setCursorInternal(newCurSampleInd, false, updateViewportOffset);
		}
	}
}

int TranscriberViewModel::currentMarkerInd() const
{
	return currentMarkerInd_;
}

void TranscriberViewModel::setCurrentMarkerTranscriptText(const QString& text)
{
	if (currentMarkerInd_ != -1)
	{
		PticaGovorun::TimePointMarker& marker = speechAnnot_.marker(currentMarkerInd_);
		marker.TranscripText = text;

		//// ignore language for 'phone' markers
		//if (marker.LevelOfDetail == PticaGovorun::MarkerLevelOfDetail::Phone || text.isEmpty())
		//	marker.Language = PticaGovorun::SpeechLanguage::NotSet;
		if (marker.LevelOfDetail == PticaGovorun::MarkerLevelOfDetail::Word)
		{
			// first time initialization?
			if (marker.Language == PticaGovorun::SpeechLanguage::NotSet)
				marker.Language = templateMarkerSpeechLanguage_;
		}

		emit currentMarkerIndChanged();
	}
	else
		cursorTextToAlign_ = text;
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
		speechAnnot_.marker(currentMarkerInd_).LevelOfDetail = levelOfDetail;

		// TODO: repaint only current marker
		emit audioSamplesChanged();
	}
}

void TranscriberViewModel::setCurrentMarkerStopOnPlayback(bool stopsPlayback)
{
	if (currentMarkerInd_ == -1)
		return;
	speechAnnot_.marker(currentMarkerInd_).StopsPlayback = stopsPlayback;
}

void TranscriberViewModel::setCurrentMarkerLang(PticaGovorun::SpeechLanguage lang)
{
	std::string langStr = speechLanguageToStr(lang);
	if (currentMarkerInd_ == -1)
	{
		// apply user selection as a template for new markers
		if (templateMarkerSpeechLanguage_ != lang)
		{
			templateMarkerSpeechLanguage_ = lang;
			emit nextNotification(QString("Speech language for new markers=%1").arg(langStr.c_str()));
		}
	}
	else
	{
		// apply the value to current marker
		PticaGovorun::TimePointMarker& m = speechAnnot_.marker(currentMarkerInd_);
		PticaGovorun::SpeechLanguage oldLang = m.Language;
		if (oldLang != lang)
		{
			m.Language = lang;
			emit nextNotification(QString("Marker[id=%1].Language=%2").arg(m.Id).arg(langStr.c_str()));
		}
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
		if (!initRecognizerConfiguration(recogName, rs))
		{
			emit nextNotification("Can't find speech recognizer configuration");
			return;
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

void TranscriberViewModel::recognizeCurrentSegmentJuliusRequest()
{
	using namespace PticaGovorun;
	ensureRecognizerIsCreated();

	if (recognizer_ == nullptr)
		return;

	long curSegBeg;
	long curSegEnd;
	std::tie(curSegBeg, curSegEnd) = getSampleRangeToPlay(currentSampleInd(), SegmentStartFrameToPlayChoice::SegmentBegin);
	auto len = curSegEnd - curSegBeg;

	// limit range to some short period, otherwise Julius may crash.
	// Julius has limitations MAXSPEECHLEN=320k for the number of samples and MAXSEQNUM=150 for the number of words
	const int maxDur = 10; // sec, max duration to process
	long maxFrames = audioFrameRate_ * maxDur;
	if (len > maxFrames)
	{
		auto oldLen = len;
		len = maxFrames;
		curSegEnd = curSegBeg + len;
		emit nextNotification(QString("WARN: Segment is too large. Limiting it from %1 to %2 samles").arg(oldLen).arg(len));
	}

	// pad the audio with silince
	PticaGovorun::padSilence(&audioSamples_[curSegBeg], len, silencePadAudioSamplesCount_, audioSegmentBuffer_);

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

#if PG_DEBUG // TODO: take sampleInd from curSegBeg
	long markerOffset = curSegBeg - silencePadAudioSamplesCount_;
	for (const PticaGovorun::AlignedPhoneme& phone : recogResult.AlignedPhonemeSeq)
	{
		qDebug() << "[" << phone.BegFrame << "," << phone.EndFrameIncl << "] [" 
			<< markerOffset + phone.BegSample << "," << markerOffset + phone.EndSample << "]";
	}
#endif

	//
	DiagramSegment diagSeg;
	diagSeg.SampleIndBegin = curSegBeg;
	diagSeg.SampleIndEnd = curSegEnd;
	diagSeg.RecogAlignedPhonemeSeqPadded = true;

	std::wstringstream recogText;
	for (const std::wstring& word : recogResult.TextPass1)
	{
		recogText << word << " ";
	}
	diagSeg.RecogSegmentText = QString::fromStdWString(recogText.str());

	recogText.str(L"");
	for (const std::wstring& word : recogResult.WordSeq)
	{
		recogText << word << " ";
	}
	diagSeg.RecogSegmentWords = QString::fromStdWString(recogText.str());
	diagSeg.RecogAlignedPhonemeSeq = recogResult.AlignedPhonemeSeq;

	diagramSegments_.push_back(diagSeg);

	// redraw current segment
	emit audioSamplesChanged();

	// push the text to log so a user can copy it
	emit nextNotification(diagSeg.RecogSegmentWords);
}

#if HAS_POCKETSPHINX
void TranscriberViewModel::recognizeCurrentSegmentSphinxRequest()
{
	using namespace PticaGovorun;

	long curSegBeg;
	long curSegEnd;
	std::tie(curSegBeg, curSegEnd) = getSampleRangeToPlay(currentSampleInd(), SegmentStartFrameToPlayChoice::SegmentBegin);
	auto len = curSegEnd - curSegBeg;

	// pad the audio with silince
	PticaGovorun::padSilence(&audioSamples_[curSegBeg], len, silencePadAudioSamplesCount_, audioSegmentBuffer_);

	//PticaGovorun::JuiliusRecognitionResult recogResult;
	//auto recogOp = recognizer_->recognize(FrameToSamplePicker, audioSegmentBuffer_.data(), audioSegmentBuffer_.size(), recogResult);

	const char* hmmPath =       R"path(C:\devb\PticaGovorunProj\data\TrainSphinx\persian\model_parameters\persian.cd_cont_200)path";
	const char* langModelPath = R"path(C:\devb\PticaGovorunProj\data\TrainSphinx\persian\etc\persian.lm.DMP)path";
	const char* dictPath =      R"path(C:\devb\PticaGovorunProj\data\TrainSphinx\persian\etc\persian.dic)path";
	cmd_ln_t *config = cmd_ln_init(nullptr, ps_args(), true,
		"-hmm", hmmPath,
		"-lm", langModelPath,
		"-dict", dictPath,
		nullptr);
	if (config == nullptr)
		return;

	ps_decoder_t *ps = ps_init(config);
	if (ps == nullptr)
		return;

	fe_t* pFeatInfo = ps->acmod->fe; // feature extraction info
	float sphinxSampleRate = pFeatInfo->sampling_rate;
	int frameSize = pFeatInfo->frame_size;
	int frameShift = pFeatInfo->frame_shift;

	// convert to sample frequency required by Sphinx
	std::vector<float> samplesFloat(std::begin(audioSegmentBuffer_), std::end(audioSegmentBuffer_));
	std::vector<float> resampledWaveFloat(samplesFloat.size(), 0);

	SRC_DATA convertData;
	convertData.data_in = samplesFloat.data();
	convertData.input_frames = samplesFloat.size();
	convertData.data_out = resampledWaveFloat.data();
	convertData.output_frames = resampledWaveFloat.size();
	convertData.src_ratio = sphinxSampleRate / (float)audioFrameRate_;

	int converterType = SRC_SINC_BEST_QUALITY;
	int channels = 1;
	int error = src_simple(&convertData, converterType, channels);
	if (error != 0)
	{
		const char* msg = src_strerror(error);
		qDebug() << msg;
		return;
	}

	resampledWaveFloat.resize(convertData.output_frames_gen);
	std::vector<short> resampledWave(std::begin(resampledWaveFloat), std::end(resampledWaveFloat));

	//

	int ret = ps_start_utt(ps, "goforward");
	if (ret < 0)
		return;

	const bool fullUtterance = true;
	ret = ps_process_raw(ps, resampledWave.data(), resampledWave.size(), false, fullUtterance);
	if (ret < 0)
		return;

	ret = ps_end_utt(ps);
	if (ret < 0)
		return;

	//
	int32 score;
	const char* uttid;
	const char* hyp = ps_get_hyp(ps, &score, &uttid);
	if (hyp == nullptr)
	{
		QString msg("No hypothesis is available");
		emit nextNotification(msg);
		return;
	}

	QTextCodec* textCodec = QTextCodec::codecForName("utf8");
	QString hypQStr = textCodec->toUnicode(hyp);
	std::wstring hypWStr = hypQStr.toStdWString();


	// word segmentation
	std::vector<PticaGovorun::AlignedWord> wordBoundaries;
	for (ps_seg_t *seg = ps_seg_iter(ps, &score); seg; seg = ps_seg_next(seg))
	{
		const char* word = ps_seg_word(seg);
		QString wordQStr = textCodec->toUnicode(word);

		int frameStart, frameEnd;
		ps_seg_frames(seg, &frameStart, &frameEnd);

		int32 lscr, ascr, lback;
		int32 post = ps_seg_prob(seg, &ascr, &lscr, &lback); // posterior probability?

		float64 prob = logmath_exp(ps_get_logmath(ps), post);

		long sampleBeg = -1;
		long sampleEnd = -1;
		frameRangeToSampleRange(frameStart, frameEnd, FrameToSamplePicker, frameSize, frameShift, sampleBeg, sampleEnd);

		// convert sampleInd back to original sample rate
		sampleBeg /= convertData.src_ratio;
		sampleEnd /= convertData.src_ratio;

		AlignedWord wordBnds;
		wordBnds.Name = wordQStr;
		wordBnds.BegSample = sampleBeg;
		wordBnds.EndSample = sampleEnd;
		wordBnds.Prob = (float)prob;
		wordBoundaries.push_back(wordBnds);
	}

	ps_free(ps);

	//
	DiagramSegment diagSeg;
	diagSeg.SampleIndBegin = curSegBeg;
	diagSeg.SampleIndEnd = curSegEnd;
	diagSeg.RecogAlignedPhonemeSeqPadded = true;
	diagSeg.RecogSegmentText = hypQStr;
	diagSeg.WordBoundaries = std::move(wordBoundaries);
	diagramSegments_.push_back(diagSeg);

	// push the text to log so a user can copy it
	emit nextNotification(hypQStr);

	// redraw current segment
	emit audioSamplesChanged();
}

#endif

void TranscriberViewModel::dumpSilence(long i1, long i2)
{
	using namespace PticaGovorun;
	long len = i2 - i1;
	wv::slice<short> frames = wv::make_view(&audioSamples_[i1], len);
	float mag = signalMagnitude(frames);
	nextNotification(QString("Magnitude=%1").arg(mag));
	std::cout << "Magnitude=" << mag << std::endl;

	//
	int windowSize = FrameSize;
	int windowShift = FrameShift;
	int wndsCount = slidingWindowsCount(len, windowSize, windowShift);

	float avgMag2 = 0;
	std::vector <TwoFrameInds> wnds(wndsCount);
	slidingWindows(i1, len, windowSize, windowShift, wnds);
	for (int i = 0; i < wndsCount; ++i)
	{
		TwoFrameInds bounds = wnds[i];
		float m = signalMagnitude(wv::make_view(&audioSamples_[bounds.Start], bounds.Count));
		std::cout << m << " ";
		avgMag2 += m;
	}
	std::cout << std::endl;

	avgMag2 /= wndsCount;


	std::cout << "Magnitude2=" << avgMag2 << std::endl; 
}

void TranscriberViewModel::dumpSilence()
{
	using namespace PticaGovorun;
	if (cursorKind() != TranscriberCursorKind::Range)
		return;
	long start = std::min(cursor_.first, cursor_.second);
	long end = std::max(cursor_.first, cursor_.second);
	dumpSilence(start, end);
}

void TranscriberViewModel::analyzeUnlabeledSpeech()
{
	using namespace PticaGovorun;
	// get segments, ignoring auto markers
	// get segment complements
	// analyze windows of samples for silence
	// take 10 in a row like silence
	// insert auto markers
	if (cursorKind() != TranscriberCursorKind::Range)
		return;
	long start = std::min(cursor_.first, cursor_.second);
	long end = std::max(cursor_.first, cursor_.second);
	long len = end - start;
	wv::slice<short> frames = wv::make_view(&audioSamples_[start], len);

	// 104 ms = between syllable
	if (silenceSlidingWindowDur_ == -1)
		silenceSlidingWindowDur_ = 120;

	int windowSize = static_cast<int>(silenceSlidingWindowDur_ / 1000 * audioFrameRate_);
	
	if (silenceSlidingWindowShift_ == -1)
		//silenceSlidingWindowShift_ = windowSize / 3;
		silenceSlidingWindowShift_ = FrameShift;
	int windowShift = silenceSlidingWindowShift_;

	int wndsCount = slidingWindowsCount(len, windowSize, windowShift);
	std::vector <TwoFrameInds> wnds(wndsCount);
	slidingWindows(start, len, windowSize, windowShift, wnds);

	//
	if (silenceMagnitudeThresh_ == -1)
		silenceMagnitudeThresh_ = 300;
	if (silenceSmallWndMagnitudeThresh_ == -1)
		silenceSmallWndMagnitudeThresh_ = 200;

	std::vector<uchar> wndIsSilence(wndsCount);
	for (int i = 0; i < wndsCount; ++i)
	{
		TwoFrameInds bounds = wnds[i];
		float mag = signalMagnitude(wv::make_view(&audioSamples_[bounds.Start], bounds.Count));
		
		int smallWndSize = FrameSize;
		int smallWndShift = FrameShift;
		float smallMag = std::numeric_limits<float>::max();
		for (int smallStartInd = bounds.Start; smallStartInd < bounds.Start + bounds.Count; smallStartInd += smallWndShift)
		{
			float m = signalMagnitude(wv::make_view(&audioSamples_[smallStartInd], smallWndSize));
			smallMag = std::min(smallMag, m);
		}

		std::cout << "wnd[" << i << "] large=" <<mag << " small=" <<smallMag <<std::endl;

		bool isSil = mag < silenceMagnitudeThresh_ && smallMag < silenceSmallWndMagnitudeThresh_;
		wndIsSilence[i] = isSil;
	}

	PticaGovorun::TimePointMarker newMarker;
	newMarker.Id = PticaGovorun::NullSampleInd; // id is generated by ownding collection
	newMarker.IsManual = false;
	newMarker.LevelOfDetail = MarkerLevelOfDetail::Word;
	newMarker.StopsPlayback = getDefaultMarkerStopsPlayback(templateMarkerLevelOfDetail_);

	for (int i = 0; true; )
	{
		// find selecnce
		for (; i < wndsCount && !wndIsSilence[i]; ++i) {}
		if (i >= wndsCount)
			break;

		assert(wndIsSilence[i]);

		long left = wnds[i].Start;

		// find end of silence
		for (i = i + 1; i < wndsCount && wndIsSilence[i]; ++i) {}

		assert(wndIsSilence[i-1]);

		TwoFrameInds bndsLast = wnds[i - 1];
		long rightExcl = bndsLast.Start + bndsLast.Count;

		// reduce silence range, because usually it takes valueable data
		left += FrameShift;
		rightExcl -= FrameShift;

		newMarker.SampleInd = left;
		insertNewMarker(newMarker, false, false);

		newMarker.SampleInd = rightExcl;
		insertNewMarker(newMarker, false, false);

		std::cout << "[" << left << " " << rightExcl << "] ";
	}
	
	std::cout << std::endl;
}

void TranscriberViewModel::ensureWordToPhoneListVocabularyLoaded()
{
	using namespace PticaGovorun;

	QTextCodec* pTextCodec = QTextCodec::codecForName(PGEncodingStr);
	PG_Assert(pTextCodec != nullptr && "Can't load text encoding");

	// TODO: vocabulary is recognizer dependent
	if (wordToPhoneListDict_.empty())
	{
		QString dicPath = QString::fromStdString(recognizer_->settings().DictionaryFilePath);

		clock_t startTime = std::clock();
		std::tuple<bool, const char*> loadOp = PticaGovorun::loadPronunciationVocabulary(dicPath.toStdWString(), wordToPhoneListDict_, *pTextCodec);
		double elapsedTime1 = static_cast<double>(clock() - startTime) / CLOCKS_PER_SEC;
		if (!std::get<0>(loadOp))
		{
			emit nextNotification(QString(std::get<1>(loadOp)));
			return;
		}
		emit nextNotification(QString("Loaded pronunciation dictionary in %1 secs").arg(elapsedTime1, 2));
	}

	if (wordToPhoneListAuxiliaryDict_.empty())
		loadAuxiliaryPhoneticDictionaryRequest();
}

void TranscriberViewModel::loadAuxiliaryPhoneticDictionaryRequest()
{
	using namespace PticaGovorun;
	QTextCodec* pTextCodec = QTextCodec::codecForName(PGEncodingStr);
	PG_Assert(pTextCodec != nullptr && "Can't load text encoding");

	const char* dicPatCStr = qgetenv("PG_AUX_DICT_PATH").constData();
	QString dicPath = QString(dicPatCStr);
	if (dicPath.isEmpty())
	{
		emit nextNotification("Specify path PG_AUX_DICT_PATH to auxiliary dictionary");
		return;
	}

	clock_t startTime = std::clock();
	wordToPhoneListAuxiliaryDict_.clear();
	std::tuple<bool,const char*> loadOp = PticaGovorun::loadPronunciationVocabulary(dicPath.toStdWString(), wordToPhoneListAuxiliaryDict_, *pTextCodec);
	double elapsedTime1 = static_cast<double>(clock() - startTime) / CLOCKS_PER_SEC;
	if (!std::get<0>(loadOp))
	{
		emit nextNotification(QString(std::get<1>(loadOp)));
		return;
	}
	emit nextNotification(QString("Loaded pronunciation AX dictionary in %1 secs").arg(elapsedTime1, 2));
}

void TranscriberViewModel::alignPhonesForCurrentSegmentRequest()
{
	using namespace PticaGovorun;
	ensureRecognizerIsCreated();

	if (recognizer_ == nullptr)
		return;

	ensureWordToPhoneListVocabularyLoaded();

	int outLeftMarkerInd = -1;
	long curSegBeg;
	long curSegEnd;
	std::tie(curSegBeg, curSegEnd) = getSampleRangeToPlay(currentSampleInd(), SegmentStartFrameToPlayChoice::SegmentBegin, &outLeftMarkerInd);
	auto len = curSegEnd - curSegBeg;

	//
	QString alignText;
	if (!cursorTextToAlign_.isEmpty())
		alignText = cursorTextToAlign_;
	else if (outLeftMarkerInd != -1)
	{
		// there is a marker assiciated with this alignment operation
		alignText = speechAnnot_.marker(outLeftMarkerInd).TranscripText;
	}
	
	if (alignText.isEmpty()) // nothing to align
		return;
	
	// make all words lowercase, because all words in phonetic dictionary are in lowercase
	alignText = alignText.toLower();

	auto wordToPhoneListFun = [this](const std::wstring& word, std::vector<std::string>& wordPhones)->bool
	{
		auto it = wordToPhoneListDict_.find(word);
		if (it != std::end(wordToPhoneListDict_))
		{
			const std::vector<std::string>& dictWordPhones = it->second;
			std::copy(std::begin(dictWordPhones), std::end(dictWordPhones), std::back_inserter(wordPhones));
			return true;
		}
		it = wordToPhoneListAuxiliaryDict_.find(word);
		if (it != std::end(wordToPhoneListAuxiliaryDict_))
		{
			const std::vector<std::string>& dictWordPhones = it->second;
			std::copy(std::begin(dictWordPhones), std::end(dictWordPhones), std::back_inserter(wordPhones));
			return true;
		}
		return false;
	};

	bool insertShortPause = false;
	std::vector<std::string> speechPhones;
	std::tuple<bool, std::wstring> convOp = convertTextToPhoneList(alignText.toStdWString(), wordToPhoneListFun, insertShortPause, speechPhones);
	if (!std::get<0>(convOp))
	{
		std::wstring errMsg = std::get<1>(convOp);
		emit nextNotification(QString::fromStdWString(errMsg));
		return;
	}

	// pad the audio with silince
	padSilence(&audioSamples_[curSegBeg], len, silencePadAudioSamplesCount_, audioSegmentBuffer_);

	PticaGovorun::AlignmentParams alignmentParams;
	alignmentParams.FrameSize = recognizer_->settings().FrameSize;
	alignmentParams.FrameShift = recognizer_->settings().FrameShift;
	alignmentParams.TakeSample = FrameToSamplePicker;

	int phoneStateDistrributionTailSize = 5;
	PticaGovorun::PhoneAlignmentInfo alignmentResult;
	recognizer_->alignPhones(audioSegmentBuffer_.data(), audioSegmentBuffer_.size(), speechPhones, alignmentParams, phoneStateDistrributionTailSize, alignmentResult);

	//
	DiagramSegment diagSeg;
	diagSeg.SampleIndBegin = curSegBeg;
	diagSeg.SampleIndEnd = curSegEnd;
	diagSeg.RecogAlignedPhonemeSeqPadded = true;
	diagSeg.TextToAlign = alignText;
	diagSeg.TranscripTextPhones = alignmentResult;
	diagramSegments_.push_back(diagSeg);

	emit nextNotification(QString("Alignment score=%1").arg(alignmentResult.AlignmentScore));

	// redraw current segment
	emit audioSamplesChanged();
}

size_t TranscriberViewModel::silencePadAudioFramesCount() const
{
	return silencePadAudioSamplesCount_;
}

QString TranscriberViewModel::recognizerName() const
{
	return curRecognizerName_;
}

void TranscriberViewModel::setRecognizerName(const QString& filePath)
{
	curRecognizerName_ = filePath;
}

void TranscriberViewModel::computeMfccRequest()
{
	using namespace PticaGovorun;
	ensureRecognizerIsCreated();
	if (recognizer_ == nullptr)
		return;

	// collect features

	phoneNameToFeaturesVector_.clear();

	QFileInfo wavFolder(R"path(E:\devb\workshop\PticaGovorunProj\data\prokopeo_specific\)path");
	auto featsOp = PticaGovorun::collectMfccFeatures(wavFolder, FrameSize, FrameShift, MfccVecLen, phoneNameToFeaturesVector_);
	if (!std::get<0>(featsOp))
	{
		emit nextNotification(QString::fromStdString(std::get<1>(featsOp)));
		return;
	}

	// train GMMs

	phoneNameToEMObj_.clear();
	int nclusters = NumClusters;
	auto trainOp = PticaGovorun::trainMonophoneClassifier(phoneNameToFeaturesVector_, MfccVecLen, nclusters, phoneNameToEMObj_);
	if (!std::get<0>(trainOp))
	{
		emit nextNotification(QString::fromStdString(std::get<1>(trainOp)));
		return;
	}

	emit nextNotification("Done training phone classifiers");
}

void TranscriberViewModel::testMfccRequest()
{
	using namespace PticaGovorun;
	ensureRecognizerIsCreated();
	if (recognizer_ == nullptr)
		return;

	int outLeftMarkerInd;
	long curSegBeg;
	long curSegEnd;
	std::tie(curSegBeg, curSegEnd) = getSampleRangeToPlay(currentSampleInd(), SegmentStartFrameToPlayChoice::SegmentBegin, &outLeftMarkerInd);
	if (outLeftMarkerInd == -1) // not marker to associate recognized text with
		return;

	auto len = curSegEnd - curSegBeg;

	// convert
	auto& leftMarker = speechAnnot_.marker(outLeftMarkerInd);
	if (leftMarker.TranscripText.isEmpty())
		return;

	size_t mfccVecLen = MfccVecLen;
	std::vector<float> mfccFeatures;
	int framesCount;
	auto mfccFeatsOp = PticaGovorun::computeMfccFeaturesPub(&audioSamples_[curSegBeg], len, FrameSize, FrameShift, mfccVecLen, mfccFeatures, framesCount);
	if (!std::get<0>(mfccFeatsOp))
	{
		emit nextNotification(QString::fromStdString(std::get<1>(mfccFeatsOp)));
		return;
	}

	// convert data to to double

	std::vector<double> mfccFeaturesDouble(mfccFeatures.size());
	std::transform(std::begin(mfccFeatures), std::end(mfccFeatures), std::begin(mfccFeaturesDouble), [](float x) { return (double)x; });
	
	// test each frame, each phone
	
	cv::Mat cacheL;
	cacheL.create(1, NumClusters, CV_64FC1);

	for (int i = 0; i < framesCount; i++)
	{
		qDebug() << "frame=" << i;
		for (const auto& phoneEMPair : phoneNameToEMObj_)
		{
			const std::string& phoneName = phoneEMPair.first;
			PticaGovorun::EMQuick& em = *phoneEMPair.second.get();

			cv::Mat oneMfccVector(1, mfccVecLen, CV_64FC1, mfccFeaturesDouble.data() + (i*mfccVecLen));
			cv::Vec2d resP2 = PticaGovorun::EMQuick::predict2(oneMfccVector, em.getMeans(), em.getInvCovsEigenValuesPar(), em.getLogWeightDivDetPar(), cacheL);

			cv::Vec2d resEm = em.predict(oneMfccVector); // TODO: OpenCV impl doesn't work?

			//double ampLogProb = computeGaussMixtureModel(NumClusters, mfccVecLen, (double*)em.getMeans().data, (double*)em.getInvCovsEigenValuesPar()[0].data, (double*)em.getLogWeightDivDetPar().data, (double*)oneMfccVector.data, (double*)cacheL.data);
			double ampLogProb = -1;

			//qDebug() <<QString::fromStdString(phoneName) << "LogP=" << res[0] << " Label=" << res[1];
			//qDebug() <<QString::fromStdString(phoneName) << "LogP=" << logProb;
			qDebug() << QString::fromStdString(phoneName) << "LOGP: amp=" << ampLogProb <<" p2=" <<resP2[0] <<" EM=" <<resEm[0];
		}
	}

	std::stringstream msg;
	for (int i = 0; i < framesCount; i++)
	{
		std::string bestPhoneName = "?";
		double bestPhoneLogProb = std::numeric_limits<double>::lowest();
		for (const auto& phoneEMPair : phoneNameToEMObj_)
		{
			const std::string& phoneName = phoneEMPair.first;
			PticaGovorun::EMQuick& em = *phoneEMPair.second.get();

			cv::Mat oneMfccVector(1, mfccVecLen, CV_64FC1, mfccFeaturesDouble.data() + (i*mfccVecLen));
			cv::Vec2d resP2 = PticaGovorun::EMQuick::predict2(oneMfccVector, em.getMeans(), em.getInvCovsEigenValuesPar(), em.getLogWeightDivDetPar(), cacheL);
			double logProb = resP2[0];
			//double ampLogProb = computeGaussMixtureModel(NumClusters, mfccVecLen, (double*)em.getMeans().data, (double*)em.getInvCovsEigenValuesPar()[0].data, //(double*)em.getLogWeightDivDetPar().data, (double*)oneMfccVector.data, (double*)cacheL.data);
			if (logProb > bestPhoneLogProb)
			{
				bestPhoneLogProb = logProb;
				bestPhoneName = phoneName;
			}
		}
		msg << bestPhoneName << " ";
	}
	qDebug() << "Recog: " << msg.str().c_str();
}

void TranscriberViewModel::classifyMfccIntoPhones()
{
	using namespace PticaGovorun;

	long curSegBeg;
	long curSegEnd;
	std::tie(curSegBeg, curSegEnd) = getSampleRangeToPlay(currentSampleInd(), SegmentStartFrameToPlayChoice::SegmentBegin);
	auto len = curSegEnd - curSegBeg;

	// speech -> MFCC

	int frameSize = FrameSize;
	int frameShift = FrameShift;
	float sampleRate = audioFrameRate_;

	// init filter bank
	int binCount = 24; // number of bins in the triangular filter bank
	int fftNum = getMinDftPointsCount(frameSize);
	TriangularFilterBank filterBank;
	buildTriangularFilterBank(sampleRate, binCount, fftNum, filterBank);

	// speech -> MFCC

	const int mfccCount = 12;
	// +1 for usage of cepstral0 coef
	// *3 for velocity and acceleration coefs
	int mfccVecLen = 3 * (mfccCount + 1);

	int framesCount = slidingWindowsCount(len, frameSize, frameShift);
	std::vector<float> mfccFeatures(mfccVecLen*framesCount, 0);
	wv::slice<short> samplesPart = wv::make_view(audioSamples_.data() + curSegBeg, len);
	computeMfccVelocityAccel(samplesPart, frameSize, frameShift, framesCount, mfccCount, mfccVecLen, filterBank, mfccFeatures);

#if HAS_MATLAB
	int phonesCount = 6;
	// MFCC -> phoneIds
	std::vector<ClassifiedSpeechSegment> classifiedWindows;
	for (int i = 0; i < framesCount; i++)
	{
		mwArray featsMat(mfccVecLen, 1, mxSINGLE_CLASS);
		featsMat.SetData(mfccFeatures.data() + (i*mfccVecLen), mfccVecLen);

		mwArray probsPerPatternMat;
		classifySpeechMfcc(1, probsPerPatternMat, featsMat);
		std::vector<float> probsPerPhone(phonesCount);
		probsPerPatternMat.GetData(probsPerPhone.data(), probsPerPhone.size());

		//
		ClassifiedSpeechSegment alignedPhone;
		alignedPhone.BegFrame = i;
		alignedPhone.EndFrameIncl = i;

		long begSample = -1;
		long endSample = -1;
		frameRangeToSampleRange(i, i, FrameToSamplePicker, FrameSize, FrameShift, begSample, endSample);

		alignedPhone.BegSample = begSample;
		alignedPhone.EndSample = endSample;

		alignedPhone.PhoneLogProbs = std::move(probsPerPhone);

		classifiedWindows.push_back(alignedPhone);
	}

	//
	std::stringstream msg;
	std::vector<AlignedPhoneme> monoPhonesPerWindow;
	for (const ClassifiedSpeechSegment& probsPerPhone : classifiedWindows)
	{
		auto maxIt = std::max_element(std::begin(probsPerPhone.PhoneLogProbs), std::end(probsPerPhone.PhoneLogProbs));
		int phoneInd = maxIt - std::begin(probsPerPhone.PhoneLogProbs);

		std::string bestPhoneName = "?";
		phoneIdToByPhoneName(phoneInd, bestPhoneName);

		AlignedPhoneme ap;
		ap.Name = bestPhoneName;
		ap.BegFrame = probsPerPhone.BegFrame;
		ap.EndFrameIncl = probsPerPhone.EndFrameIncl;
		ap.BegSample = probsPerPhone.BegSample;
		ap.EndSample = probsPerPhone.EndSample;
		monoPhonesPerWindow.push_back(ap);
	
		msg << bestPhoneName << " ";
	}
	qDebug() << "Recog: " << msg.str().c_str();

	std::vector<AlignedPhoneme> monoPhonesMerged;
	mergeSamePhoneStates(monoPhonesPerWindow, monoPhonesMerged);

	DiagramSegment diagSeg;
	diagSeg.SampleIndBegin = curSegBeg;
	diagSeg.SampleIndEnd = curSegEnd;
	diagSeg.RecogAlignedPhonemeSeqPadded = false;
	diagSeg.RecogAlignedPhonemeSeq = monoPhonesMerged;
	diagSeg.ClassifiedFrames = classifiedWindows;
	diagramSegments_.push_back(diagSeg);

	// redraw current segment
	emit audioSamplesChanged();
#endif
}

void TranscriberViewModel::playComposingRecipeRequest(QString recipe)
{
	composedAudio_.clear();

	// Composition recipe format:
	// characters after # signs are comments
	// 0 section contains the name of the file
	// 1 section has format 3-15 meaning play audio from markerId=3 to markerId=15
	// R3 means repeat this section 3 times
	// empty lines are ignored

	QStringList lines = recipe.split('\n', QString::SkipEmptyParts);
	for (QString line : lines)
	{
		// check line comment
		int hashInd = line.indexOf('#');
		if (hashInd != -1)
		{
			// remove comment: everything in the line starting from comment sign
			line = line.remove(hashInd, line.size() - hashInd);
		}

		if (line.isEmpty())
			continue;

		QStringList lineParts = line.split(';', QString::SkipEmptyParts);
		if (lineParts.empty())
			continue;

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

		int fromMarkerInd = speechAnnot_.markerIndByMarkerId(fromMarkerId);
		if (fromMarkerInd == -1)
			return;
		int toMarkerInd = speechAnnot_.markerIndByMarkerId(toMarkerId);
		if (toMarkerInd == -1)
			return;

		long fromFrameInd = speechAnnot_.marker(fromMarkerInd).SampleInd;
		long toFrameInd = speechAnnot_.marker(toMarkerInd).SampleInd;

		// parse other possible options

		int repeatTimes = 1; // R3 means repeating three times

		for (int i=2; i < lineParts.size(); i++)
		{
			const QString& item = lineParts[i];

			if (item.startsWith('R', Qt::CaseInsensitive))
			{
				QStringRef timesStr = item.rightRef(item.size() - 1);
				bool parseIntOp;
				int times = timesStr.toInt(&parseIntOp);
				if (parseIntOp)
					repeatTimes = times;
			}
		}

		//
		for (int i = 0; i < repeatTimes; ++i)
			std::copy(std::begin(audioSamples_) + fromFrameInd, std::begin(audioSamples_) + toFrameInd, std::back_inserter(composedAudio_));
	}

	// play composed audio
	soundPlayerPlay(composedAudio_.data(), 0, composedAudio_.size(), false);
}

void TranscriberViewModel::setAudioMarkupNavigator(std::shared_ptr<AudioMarkupNavigator> audioMarkupNavigator)
{
	audioMarkupNavigator_ = audioMarkupNavigator;
}

void TranscriberViewModel::navigateToMarkerRequest()
{
	if (audioMarkupNavigator_ == nullptr)
		return;
	
	// ask user for marker id
	int markerId = -1;
	if (!audioMarkupNavigator_->requestMarkerId(markerId))
		return;

	int markerInd = speechAnnot_.markerIndByMarkerId(markerId);
	if (markerInd == -1) // user input non existent marker id
		return;

	setCurrentMarkerIndInternal(markerInd, true, true);
}
