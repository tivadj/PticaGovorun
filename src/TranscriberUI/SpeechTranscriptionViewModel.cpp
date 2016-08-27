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
#include <QStringList>

#include <samplerate.h>

#if PG_HAS_SPHINX
#include <pocketsphinx.h>
#include <pocketsphinx_internal.h> // ps_decoder_s
#include <fe_internal.h> // fe_s
#endif


#if HAS_MATLAB
#include "matrix.h" // mxArray, mxCreateLogicalArray
#include "PticaGovorunInteropMatlab.h"
#endif

#include "WavUtils.h"
#include "SpeechTranscriptionViewModel.h"
#include "assertImpl.h"
#include "ClnUtils.h"
//#include "algos_amp.cpp"
#include "InteropPython.h"
#include "SpeechProcessing.h"
#include "PhoneticService.h"
#include "PresentationHelpers.h"
#include "SharedServiceProvider.h"
#include "AppHelpers.h"
#include "SphinxModel.h"

namespace PticaGovorun
{
static const decltype(paComplete) SoundPlayerCompleteOrAbortTechnique = paComplete; // paComplete or paAbort

SpeechTranscriptionViewModel::SpeechTranscriptionViewModel()
{
}

	void SpeechTranscriptionViewModel::init(std::shared_ptr<SharedServiceProvider> serviceProvider)
	{
		notificationService_ = serviceProvider->notificationService();
#ifdef PG_HAS_JULIUS
		recognizerNameHintProvider_ = serviceProvider->recognizerNameHintProvider();
		juliusRecognizerProvider_ = serviceProvider->juliusRecognizerProvider();
#endif
	}

	void SpeechTranscriptionViewModel::loadAnnotAndAudioFileRequest()
{
	PG_DbgAssert2(QFileInfo::exists(annotFilePathAbs_), "Annotation file must be specified");

	speechAnnot_.clear();
	bool loadOp;
	const char* errMsg;
	std::tie(loadOp, errMsg) = loadAudioMarkupFromXml(annotFilePathAbs_.toStdWString(), speechAnnot_);
	if (!loadOp)
	{
		nextNotification(errMsg);
		return;
	}

	QString audioFileRelPathQ = QString::fromStdWString(speechAnnot_.audioFileRelPath());
	QDir annotFileDir = QFileInfo(annotFilePathAbs_).absoluteDir();
	QString audioFilePath =  QFileInfo(annotFileDir, audioFileRelPathQ).canonicalFilePath();

	//
	audioSamples_.clear();
	diagramSegments_.clear();

	auto readOp = PticaGovorun::readAllSamplesFormatAware(audioFilePath.toStdString().c_str(), audioSamples_, &audioFrameRate_);
	if (!std::get<0>(readOp))
	{
		auto msg = std::get<1>(readOp);
		nextNotification(QString::fromStdString(msg));
		return;
    }
	if (audioFrameRate_ != SampleRate)
		nextNotification("WARN: FrameRate != 22050. Perhaps other parameters (FrameSize, FrameShift) should be changed");

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

	QString msg = QString("Loaded '%1' FrameRate=%2 FramesCount=%3").arg(audioFilePath).arg(audioFrameRate_).arg(audioSamples_.size());
	nextNotification(msg);

	//
	setCursorInternal(0, true ,false);

	emit audioSamplesLoaded();
}

void SpeechTranscriptionViewModel::soundPlayerPlay(const short* audioSouce, long startPlayingFrameInd, long finishPlayingFrameInd, bool restoreCurFrameInd)
{
	using namespace PticaGovorun;
	PG_DbgAssert2(startPlayingFrameInd <= finishPlayingFrameInd, "Must play start <= end");

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

	auto logPaError = [](PaError err)
	{
		auto msg = Pa_GetErrorText(err);
		qDebug() << msg;
	};

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

		SpeechTranscriptionViewModel& transcriberViewModel = *data.transcriberViewModel;

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

		SpeechTranscriptionViewModel& transcriberViewModel = *data.transcriberViewModel;

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
	{
		logPaError(err);
		return;
	}

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
	{
		logPaError(err);
		return;
	}

	// play

	soundPlayerData_.allowPlaying = true;
	isPlaying_ = true;

	err = Pa_StartStream(stream);
	if (err != paNoError)
	{
		logPaError(err);
		return;
	}
}

template <typename MarkerPred>
void SpeechTranscriptionViewModel::transformMarkersIf(const std::vector<PticaGovorun::TimePointMarker>& markers, std::vector<MarkerRefToOrigin>& markersOfInterest, MarkerPred canSelectMarker)
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

std::tuple<long, long> SpeechTranscriptionViewModel::getSampleRangeToPlay(long curSampleInd, SegmentStartFrameToPlayChoice startFrameChoice, int* outLeftMarkerInd)
{
	PG_Assert2(!audioSamples_.empty(), "Audio samples must be loaded");

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
	PG_Assert2(foundSegOp, "The segment to play must be found");

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
	
	PG_Assert2(startPlayFrameInd != -1, "Must be valid frameInd");
	PG_Assert2(endPlayFrameInd != -1, "Must be valid frameInd");
	PG_Assert2(startPlayFrameInd < endPlayFrameInd, "Must be valid range of frames");

	//return std::make_tuple<long,long>(startPlayFrameInd, endPlayFrameInd); // TODO: error C2664: 'std::tuple<long,long> std::make_tuple<long,long>(long &&,long &&)' : cannot convert argument 1 from 'long' to 'long &&'
	return std::tuple<long,long>(startPlayFrameInd, endPlayFrameInd);
}

void SpeechTranscriptionViewModel::soundPlayerPlayCurrentSegment(SegmentStartFrameToPlayChoice startFrameChoice)
{
	if (audioSamples_.empty())
		return;

	auto curFrameInd = currentSampleInd();
	if (curFrameInd < 0)
		curFrameInd = 0;

	// determine the frameInd to which to play audio
	long curSegBeg;
	long curSegEnd;
	int leftMarkerind = -1;
	std::tie(curSegBeg, curSegEnd) = getSampleRangeToPlay(curFrameInd, startFrameChoice, &leftMarkerind);

	soundPlayerPlay(audioSamples_.data(), curSegBeg, curSegEnd, true);
}

void SpeechTranscriptionViewModel::soundPlayerPause()
{
	qDebug() << "Pause. Stopping stream";
	soundPlayerData_.allowPlaying = false;

	PaStream *stream = soundPlayerData_.stream;
	PaError err = Pa_StopStream(stream);
	if (err != paNoError)
		return;

	// note, stream is closed in the Pa_SetStreamFinishedCallback
}

void SpeechTranscriptionViewModel::soundPlayerPlay()
{
	if (audioSamples_.empty())
		return;

	auto curFrameInd = currentSampleInd();
	if (curFrameInd == PticaGovorun::NullSampleInd)
		return;

	soundPlayerPlay(audioSamples_.data(), curFrameInd, audioSamples_.size() - 1, false);
}

void SpeechTranscriptionViewModel::soundPlayerTogglePlayPause()
{
	qDebug() << "soundPlayerTogglePlayPause{ isPlaying=" << isPlaying_;
	if (isPlaying_)
		soundPlayerPause();
	else
		soundPlayerPlay();
	qDebug() << "soundPlayerTogglePlayPause} isPlaying=" << isPlaying_;
}

bool SpeechTranscriptionViewModel::soundPlayerIsPlaying() const
{
	return isPlaying_;
}

long SpeechTranscriptionViewModel::playingSampleInd() const
{
	return playingSampleInd_;
}

void SpeechTranscriptionViewModel::setPlayingSampleInd(long value, bool updateViewportOffset)
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

void SpeechTranscriptionViewModel::saveAudioMarkupToXml()
{
	PticaGovorun::saveAudioMarkupToXml(speechAnnot_, annotFilePathAbs_.toStdWString());
	
	nextNotification(formatLogLineWithTime("Saved xml markup."));
}

void SpeechTranscriptionViewModel::saveCurrentRangeAsWavRequest()
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
		nextNotification(QString::fromStdString(msg));
		return;
	}
	nextNotification(QString("Wrote %1 frames to file").arg(len));
}

	QString SpeechTranscriptionViewModel::modelShortName() const
	{
		QFileInfo audioFileInfo(annotFilePathAbs_);
		return audioFileInfo.baseName();
	}

	QString SpeechTranscriptionViewModel::annotFilePath() const
	{
		return annotFilePathAbs_;
	}

	void SpeechTranscriptionViewModel::setAnnotFilePath(const QString& filePath)
	{
		annotFilePathAbs_ = filePath;
	}

	float SpeechTranscriptionViewModel::pixelsPerSample() const
{
	return scale_;
}

float SpeechTranscriptionViewModel::docPaddingX() const
{
	return docPaddingPix_;
}

float SpeechTranscriptionViewModel::docOffsetX() const
{
    return docOffsetX_;
}

void SpeechTranscriptionViewModel::setDocOffsetX(float value)
{
    if (docOffsetX_ != value)
    {
        docOffsetX_ = value;
		emit docOffsetXChanged();
    }
}

float SpeechTranscriptionViewModel::docWidthPix() const
{
    return audioSamples_.size() * scale_ + 2 * docPaddingPix_;
}

bool SpeechTranscriptionViewModel::phoneRulerVisible() const
{
	return phoneRulerVisible_;
}

void SpeechTranscriptionViewModel::scrollDocumentEndRequest()
{
	float maxDocWidth = docWidthPix();\
	scrollPageWithLimits(maxDocWidth);
}

void SpeechTranscriptionViewModel::scrollDocumentStartRequest()
{
	scrollPageWithLimits(0);
}
\
void SpeechTranscriptionViewModel::scrollPageForwardRequest()
{
	static const float PageWidthPix = 500;
	scrollPageWithLimits(docOffsetX_ + PageWidthPix);
}

void SpeechTranscriptionViewModel::scrollPageBackwardRequest()
{
	static const float PageWidthPix = 500;
	scrollPageWithLimits(docOffsetX_ - PageWidthPix);
}

void SpeechTranscriptionViewModel::scrollPageWithLimits(float newDocOffsetX)
{
	float maxDocWidth = docWidthPix();
	if (newDocOffsetX > maxDocWidth)
		newDocOffsetX = maxDocWidth;
	else if (newDocOffsetX < 0)
		newDocOffsetX = newDocOffsetX;
	setDocOffsetX(newDocOffsetX);
}

int SpeechTranscriptionViewModel::lanesCount() const
{
	return lanesCount_;
}

void SpeechTranscriptionViewModel::setLanesCount(int lanesCount)
{
	if (lanesCount_ != lanesCount && lanesCount > 0)
	{
		lanesCount_ = lanesCount;
		emit audioSamplesChanged();
	}
}

void SpeechTranscriptionViewModel::increaseLanesCountRequest()
{
	int newLanesCount = lanesCount() + 1;
	setLanesCount(newLanesCount);
}
void SpeechTranscriptionViewModel::decreaseLanesCountRequest()
{
	int newLanesCount = lanesCount() - 1;
	setLanesCount(newLanesCount);
}


QSizeF SpeechTranscriptionViewModel::viewportSize() const
{
	return viewportSize_;
}

void SpeechTranscriptionViewModel::setViewportSize(float viewportWidth, float viewportHeight)
{
	viewportSize_ = QSizeF(viewportWidth, viewportHeight);
}

float SpeechTranscriptionViewModel::docPosXToSampleInd(float docPosX) const
{
	return (docPosX - docPaddingPix_) / scale_;
}

float SpeechTranscriptionViewModel::sampleIndToDocPosX(long sampleInd) const
{
	return docPaddingPix_ + sampleInd * scale_;
}

bool SpeechTranscriptionViewModel::viewportPosToLocalDocPosX(const QPointF& pos, float& docPosX) const
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

bool SpeechTranscriptionViewModel::docPosToViewport(float docX, ViewportHitInfo& hitInfo) const
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
	PG_DbgAssert2(laneInd < lanesCount_, "DocPos must hit some lane");

	float insideLaneOffset = viewportDocX - laneInd * viewportSize_.width();
	
	hitInfo.LaneInd = laneInd;
	hitInfo.DocXInsideLane = insideLaneOffset;
	return true;
}

RectY SpeechTranscriptionViewModel::laneYBounds(int laneInd) const
{
	float laneHeight = viewportSize_.height() / lanesCount();

	RectY bnd;
	bnd.Top = laneInd * laneHeight;
	bnd.Height = laneHeight;

	return bnd;
}

void SpeechTranscriptionViewModel::deleteRequest(bool isControl)
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
			for (int markerInd = speechAnnot_.markersSize() - 1; markerInd >= 0; --markerInd)
			{
				long markerFrameInd = speechAnnot_.marker(markerInd).SampleInd;
				if (markerFrameInd >= start && markerFrameInd < end)
				{
					bool delMarkerOp = deleteMarker(markerInd);
					PG_DbgAssert(delMarkerOp);
				}
			}
		}
	}

	std::pair<long, long> samplesRange = cursor();
	bool delDiagOp = deleteDiagramSegmentsAtCursor(samplesRange);
	if (delDiagOp)
		return;
}

void SpeechTranscriptionViewModel::refreshRequest()
{
#ifdef PG_HAS_JULIUS
	loadAuxiliaryPhoneticDictionaryRequest();
#endif
}

const std::vector<short>& SpeechTranscriptionViewModel::audioSamples() const
{
	return audioSamples_;
}

const wv::slice<const DiagramSegment> SpeechTranscriptionViewModel::diagramSegments() const
{
	//return wv::make_view(diagramSegments_);
	return wv::make_view(diagramSegments_.data(), diagramSegments_.size());
}

void SpeechTranscriptionViewModel::collectDiagramSegmentIndsOverlappingRange(std::pair<long, long> samplesRange, std::vector<int>& diagElemInds)
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

bool SpeechTranscriptionViewModel::deleteDiagramSegmentsAtCursor(std::pair<long, long> samplesRange)
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

const PticaGovorun::SpeechAnnotation& SpeechTranscriptionViewModel::speechAnnotation() const
{
	return speechAnnot_;
}

const std::vector<PticaGovorun::TimePointMarker>& SpeechTranscriptionViewModel::frameIndMarkers() const
{
	return speechAnnot_.markers();
}

void SpeechTranscriptionViewModel::setCurrentMarkerSpeaker(const std::wstring& speakerBriefId)
{
	if (currentMarkerInd_ == -1)
		return;
	
	// apply the value to current marker
	PticaGovorun::TimePointMarker& m = speechAnnot_.marker(currentMarkerInd_);
	std::wstring oldValue = m.SpeakerBriefId;
	if (oldValue != speakerBriefId)
	{
		m.SpeakerBriefId = speakerBriefId;
		nextNotification(QString("Marker[id=%1].SpeakerBriefId=%2").arg(m.Id).arg(QString::fromStdWString(speakerBriefId)));
	}
}

void SpeechTranscriptionViewModel::setTemplateMarkerLevelOfDetail(PticaGovorun::MarkerLevelOfDetail levelOfDetail)
{
	templateMarkerLevelOfDetail_ = levelOfDetail;
}

PticaGovorun::MarkerLevelOfDetail SpeechTranscriptionViewModel::templateMarkerLevelOfDetail() const
{
	return templateMarkerLevelOfDetail_;
}

PticaGovorun::SpeechLanguage SpeechTranscriptionViewModel::templateMarkerSpeechLanguage() const
{
	return templateMarkerSpeechLanguage_;
}

void SpeechTranscriptionViewModel::dragMarkerStart(const QPointF& localPos, int markerId)
{
	qDebug() << "dragMarkerStart";
	draggingMarkerId_ = markerId;
	isDraggingMarker_ = true;
	draggingLastViewportPos_ = localPos;
}

void SpeechTranscriptionViewModel::dragMarkerStop()
{
	if (isDraggingMarker_)
	{
		qDebug() << "dragMarkerStop";
		draggingMarkerId_ = -1;
		isDraggingMarker_ = false;
	}
}

void SpeechTranscriptionViewModel::dragMarkerContinue(const QPointF& localPos)
{
	if (isDraggingMarker_ && draggingLastViewportPos_ != localPos)
	{
		qDebug() << "dragMarkerContinue";
		PG_DbgAssert(draggingMarkerId_ != -1);

		float mouseMoveLocalDocPosX = -1;
		if (!viewportPosToLocalDocPosX(localPos, mouseMoveLocalDocPosX))
			return;

		float mouseMoveDocPosX = docOffsetX_ + mouseMoveLocalDocPosX;
		long frameInd = (long)docPosXToSampleInd(mouseMoveDocPosX);

		bool updateOp = speechAnnot_.setMarkerFrameInd(draggingMarkerId_, frameInd);
		if (!updateOp)
			return; // The dragged marker's SampleInd was not changed

		draggingLastViewportPos_ = localPos;

		// redraw current marker
		emit audioSamplesChanged();
	}
}

void SpeechTranscriptionViewModel::setLastMousePressPos(const QPointF& localPos, bool isShiftPressed)
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
		const auto& marker = speechAnnot_.marker(closestMarkerInd);
		dragMarkerStart(localPos, marker.Id);
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

long SpeechTranscriptionViewModel::currentSampleInd() const
{
	return cursor_.first;
}

void SpeechTranscriptionViewModel::setCursorInternal(long curSampleInd, bool updateCurrentMarkerInd, bool updateViewportOffset)
{
	auto cursor = cursor_;
	cursor.first = curSampleInd;
	cursor.second = PticaGovorun::NullSampleInd;

	setCursorInternal(cursor, updateCurrentMarkerInd, updateViewportOffset);
}

void SpeechTranscriptionViewModel::setCursorInternal(std::pair<long,long> value, bool updateCurrentMarkerInd, bool updateViewportOffset)
{
	if (cursor_ != value)
	{
		auto oldValue = cursor_;

		cursor_ = value;
		// TODO: the line below emits Qt warning when playing stops
		// QObject::connect: Cannot queue arguments of type 'std::pair<long,long>'
		//	(Make sure 'std::pair<long,long>' is registered using qRegisterMetaType().)
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

std::pair<long, long> SpeechTranscriptionViewModel::cursor() const
{
	return cursor_;
}

std::pair<long, long> SpeechTranscriptionViewModel::cursorOrdered() const
{
	PG_DbgAssert(cursorKind() == TranscriberCursorKind::Range);
	long start = std::min(cursor_.first, cursor_.second);
	long end   = std::max(cursor_.first, cursor_.second);
	return std::make_pair(start,end);
}

TranscriberCursorKind SpeechTranscriptionViewModel::cursorKind() const
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
		PG_DbgAssert2(cursor_.second == PticaGovorun::NullSampleInd, "Cursor invariant: when first position is empty, the second position must be empty too");
		return TranscriberCursorKind::Empty;
	}
}

void SpeechTranscriptionViewModel::insertNewMarkerAtCursorRequest()
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

void SpeechTranscriptionViewModel::insertNewMarker(const PticaGovorun::TimePointMarker& marker, bool updateCursor, bool updateViewportOffset)
{
	int newMarkerInd = speechAnnot_.insertMarker(marker);

	setCurrentMarkerIndInternal(newMarkerInd, updateCursor, updateViewportOffset);

	// TODO: repaint only current marker
	emit audioSamplesChanged();
}

bool SpeechTranscriptionViewModel::deleteMarker(int markerInd)
{
	bool delOp = speechAnnot_.deleteMarker(markerInd);
	if (delOp)
		setCurrentMarkerIndInternal(-1, false, false);

	return delOp;
}

void SpeechTranscriptionViewModel::selectMarkerClosestToCurrentCursorRequest()
{
	long curFrameInd = currentSampleInd();

	long distToClosestMarker = -1;
	int closestMarkerInd = speechAnnot_.getClosestMarkerInd(curFrameInd, &distToClosestMarker);

	setCurrentMarkerIndInternal(closestMarkerInd, true, false);
}

void SpeechTranscriptionViewModel::selectMarkerForward()
{
	selectMarkerInternal(true);
}

void SpeechTranscriptionViewModel::selectMarkerBackward()
{
	selectMarkerInternal(false);
}

void SpeechTranscriptionViewModel::selectMarkerInternal(bool moveForward)
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
		PG_DbgAssert(rightMarkerInd < speechAnnot_.markers().size());
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

void SpeechTranscriptionViewModel::setCurrentMarkerIndInternal(int markerInd, bool updateCursor, bool updateViewportOffset)
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

int SpeechTranscriptionViewModel::currentMarkerInd() const
{
	return currentMarkerInd_;
}

void SpeechTranscriptionViewModel::setCurrentMarkerTranscriptText(const QString& text)
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
			// silence has no language, has no speaker
			if (marker.TranscripText != toQString(fillerSilence()))
			{
				// inhale has speaker but has no language
				if (marker.TranscripText != toQString(fillerInhale()))
				{
					// first time initialization?
					if (marker.Language == PticaGovorun::SpeechLanguage::NotSet)
						marker.Language = templateMarkerSpeechLanguage_;
				}

				// first time initialization?
				if (marker.SpeakerBriefId.empty())
					marker.SpeakerBriefId = speechAnnot_.inferRecentSpeaker(currentMarkerInd_);
			}
		}

		if (marker.Language == SpeechLanguage::Ukrainian)
		{
			QStringList validResult;
			speechData_->validateUtteranceHasPhoneticExpansion(text, validResult);
			if (!validResult.isEmpty())
			{
				QString msg = validResult.join('\n');
				nextNotification(formatLogLineWithTime(msg));
			}
		}

		emit currentMarkerIndChanged();
	}
	else
		cursorTextToAlign_ = text;
}

void SpeechTranscriptionViewModel::setCurrentMarkerLevelOfDetail(PticaGovorun::MarkerLevelOfDetail levelOfDetail)
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

void SpeechTranscriptionViewModel::setCurrentMarkerStopOnPlayback(bool stopsPlayback)
{
	if (currentMarkerInd_ == -1)
		return;
	speechAnnot_.marker(currentMarkerInd_).StopsPlayback = stopsPlayback;
}

	void SpeechTranscriptionViewModel::setCurrentMarkerExcludePhase(boost::optional<ResourceUsagePhase> excludePhase)
	{
		if (currentMarkerInd_ == -1)
			return;
		speechAnnot_.marker(currentMarkerInd_).ExcludePhase = excludePhase;
	}

	void SpeechTranscriptionViewModel::setCurrentMarkerLang(PticaGovorun::SpeechLanguage lang)
{
	std::string langStr = speechLanguageToStr(lang);
	if (currentMarkerInd_ == -1)
	{
		// apply user selection as a template for new markers
		if (templateMarkerSpeechLanguage_ != lang)
		{
			templateMarkerSpeechLanguage_ = lang;
			nextNotification(QString("Speech language for new markers=%1").arg(langStr.c_str()));
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
			nextNotification(QString("Marker[id=%1].Language=%2").arg(m.Id).arg(langStr.c_str()));
		}
	}
}

	QString SpeechTranscriptionViewModel::currentMarkerPhoneListString() const
	{
		if (currentMarkerInd_ == -1)
			return QString();
		const TimePointMarker& marker = speechAnnot_.marker(currentMarkerInd_);
		QString text = marker.TranscripText;

		std::vector<wchar_t> textBuff;
		boost::wstring_view textRef = toWStringRef(text, textBuff);

		std::string phoneListString;
		bool convOp;
		const char* errMsg;
		std::tie(convOp, errMsg) = speechData_->convertTextToPhoneListString(textRef, phoneListString);
		if (!convOp)
		{
			nextNotification(QString::fromLatin1(errMsg));
			return QString();
		}

		return QString::fromStdString(phoneListString);
	}

	void SpeechTranscriptionViewModel::onPronIdPhoneticSpecChanged()
	{
		// update phonetic expansion of current marker's text
		emit currentMarkerIndChanged();
	}

#ifdef PG_HAS_JULIUS
	void SpeechTranscriptionViewModel::ensureRecognizerIsCreated()
{
	// initialize the recognizer lazily
	if (recognizer_ == nullptr)
	{
		std::string recogName = recognizerNameHintProvider_->recognizerNameHint();
		recognizer_ = juliusRecognizerProvider_->instance(recogName);
		if (recognizer_ == nullptr)
		{
			nextNotification("Can't find speech recognizer");
			return;
		}
	}
}

void SpeechTranscriptionViewModel::recognizeCurrentSegmentJuliusRequest()
{
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
		nextNotification(QString("WARN: Segment is too large. Limiting it from %1 to %2 samles").arg(oldLen).arg(len));
	}

	// pad the audio with silince
	PticaGovorun::padSilence(&audioSamples_[curSegBeg], len, silencePadAudioSamplesCount_, audioSegmentBuffer_);

	PticaGovorun::JuiliusRecognitionResult recogResult;
	auto recogOp = recognizer_->recognize(FrameToSamplePicker, audioSegmentBuffer_.data(), audioSegmentBuffer_.size(), recogResult);
	if (!std::get<0>(recogOp))
	{
		auto err = std::get<1>(recogOp);
		nextNotification(QString::fromStdString(err));
		return;
	}

	if (!recogResult.AlignmentErrorMessage.empty())
		nextNotification(QString::fromStdString(recogResult.AlignmentErrorMessage));

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
	nextNotification(diagSeg.RecogSegmentWords);
}

void SpeechTranscriptionViewModel::alignPhonesForCurrentSegmentJuliusRequest()
{
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
		nextNotification(QString::fromStdWString(errMsg));
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

	nextNotification(QString("Alignment score=%1").arg(alignmentResult.AlignmentScore));

	// redraw current segment
	emit audioSamplesChanged();
}

void SpeechTranscriptionViewModel::ensureWordToPhoneListVocabularyLoaded()
{
	using namespace PticaGovorun;

	QTextCodec* pTextCodec = QTextCodec::codecForName(PGEncodingStr);
	PG_Assert2(pTextCodec != nullptr, "Can't load text encoding");

	// TODO: vocabulary is recognizer dependent
	if (wordToPhoneListDict_.empty())
	{
		QString dicPath = QString::fromStdString(recognizer_->settings().DictionaryFilePath);

		clock_t startTime = std::clock();
		std::tuple<bool, const char*> loadOp = PticaGovorun::loadPronunciationVocabulary(dicPath.toStdWString(), wordToPhoneListDict_, *pTextCodec);
		double elapsedTime1 = static_cast<double>(clock() - startTime) / CLOCKS_PER_SEC;
		if (!std::get<0>(loadOp))
		{
			nextNotification(QString(std::get<1>(loadOp)));
			return;
		}
		nextNotification(QString("Loaded pronunciation dictionary in %1 secs").arg(elapsedTime1, 2));
	}

	if (wordToPhoneListAuxiliaryDict_.empty())
		loadAuxiliaryPhoneticDictionaryRequest();
}

void SpeechTranscriptionViewModel::loadAuxiliaryPhoneticDictionaryRequest()
{
	using namespace PticaGovorun;
	QTextCodec* pTextCodec = QTextCodec::codecForName(PGEncodingStr);
	PG_Assert2(pTextCodec != nullptr, "Can't load text encoding");

	const char* dicPatCStr = qgetenv("PG_AUX_DICT_PATH").constData();
	QString dicPath = QString(dicPatCStr);
	if (dicPath.isEmpty())
	{
		nextNotification("Specify path PG_AUX_DICT_PATH to auxiliary dictionary");
		return;
	}

	clock_t startTime = std::clock();
	wordToPhoneListAuxiliaryDict_.clear();
	std::tuple<bool, const char*> loadOp = PticaGovorun::loadPronunciationVocabulary(dicPath.toStdWString(), wordToPhoneListAuxiliaryDict_, *pTextCodec);
	double elapsedTime1 = static_cast<double>(clock() - startTime) / CLOCKS_PER_SEC;
	if (!std::get<0>(loadOp))
	{
		nextNotification(QString(std::get<1>(loadOp)));
		return;
	}
	nextNotification(QString("Loaded pronunciation AX dictionary in %1 secs").arg(elapsedTime1, 2));
}
#endif


#if PG_HAS_SPHINX
void SpeechTranscriptionViewModel::recognizeCurrentSegmentSphinxRequest()
{
	using namespace PticaGovorun;

	long curSegBeg;
	long curSegEnd;
	std::tie(curSegBeg, curSegEnd) = getSampleRangeToPlay(currentSampleInd(), SegmentStartFrameToPlayChoice::SegmentBegin);
	auto len = curSegEnd - curSegBeg;

	// pad the audio with silince
	PticaGovorun::padSilence(&audioSamples_[curSegBeg], len, silencePadAudioSamplesCount_, audioSegmentBuffer_);

	// The name of the directory with sphinx speech model data.
	QString sphinxModelDirName = AppHelpers::configParamQString("SphinxModelDirName", "persian");

	std::string hmmPath = AppHelpers::mapPathStdString(QString("data/TrainSphinx/%1/model_parameters/persian.cd_cont_200").arg(sphinxModelDirName));
	std::string langModelPath = AppHelpers::mapPathStdString(QString("data/TrainSphinx/%1/etc/persian_test.lm.DMP").arg(sphinxModelDirName));
	std::string dictPath = AppHelpers::mapPathStdString(QString("data/TrainSphinx/%1/etc/persian_test.dic").arg(sphinxModelDirName));
	cmd_ln_t *config = SphinxConfig::pg_init_cmd_ln_t(hmmPath, langModelPath, dictPath, true, false, true, boost::string_view());
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

	int ret = ps_start_utt(ps);
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
	const char* hyp = ps_get_hyp(ps, &score);
	if (hyp == nullptr)
	{
		QString msg("No hypothesis is available");
		nextNotification(msg);
		return;
	}

	QTextCodec* textCodec = QTextCodec::codecForName("utf8");
	QString hypQStr = textCodec->toUnicode(hyp);
	std::wstring hypWStr = hypQStr.toStdWString();


	// word segmentation
	std::vector<PticaGovorun::AlignedWord> wordBoundaries;
	for (ps_seg_t *seg = ps_seg_iter(ps); seg; seg = ps_seg_next(seg))
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
	nextNotification(hypQStr);

	// redraw current segment
	emit audioSamplesChanged();
}

#endif

void SpeechTranscriptionViewModel::dumpSilence(long i1, long i2)
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

void SpeechTranscriptionViewModel::dumpSilence()
{
	using namespace PticaGovorun;
	if (cursorKind() != TranscriberCursorKind::Range)
		return;
	long start = std::min(cursor_.first, cursor_.second);
	long end = std::max(cursor_.first, cursor_.second);
	dumpSilence(start, end);
}

void SpeechTranscriptionViewModel::analyzeUnlabeledSpeech()
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

		PG_DbgAssert(wndIsSilence[i]);

		long left = wnds[i].Start;

		// find end of silence
		for (i = i + 1; i < wndsCount && wndIsSilence[i]; ++i) {}

		PG_DbgAssert(wndIsSilence[i - 1]);

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

size_t SpeechTranscriptionViewModel::silencePadAudioFramesCount() const
{
	return silencePadAudioSamplesCount_;
}

void SpeechTranscriptionViewModel::playComposingRecipeRequest(QString recipe)
{
	composedAudio_.clear();

	// Composition recipe format:
	// characters after # signs are comments
	// 0 section contains the name of the file
	// 1 section has format 3-15 meaning play audio from markerId=3 to markerId=15
	// R3 means repeat this section 3 times
	// empty lines are ignored
	// eg: "test.wav;3-15;R3" means, play from markerId=3 to markerId=15 three times

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
			nextNotification("Can't parse fromMarkerInd=" + fromMarkerIdStr);
			return;
		}
		int toMarkerId = toMarkerIdStr.toInt(&parseOp);
		if (!parseOp)
		{
			nextNotification("Can't parse toMarkerInd=" + fromMarkerIdStr);
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

void SpeechTranscriptionViewModel::setAudioMarkupNavigator(std::shared_ptr<AudioMarkupNavigator> audioMarkupNavigator)
{
	audioMarkupNavigator_ = audioMarkupNavigator;
}

void SpeechTranscriptionViewModel::navigateToMarkerRequest()
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

void SpeechTranscriptionViewModel::setPhoneticDictViewModel(std::shared_ptr<PticaGovorun::PhoneticDictionaryViewModel> phoneticDictViewModel)
{
	PG_Assert2(phoneticDictViewModel != nullptr, "")
	phoneticDictViewModel_ = phoneticDictViewModel;
}

void SpeechTranscriptionViewModel::setSpeechData(std::shared_ptr<SpeechData> speechData)
{
	PG_Assert2(speechData != nullptr, "");
	speechData_ = speechData;
}

std::shared_ptr<PticaGovorun::PhoneticDictionaryViewModel> SpeechTranscriptionViewModel::phoneticDictViewModel()
{
	return phoneticDictViewModel_;
}

	void SpeechTranscriptionViewModel::setNotificationService(std::shared_ptr<VisualNotificationService> _)
	{
		notificationService_ = _;
	}

	void SpeechTranscriptionViewModel::nextNotification(const QString& message) const
	{
		if (notificationService_ != nullptr)
			notificationService_->nextNotification(message);
	}
} // namespace PticaGovorun