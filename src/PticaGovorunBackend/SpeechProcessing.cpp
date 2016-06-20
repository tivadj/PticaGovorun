#include "stdafx.h"
#include <regex>
#include <memory>
#include <cctype> // std::isalpha
#include <numeric> // std::accumulate

#include <QDir>

#include <opencv2\core.hpp> // cv::Mat
#include <opencv2\ml.hpp> // cv::EM

#include "SpeechProcessing.h"
#include "XmlAudioMarkup.h"
#include "WavUtils.h"
#include "MLUtils.h"
#include "JuliusToolNativeWrapper.h"
#include "InteropPython.h"
#include "PticaGovorunCore.h"
#include "PhoneticService.h"

namespace PticaGovorun 
{
	bool getDefaultMarkerStopsPlayback(MarkerLevelOfDetail levelOfDetail)
	{
		if (levelOfDetail == MarkerLevelOfDetail::Word)
			return true;

		// do not stop on phone markers
		return false;
	}

	PG_EXPORTS std::string speechLanguageToStr(SpeechLanguage lang)
	{
		switch (lang)
		{
		case SpeechLanguage::Russian:
			return "ru";
		case SpeechLanguage::Ukrainian:
			return "uk";
		default:
			return "";
		}
	}

	boost::string_ref toString(ResourceUsagePhase phase)
	{
		switch (phase)
		{
		case ResourceUsagePhase::Train:
			return "train";
		case ResourceUsagePhase::Test:
			return "test";
		default:
			return nullptr;
		}
	}

	boost::optional<ResourceUsagePhase> resourceUsagePhaseFromString(boost::string_ref phase)
	{
		if (phase.compare("train") == 0)
			return ResourceUsagePhase::Train;
		if (phase.compare("test") == 0)
			return ResourceUsagePhase::Test;
		return nullptr;
	}

	void splitUtteranceIntoWords(boost::wstring_ref text, std::vector<boost::wstring_ref>& words)
	{
		std::wcmatch matchRes;
		// clothes clothes(1)
		// apostrophe (eg: п'ять)
		// dash (eg: to-do)
		// angle bracket, slash <s> <sil> </sil>
		// square bracket [sp] [inh]
		static std::wregex r(LR"regex([\w\(\)\[\]'-<>/]+)regex"); // match words

		const wchar_t* wordBeg = std::begin(text);
		while (std::regex_search(wordBeg, std::cend(text), matchRes, r))
		{
			auto& wordSlice = matchRes[0];
			boost::wstring_ref word = boost::wstring_ref(&*wordSlice.first, wordSlice.second - wordSlice.first);
			words.push_back(word);

			wordBeg = wordSlice.second;
		}
	}

	// wordToPhoneListFun returns false, if word can't be translated into phone list
	std::tuple<bool, std::wstring> convertTextToPhoneList(const std::wstring& text, std::function<auto (const std::wstring&, std::vector<std::string>&) -> bool> wordToPhoneListFun, bool insertShortPause, std::vector<std::string>& speechPhones)
	{
		std::wstring word;
		word.reserve(32);

		std::vector<std::string> wordPhones;
		wordPhones.reserve(word.capacity());

		// insert the starting silence phone
		speechPhones.push_back(PGPhoneSilence);

		std::vector<boost::wstring_ref> words;
		splitUtteranceIntoWords(text, words);

		// iterate through words
		for (boost::wstring_ref wordRef : words)
		{
			toStdWString(wordRef, word);

			// convert word to phones
			wordPhones.clear();
			if (!wordToPhoneListFun(word, wordPhones))
			{
				std::wstringstream errBuf;
				errBuf << "Word '" << word << "' is not found in the dictionary";
				return std::make_tuple(false, errBuf.str());
			}

			std::copy(std::begin(wordPhones), std::end(wordPhones), std::back_inserter(speechPhones));

			// insert the silence phone between words
			if (insertShortPause)
				speechPhones.push_back(PGShortPause);
		}

		// remove last short pause phone
		if (insertShortPause)
		{
			speechPhones.pop_back();
		}

		// set the last pohone to be a silence phone
		speechPhones.push_back(PGPhoneSilence);

		return std::make_tuple(true, L"");
	}

	void padSilence(const short* audioFrames, int audioFramesCount, int silenceFramesCount, std::vector<short>& paddedAudio)
	{
		PG_Assert(audioFrames != nullptr);
		PG_Assert(audioFramesCount >= 0);
		PG_Assert(silenceFramesCount >= 0);

		paddedAudio.resize(audioFramesCount + 2 * silenceFramesCount);

		static const auto FrameSize = sizeof(short);
		int tmp = sizeof(decltype(paddedAudio[0]));
		
		// silence before the audio
		std::memset(paddedAudio.data(), 0, silenceFramesCount * FrameSize);

		// audio
		std::memcpy(paddedAudio.data() + silenceFramesCount, audioFrames, audioFramesCount * FrameSize);

		// silence after the audio
		std::memset(paddedAudio.data() + silenceFramesCount + audioFramesCount, 0, silenceFramesCount * FrameSize);
	}

	void mergeSamePhoneStates(const std::vector<AlignedPhoneme>& phoneStates, std::vector<AlignedPhoneme>& monoPhones)
	{
		const char* UnkStateName = "?";

		for (size_t mergeStateInd = 0; mergeStateInd < phoneStates.size();)
		{
			const auto& stateToMerge = phoneStates[mergeStateInd];
			auto stateName = stateToMerge.Name;
			if (stateName == UnkStateName)
			{
				// unknown phone will not be merged
				monoPhones.push_back(stateToMerge);
				++mergeStateInd;
				continue;
			}

			auto phoneNameFromStateName = [](const std::string& stateName, std::string& resultPhoneName) -> bool
			{
				// phone name is a prefix before the numbers
				size_t nonCharInd = 0;
				for (; nonCharInd < stateName.size(); ++nonCharInd)
					if (!std::isalpha(stateName[nonCharInd])) // stop on first number
						break;

				if (nonCharInd == 0) // weid phone name starts with number; do not merge it
					return false;

				resultPhoneName = std::string(stateName.data(), stateName.data() + nonCharInd);
				return true;
			};

			std::string phoneName;
			if (!phoneNameFromStateName(stateName, phoneName))
			{
				monoPhones.push_back(stateToMerge);
				++mergeStateInd;
				continue;
			}

			// do merging

			auto mergePhone = stateToMerge;
			mergePhone.Name = phoneName; // update phone name instead of state name

			// merge this phone with next phones of the same name
			size_t nextStateInd = mergeStateInd + 1;
			for (; nextStateInd < phoneStates.size(); nextStateInd++)
			{
				// stop merging if stuck into unknown phone
				std::string neighStateName = phoneStates[nextStateInd].Name;
				if (neighStateName == UnkStateName)
					break;

				std::string neighPhoneName;
				// stop merging if phone name can't be extracted
				if (!phoneNameFromStateName(neighStateName, neighPhoneName))
					break;

				// stop merging if different phones
				if (phoneName != neighPhoneName)
					break;

				// merge
				mergePhone.EndFrameIncl = phoneStates[nextStateInd].EndFrameIncl;
				mergePhone.EndSample = phoneStates[nextStateInd].EndSample;
			}
			monoPhones.push_back(mergePhone);
			mergeStateInd = nextStateInd;
		}
	}

	void frameRangeToSampleRange(size_t framBegIndex, size_t framEndIndex, LastFrameSample endSampleChoice, size_t frameSize, size_t frameShift, long& sampleBeg, long& sampleEnd)
	{
		switch (endSampleChoice)
		{
		case LastFrameSample::BeginOfTheNextFrame:
		{
			sampleBeg = framBegIndex * frameShift;
			sampleEnd = (1 + framEndIndex) * frameShift;
			break;
		}
		case LastFrameSample::EndOfThisFrame:
		{
			sampleBeg = framBegIndex * frameShift;
			sampleEnd = framEndIndex * frameShift + frameSize;
			break;
		}
		case LastFrameSample::MostLikely:
		{
			int dx = static_cast<size_t>(0.5 * (frameSize + frameShift));
			sampleEnd = framEndIndex      * frameShift + dx;

			// segment bound lie close
			// begin of this frame is the end of the previous frame
			sampleBeg = (framBegIndex - 1) * frameShift + dx;
			break;
		}
		default:
			PG_Assert(false && "Unrecognized value of LastFrameSample enum");
		}
	}

	// Returns FrameInd which corresponds to FrameInd in resampled audio.
	long frameIndToResampled(long srcFrameInd, float srcFrameRate, float dstFrameRate)
	{
		double timePoint = srcFrameInd / srcFrameRate;
		long result = static_cast<long>(timePoint * dstFrameRate);
		return result;
	}

	std::wstring speechAnnotationFilePathAbs(const std::wstring& wavFileAbs, const std::wstring& wavRootDir, const std::wstring& annotRootDir)
	{
		QString wavFileAbsQ = QString::fromStdWString(wavFileAbs);
		QFileInfo wavFileInfo(wavFileAbsQ);
		QString speechAnnotFileName = wavFileInfo.completeBaseName() + ".xml";

		// find wav relative path
		QDir speechWavDir(QString::fromStdWString(wavRootDir));
		QString wavRelPath = speechWavDir.relativeFilePath(wavFileAbsQ);

		// mount it to annotation directory
		QString wavPathNew = QDir(QString::fromStdWString(annotRootDir)).absoluteFilePath(wavRelPath);
		QString annotPathAbs = QFileInfo(wavPathNew).absoluteDir().filePath(speechAnnotFileName);
		return annotPathAbs.toStdWString();
	}

	enum class ShortPauseUsage
	{
		Ignore,
		UseAsSilence
	};

	std::tuple<bool, const char*> loadSpeechAndAnnotation(const QFileInfo& folderOrWavFilePath, const std::wstring& wavRootDir, const std::wstring& annotRootDir, 
		MarkerLevelOfDetail targetLevelOfDetail, bool loadAudio, std::function<auto(const AnnotatedSpeechSegment& seg)->bool> segPredBefore, std::vector<AnnotatedSpeechSegment>& segments)
	{
		if (folderOrWavFilePath.isDir())
		{
			QString dirAbsPath = folderOrWavFilePath.absoluteFilePath();
			QDir dir(dirAbsPath);

			QFileInfoList items = dir.entryInfoList(QDir::Filter::Files | QDir::Filter::Dirs | QDir::Filter::NoDotAndDotDot);
			for (const QFileInfo item : items)
			{
				auto subOp = loadSpeechAndAnnotation(item, wavRootDir, annotRootDir, targetLevelOfDetail, loadAudio, segPredBefore, segments);
				if (!std::get<0>(subOp))
					return subOp;
			}
			return std::make_pair(true, "");
		}

		// process wav file path

		QString wavFilePath = folderOrWavFilePath.absoluteFilePath();

		QString extension = folderOrWavFilePath.suffix();
		if (extension.compare("wav", Qt::CaseInsensitive)) // skip non wav files
			return std::make_pair(true, "");

		QString xmlFilePath = QString::fromStdWString(speechAnnotationFilePathAbs(folderOrWavFilePath.absoluteFilePath().toStdWString(), wavRootDir, annotRootDir));
		QFileInfo xmlFilePathInfo(xmlFilePath);
		if (!xmlFilePathInfo.exists()) // wav has no corresponding markup
			return std::make_pair(true, "");

		// load audio markup
		// not all wav files has a markup, hence try to load the markup first
		QString audioMarkupFilePathAbs = xmlFilePathInfo.absoluteFilePath();
		SpeechAnnotation speechAnnot;
		std::tuple<bool, const char*> loadOp = loadAudioMarkupFromXml(audioMarkupFilePathAbs.toStdWString(), speechAnnot);
		if (!std::get<0>(loadOp))
			return loadOp;

		const std::vector<TimePointMarker>& syncPoints = speechAnnot.markers();

		// take only markers of requested type
		std::vector<TimePointMarker> markersOfInterest;
		std::copy_if(std::begin(syncPoints), std::end(syncPoints), std::back_inserter(markersOfInterest), [=](const TimePointMarker& m)
		{
			return m.LevelOfDetail == targetLevelOfDetail;
		});

		// determine the segment's bounds
		// if a text segment is flanked with the silence segment then the text segment is aumented with silence
		struct SegmentBlank
		{
			const TimePointMarker* ContentMarker = nullptr; // the marker with text

			// start/end markers may be different from content marker, if text segment is flanked with silence to the start/end
			const TimePointMarker* StartMarker = nullptr;
			const TimePointMarker* EndMarker = nullptr;
			bool HasStartSilence = false;
			bool HasEndSilence = false;
			long StartSilenceFramesCount = 0;
			long EndSilenceFramesCount = 0;
		};
		ShortPauseUsage shortPauseUsage = ShortPauseUsage::UseAsSilence;
		QString fillerSilQ = toQString(fillerSilence());
		QString fillerSpQ = toQString(fillerShortPause());
		std::vector<SegmentBlank> blankSegs;
		for (int i = 0; i < (int)markersOfInterest.size() - 1; ++i)
		{
			const auto& marker = markersOfInterest[i];

			if (marker.TranscripText.isEmpty()) // ignore empty segments
				continue;
			if (marker.TranscripText.contains(QChar('#'))) // ignore markers with pound #
				continue;
			if (marker.TranscripText == fillerSilQ) // skip silence segments, attach it to neighbour text segments
				continue;
			if (marker.TranscripText == fillerSpQ) // later [sp] is ignored or used as silence
				continue;

			SegmentBlank blankSeg;
			blankSeg.ContentMarker = &marker;
			blankSeg.StartMarker = &marker;
			blankSeg.EndMarker = &markersOfInterest[i + 1];
			
			if (i > 0)
			{
				const auto& prevMarker = markersOfInterest[i - 1];
				if (prevMarker.TranscripText == fillerSilQ ||
					prevMarker.TranscripText == fillerSpQ && shortPauseUsage == ShortPauseUsage::UseAsSilence)
				{
					blankSeg.StartMarker = &prevMarker;
					blankSeg.HasStartSilence = true;
					blankSeg.StartSilenceFramesCount = marker.SampleInd - prevMarker.SampleInd;
				}
			}
			if (i + 2 < markersOfInterest.size() && (
				markersOfInterest[i + 1].TranscripText == fillerSilQ ||
				markersOfInterest[i + 1].TranscripText == fillerSpQ && shortPauseUsage == ShortPauseUsage::UseAsSilence
				))
			{
				const auto& nextNextMarker = markersOfInterest[i + 2]; // the end of next silence segment
				blankSeg.EndMarker = &nextNextMarker;
				blankSeg.HasEndSilence = true;
				blankSeg.EndSilenceFramesCount = nextNextMarker.SampleInd - markersOfInterest[i+1].SampleInd;
			}
			blankSegs.push_back(blankSeg);
		}

		// audio frames are lazy loaded
		float frameRate = -1;
		std::vector<short> speechFrames;

		// remove [sp]
		std::vector<boost::wstring_ref> words;
		std::vector<boost::wstring_ref> wordsNoSp;

		// collect annotated frames
		// two consequent markers form a segment of intereset
		for (const SegmentBlank& blankSeg : blankSegs)
		{
			AnnotatedSpeechSegment seg;
			seg.SegmentId = blankSeg.ContentMarker->Id;
			seg.StartMarkerId = blankSeg.StartMarker->Id;
			seg.EndMarkerId = blankSeg.EndMarker->Id;
			seg.StartMarker = *blankSeg.StartMarker;
			seg.EndMarker = *blankSeg.EndMarker;
			seg.ContentMarker = *blankSeg.ContentMarker;
			seg.FilePath = wavFilePath.toStdWString();
			seg.AudioStartsWithSilence = blankSeg.HasStartSilence;
			seg.AudioEndsWithSilence = blankSeg.HasEndSilence;
			seg.StartSilenceFramesCount = blankSeg.StartSilenceFramesCount;
			seg.EndSilenceFramesCount = blankSeg.EndSilenceFramesCount;
			QString txt = blankSeg.ContentMarker->TranscripText;

			bool skipShortPause = true;
			if (skipShortPause)
			{
				std::wstring txtW = txt.toStdWString();
				words.clear();
				wordsNoSp.clear();
				splitUtteranceIntoWords(txtW, words);

				seg.TranscriptionStartsWithSilence = words.front() == fillerStartSilence();
				seg.TranscriptionEndsWithSilence = words.front() == fillerEndSilence();

				for (size_t i = 0; i < words.size(); ++i)
				{
					auto word = words[i];
					if (word == fillerShortPause())
					{
						wordsNoSp.push_back(fillerSilence()); // [sp] -> <sil>
						continue;
					}
					//if (word == fillerShortPause() || word == fillerSilence()) // [sp] -> [], <sil> -> []
					//	continue;
					wordsNoSp.push_back(word);
				}

				std::wstringstream buf;
				join(wordsNoSp.begin(), wordsNoSp.end(), std::wstring(L" "), buf);

				txt = QString::fromStdWString(buf.str());
			}
			seg.TranscriptText = txt.toStdWString();
			seg.Language = blankSeg.ContentMarker->Language;

			// filter out unwanted segments
			if (!segPredBefore(seg))
				continue;

			if (loadAudio)
			{
				// lazy load audio samples
				if (speechFrames.empty())
				{
					// load wav file
					auto readOp = readAllSamples(wavFilePath.toStdString(), speechFrames, &frameRate);
					if (!std::get<0>(readOp))
						return std::make_pair(false, "Can't read wav file");
				}

				seg.FrameRate = frameRate;

				long frameStart = blankSeg.StartMarker->SampleInd;
				long frameEnd = blankSeg.EndMarker->SampleInd;

				seg.Frames = std::vector<short>(&speechFrames[frameStart], &speechFrames[frameEnd]);
			}
			segments.push_back(seg);
		}

		return std::make_pair(true, "");
	}

	void collectAnnotatedSegments(const std::vector<TimePointMarker>& markers, std::vector<std::pair<const TimePointMarker*, const TimePointMarker*>>& segments)
	{
		std::vector<const TimePointMarker*> markersOfInterest;
		markersOfInterest.reserve(markers.size());
		for (size_t markerInd = 0; markerInd < markers.size(); ++markerInd)
		{
			const PticaGovorun::TimePointMarker& marker = markers[markerInd];
			if (marker.LevelOfDetail == MarkerLevelOfDetail::Word)
				markersOfInterest.push_back(&marker);
		}

		for (size_t markerInd = 0; markerInd < markersOfInterest.size() - 1; ++markerInd)
		{
			const PticaGovorun::TimePointMarker* marker1 = markersOfInterest[markerInd];
			if (!marker1->TranscripText.isEmpty())
			{
				const PticaGovorun::TimePointMarker* marker2 = markersOfInterest[markerInd + 1];
				segments.push_back(std::make_pair(marker1, marker2));
			}
		}
	}

	// phoneNameToFeaturesVector[phone] has mfccVecLen features for one frame, then for another frame, etc.
	std::tuple<bool, const char*> collectMfccFeatures(const QFileInfo& folderOrWavFilePath, int frameSize, int frameShift, int mfccVecLen, std::map<std::string, std::vector<float>>& phoneNameToFeaturesVector)
	{
		if (folderOrWavFilePath.isDir())
		{
			QString dirAbsPath = folderOrWavFilePath.absoluteFilePath();
			QDir dir(dirAbsPath);
			
			QFileInfoList items = dir.entryInfoList(QDir::Filter::Files | QDir::Filter::Dirs | QDir::Filter::NoDotAndDotDot);
			for (const QFileInfo item : items)
			{
				auto subOp = collectMfccFeatures(item, frameSize, frameShift, mfccVecLen, phoneNameToFeaturesVector);
				if (!std::get<0>(subOp))
					return subOp; 
			}
			return std::make_pair(true, "");
		}

		// process wav file path

		QString wavFilePath = folderOrWavFilePath.absoluteFilePath();

		QString extension = folderOrWavFilePath.suffix();
		if (extension != "wav") // skip non wav files
			return std::make_pair(true, "");
		
		QDir parentDir = folderOrWavFilePath.absoluteDir();
		QString xmlFileName = folderOrWavFilePath.completeBaseName() + ".xml";

		QString xmlFilePath = parentDir.absoluteFilePath(xmlFileName);
		QFileInfo xmlFilePathInfo(xmlFilePath);
		if (!xmlFilePathInfo.exists()) // wav has no corresponding markup	
			return std::make_pair(true, "");

		// load audio markup
		// not all wav files has a markup, hence try to load the markup first

		QString audioMarkupFilePathAbs = xmlFilePathInfo.absoluteFilePath();
		SpeechAnnotation speechAnnot;
		std::tuple<bool, const char*> loadOp = loadAudioMarkupFromXml(audioMarkupFilePathAbs.toStdWString(), speechAnnot);
		if (!std::get<0>(loadOp))
			return loadOp;

		const std::vector<TimePointMarker>& syncPoints = speechAnnot.markers();

		// load wav file
		std::vector<short> audioSamples;
		auto readOp = PticaGovorun::readAllSamples(wavFilePath.toStdString(), audioSamples);
		if (!std::get<0>(readOp))
			return std::make_pair(false, "Can't read wav file");

		// compute MFCC features
		for (int i = 0; i < syncPoints.size(); ++i)
		{
			const auto& marker = syncPoints[i];
			if (marker.LevelOfDetail == PticaGovorun::MarkerLevelOfDetail::Phone &&
				!marker.TranscripText.isEmpty() &&
				i + 1 < syncPoints.size())
			{
				std::string phoneName = marker.TranscripText.toStdString();
				int phoneId = phoneNameToPhoneId(phoneName);
				if (phoneId == -1)
					continue;
				
				auto it = phoneNameToFeaturesVector.find(phoneName);
				if (it == std::end(phoneNameToFeaturesVector))
					phoneNameToFeaturesVector.insert(std::make_pair(phoneName, std::vector<float>()));

				std::vector<float>& mfccFeatures = phoneNameToFeaturesVector[phoneName];
				long begSampleInd = marker.SampleInd;
				long endSampleInd = syncPoints[i + 1].SampleInd;

				int len = endSampleInd - begSampleInd;

				// Impl: Julius

				// collect features
				// note, this call requires initialization of Julius library
				//int framesCount;
				//auto featsOp = PticaGovorun::computeMfccFeaturesPub(&audioSamples[begSampleInd], len, frameSize, frameShift, mfccVecLen, mfccFeatures, framesCount);
				//if (!std::get<0>(featsOp))
				//	return featsOp;

				// Impl: PticaGovorun

				int sampleRate = SampleRate;
				int binCount = 24; // number of bins in the triangular filter bank
				int fftNum = getMinDftPointsCount(frameSize);
				TriangularFilterBank filterBank;
				buildTriangularFilterBank(sampleRate, binCount, fftNum, filterBank);

				const int mfccCount = 12;
				// +1 for usage of cepstral0 coef
				// *3 for velocity and acceleration coefs
				int mfccVecLen = 3 * (mfccCount + 1);

				int framesCount2 = slidingWindowsCount(len, frameSize, frameShift);
				std::vector<float> mfccFeaturesTmp(mfccVecLen*framesCount2, 0);
				wv::slice<short> samplesPart = wv::make_view(audioSamples.data() + begSampleInd, len);
				computeMfccVelocityAccel(samplesPart, frameSize, frameShift, framesCount2, mfccCount, mfccVecLen, filterBank, mfccFeaturesTmp);

				std::copy(mfccFeaturesTmp.begin(), mfccFeaturesTmp.end(), std::back_inserter(mfccFeatures));

				// dump extracted segments into wav files
				//std::stringstream ss;
				//ss << "dump_" << phoneName;
				//ss << "_" << folderOrWavFilePath.completeBaseName().toStdString();
				//ss << "_" << begSampleInd << "-" << endSampleInd;
				//ss << ".wav";
				//auto writeOp = writeAllSamplesWav(&audioSamples[begSampleInd], len, ss.str(), SampleRate);
			}
		}

		return std::make_pair(true, "");
	}

	// Gets the number of frames from a total number of features and number of MFCC features per frame.
	int featuresFramesCount(int featuresTotalCount, int mfccVecLen)
	{
		int framesCount = featuresTotalCount / mfccVecLen;
		PG_Assert(framesCount * mfccVecLen == featuresTotalCount && "There must be features for the whole number of frames");
		return framesCount;
	}

	std::tuple<bool, const char*> trainMonophoneClassifier(const std::map<std::string, std::vector<float>>& phoneNameToFeaturesVector, int mfccVecLen, int numClusters,
		std::map<std::string, std::unique_ptr<cv::EM>>& phoneNameToEMObj)
	{
		std::vector<double> mfccFeaturesDouble;

		// train GMMs

		for (const auto& pair : phoneNameToFeaturesVector)
		{
			const auto& phoneName = pair.first;

			const std::vector<float>& mfccFeatures = pair.second;

			// cv::EM requires features of double type
			mfccFeaturesDouble.resize(mfccFeatures.size());
			std::transform(std::begin(mfccFeatures), std::end(mfccFeatures), std::begin(mfccFeaturesDouble), [](float x) { return (double)x; });

			cv::Mat mfccFeaturesMat(mfccFeaturesDouble, false);

			int framesCount = featuresFramesCount(mfccFeaturesDouble.size(), mfccVecLen);
			mfccFeaturesMat = mfccFeaturesMat.reshape(1, framesCount);

			//
			int nclusters = numClusters;
			if (framesCount < nclusters)
				return std::make_tuple(false, "Not enough frames to train phone");

			auto termCrit = cv::TermCriteria(cv::TermCriteria::COUNT + cv::TermCriteria::EPS, 1000, FLT_EPSILON);
			std::unique_ptr<cv::EM> pEm = std::make_unique<cv::EM>(nclusters, cv::EM::COV_MAT_DIAGONAL, termCrit);
			bool trainOp = pEm->train(mfccFeaturesMat);
			if (!trainOp)
				return std::make_tuple(false, "Can't train the cv::EM for phone");

			phoneNameToEMObj.insert(std::make_pair(phoneName, std::move(pEm)));
		}

		std::make_tuple(true, "");
	}


// Applies first order derivative to signal.
// y[i] = y[i] - a * y[i-1]
void preEmphasisInplace(wv::slice<float> xs, float preEmph)
{
	if (xs.empty())
		return;

	for (size_t i = xs.size() - 1; i >= 1; --i)
	{
		xs[i] -= preEmph * xs[i - 1];
	}
	xs[0] *= 1 - preEmph;
}

float signalMagnitude(wv::slice<short> xs)
{
	if (xs.empty())
		return 0;
	float result = std::accumulate(std::begin(xs), std::end(xs), 0.0f, 
		[](float ax, short amplitude)
	{
		return ax + std::abs(amplitude);
	});
	result /= xs.size();
	return result;
}

// Attenuates the frame of a waveform using Hamming window.
// rename flattenFrameWithHammingWindow
void hammingInplace(wv::slice<float> frame)
{
	size_t len = frame.size();
	float step = 2 * M_PI / (len-1);

	// Hamming window at the first (i=0) and the last(i=len-1) point has value 0.08
	for (size_t i = 0; i < len; i++)
	{
		auto windowCoef = 0.54 - 0.46 * cos(step * i); // TODO: table lookup
		frame[i] *= windowCoef;
	}
}


float MelodyFun(float freqHz)
{
	float freqMel = 2595 * std::log10(1 + freqHz / 700);
	return freqMel;
}

float MelodyInvFun(float freqMel)
{
	float freqHz = 700 * (std::pow(10, freqMel / 2595) - 1);
	return freqHz;
}

// binsCount = number of bins in the filter-bank
void buildTriangularFilterBank(float sampleRate, int binsCount, int fftNum, TriangularFilterBank& filterBank)
{
	int dftHalf = fftNum / 2; // max freq = half of DFT points
	filterBank.BinCount = binsCount;
	filterBank.FftNum = fftNum;
	filterBank.fftFreqIndToHitInfo.resize(dftHalf);

	//float freqLow = mfccParams.SampleRate / fftNum;
	float freqLow = 0;
	float freqHigh = sampleRate / 2;

	float melLow = MelodyFun(freqLow);
	float melHigh = MelodyFun(freqHigh);

	int Q = binsCount;

	// for Q filters we need Q+2 points, as each triangular filter stands on 3 points
	// the first point for the first filter is melLow
	// the last point for the last filter is melHigh
	std::vector<float> binPointFreqsMel(Q + 2);
	linearSpace(melLow, melHigh, binPointFreqsMel.size(), binPointFreqsMel);

	// map back, evenly devided bin points in mel scale into logarithmic bin points in Hz scale
	std::vector<float> binPointFreqsHz(binPointFreqsMel.size());
	for (int i = 0; i < binPointFreqsHz.size(); ++i)
	{
		float freqMel = binPointFreqsMel[i];
		float freqHz = MelodyInvFun(freqMel);
		binPointFreqsHz[i] = freqHz;
	}

	// get FFT frequencies
	std::vector<float> fftFreqs(dftHalf);
	for (int i = 0; i < fftFreqs.size(); ++i)
	{
		int dftCoefInd = i + 1;
		float freq = dftCoefInd / (float)fftNum * sampleRate;
		fftFreqs[i] = freq;
	}

	// assign each Fft frequency to the filter-bank's bin
	// assert first FFT frequency < firstBin's left point

	int curBin = 0;
	// for i-th bin the coordinate of its center point is (i+1)
	float binCenterHz = binPointFreqsHz[1];
	for (int fftFreqInd = 0; fftFreqInd < fftFreqs.size();)
	{
		FilterHitInfo& filterHit = filterBank.fftFreqIndToHitInfo[fftFreqInd];

		float fftFreqHz = fftFreqs[fftFreqInd];

		// assign FFT frequencies which hit the current bin

		bool hitBin = fftFreqHz <= binCenterHz;
		if (!hitBin)
		{
			// the last DFT frequency may not exactly be equal to the last filter point
			// include the frequency into the filter coverage
			const static float eps = 0.1; // prevents round off float errors
			hitBin = fftFreqInd == fftFreqs.size() - 1 && fftFreqHz <= binCenterHz + eps;
		}
		if (hitBin)
		{
			float prevBinCenterHz = binPointFreqsHz[curBin];

			// FFT freq hits the bin
			if (curBin < Q)
			{
				filterHit.RightBinInd = curBin;

				// right bin response
				float responseRight = (fftFreqHz - prevBinCenterHz) / (binCenterHz - prevBinCenterHz);
				filterHit.RightFilterResponse = responseRight;
			}
			else
			{
				filterHit.RightFilterResponse = 0.0f;
				filterHit.RightBinInd = FilterHitInfo::NoBin;
			}

			// left bin response
			if (curBin > 0)
			{
				float responseLeft = (binCenterHz - fftFreqHz) / (binCenterHz - prevBinCenterHz);
				filterHit.LeftFilterResponse = responseLeft;

				filterHit.LeftBinInd = curBin - 1;
			}
			else
			{
				// frequencies in left shoulder of the first triangle has no left triangle
				filterHit.LeftBinInd = FilterHitInfo::NoBin;
				filterHit.LeftFilterResponse = 0.0f;
			}
			++fftFreqInd;
		}
		else
		{
			// shift to the next bin
			++curBin;

			assert(curBin + 1 < binPointFreqsHz.size() && "There are DFT frequencis greater then the last filter point");

			// for i-th bin the coordinate of its center point is (i+1)
			binCenterHz = binPointFreqsHz[curBin + 1];

			fftFreqInd += 0; // do not shift frequency, process it in the next bin
		}
	}

	// if(PG_DEBUG)
	for (int fftFreqInd = 0; fftFreqInd < fftFreqs.size(); ++fftFreqInd)
	{
		const FilterHitInfo& filterHit = filterBank.fftFreqIndToHitInfo[fftFreqInd];
		if (filterHit.LeftBinInd == FilterHitInfo::NoBin)
		{
			assert(filterHit.RightBinInd != FilterHitInfo::NoBin);
			assert(filterHit.LeftFilterResponse == 0);
		}
		if (filterHit.RightBinInd == FilterHitInfo::NoBin)
		{
			assert(filterHit.LeftBinInd != FilterHitInfo::NoBin);
			assert(filterHit.RightFilterResponse == 0);
		}

		if (filterHit.LeftBinInd != FilterHitInfo::NoBin && filterHit.RightBinInd != FilterHitInfo::NoBin)
		{
			// if freq hits two bins, then response from two bins must be unity
			float resp = filterHit.LeftFilterResponse + filterHit.RightFilterResponse;
			assert(std::abs(resp - 1) < 0.001);
		}
	}
}

int getMinDftPointsCount(int frameSize)
{
	int fftTwoPower = std::ceil(std::log2(frameSize));
	int fftNum = 1 << fftTwoPower; // number of FFT points
	return fftNum;
}

// Julius FFT implementation (mfcc-core.c).
/**
* Apply FFT
*
* @param xRe [i/o] real part of waveform
* @param xIm [i/o] imaginal part of waveform
* @param p [in] 2^p = FFT point
* @param w [i/o] MFCC calculation work area
*/
void FFT(float *xRe, float *xIm, int p)
{
	int i, ip, j, k, m, me, me1, n, nv2;
	double uRe, uIm, vRe, vIm, wRe, wIm, tRe, tIm;

	n = 1 << p;
	nv2 = n / 2;

	j = 0;
	for (i = 0; i < n - 1; i++){
		if (j > i){
			tRe = xRe[j];      tIm = xIm[j];
			xRe[j] = xRe[i];   xIm[j] = xIm[i];
			xRe[i] = tRe;      xIm[i] = tIm;
		}
		k = nv2;
		while (j >= k){
			j -= k;      k /= 2;
		}
		j += k;
	}

	for (m = 1; m <= p; m++){
		me = 1 << m;                me1 = me / 2;
		uRe = 1.0;                uIm = 0.0;
		wRe = cos(M_PI / me1);      wIm = -sin(M_PI / me1);
		for (j = 0; j < me1; j++){
			for (i = j; i < n; i += me){
				ip = i + me1;
				tRe = xRe[ip] * uRe - xIm[ip] * uIm;
				tIm = xRe[ip] * uIm + xIm[ip] * uRe;
				xRe[ip] = xRe[i] - tRe;   xIm[ip] = xIm[i] - tIm;
				xRe[i] += tRe;            xIm[i] += tIm;
			}
			vRe = uRe * wRe - uIm * wIm;   vIm = uRe * wIm + uIm * wRe;
			uRe = vRe;                     uIm = vIm;
		}
	}
}

// Computes robust signal derivative using sliding window.
// For windowHalf = 1 this function is just the difference of two neighbour elements divided by 2.
void rateOfChangeWindowed(const wv::slice<float> xs, int windowHalf, wv::slice<float> ys)
{
	assert(windowHalf >= 1 && "Radius of averaging must be positive");
	assert(ys.size() >= xs.size() && "Allocate space for the result");
	// assert xs and ys do not overlap

	float denom = 0;
	{
		for (int r = 1; r <= windowHalf; r++)
			denom += r * r;
		denom *= 2;
	}
	// denom = 0.5sum(r=1..windowHalf, r^2)
	float denom2 = windowHalf * (windowHalf + 1) * (2 * windowHalf + 1) / 3;
	assert(denom == denom2);

	//

	for (int time = 0; time < xs.size(); time++)
	{
		float change = 0;
		for (int r = 1; r <= windowHalf; r++)
		{
			// r=radius
			// replicate terminal element at the begin and end of the data
			float left  = time - r >= 0        ? xs[time - r] : xs[0];
			float right = time + r < xs.size() ? xs[time + r] : xs[xs.size()-1];
			
			change += r * (right - left);
		}
		change /= denom;

		ys[time] = change;
	}
}

float Cepstral0Coeff(const wv::slice<float> bankBinCoeffs)
{
	assert(!bankBinCoeffs.empty());

	float sum = std::accumulate(std::begin(bankBinCoeffs), std::end(bankBinCoeffs), 0);
	float sqrt2var = std::sqrt(2.0 / bankBinCoeffs.size());
	float result = sqrt2var * sum;
	return result;
}

void weightSignalInplace(const wv::slice<FilterHitInfo>& filterBank, wv::slice<float> xs)
{
}

// Computes MFCC (Mel-Frequency Cepstral Coefficients)
// pCepstral0, not null to compute zero cepstral coefficients.
void computeMfcc(wv::slice<float> samplesFloat, const TriangularFilterBank& filterBank, int mfccCount, wv::slice<float> mfcc, float* pCepstral0 = nullptr)
{
	assert(mfcc.size() >= mfccCount && "Not enough space for all MFCC");
	
	const float preEmph = 0.97;
	preEmphasisInplace(samplesFloat, preEmph);

	hammingInplace(samplesFloat);

	// compute DFT
	int fftNum = filterBank.FftNum;
	int dftHalf = fftNum / 2;
	const float DftPadValue = 0;
	std::vector<float> wavePointsRe(fftNum, DftPadValue);
	std::vector<float> wavePointsIm(fftNum, DftPadValue);
	std::copy_n(std::begin(samplesFloat), samplesFloat.size(), stdext::make_checked_array_iterator(wavePointsRe.data(), fftNum));
	FFT(wavePointsRe.data(), wavePointsIm.data(), std::log2(fftNum));

	// weight DFT with filter-bank responses
	int binCount = filterBank.BinCount;
	std::vector<float> bankBinCoeffs(binCount, 0);
	for (int fftFreqInt = 0; fftFreqInt < dftHalf; ++fftFreqInt)
	{
		float re = wavePointsRe[fftFreqInt];
		float im = wavePointsIm[fftFreqInt];
		float abs = std::sqrt(re*re + im*im);

		const FilterHitInfo& freqHitInfo = filterBank.fftFreqIndToHitInfo[fftFreqInt];
		int leftBin = freqHitInfo.LeftBinInd;
		if (leftBin != FilterHitInfo::NoBin)
		{
			float delta = freqHitInfo.LeftFilterResponse * abs;
			bankBinCoeffs[leftBin] += delta;
		}
		int rightBin = freqHitInfo.RightBinInd;
		if (rightBin != FilterHitInfo::NoBin)
		{
			float delta = freqHitInfo.RightFilterResponse * abs;
			bankBinCoeffs[rightBin] += delta;
		}
	}

	// make ln of it

	for (size_t i = 0; i < bankBinCoeffs.size(); ++i)
	{
		bankBinCoeffs[i] = std::max(std::log(bankBinCoeffs[i]), 0.0f);
	}

	//

	*pCepstral0 = Cepstral0Coeff(bankBinCoeffs);

	// apply Discrete Cosine Transform (DCT) to the filter bank

	float sqrt2var = std::sqrt(2.0 / binCount);
	for (int mfccIt= 1; mfccIt <= mfccCount; mfccIt++)
	{
		float ax = 0.0;
		for (int binIt = 1; binIt <= binCount; binIt++)
			ax += bankBinCoeffs[binIt - 1] * std::cos(mfccIt * (binIt - 0.5) * (M_PI / binCount));
		ax *= sqrt2var;

		mfcc[mfccIt-1] = ax;
	}

	// do not perform Julius 'lifting' of cepstral coefficients
}

int slidingWindowsCount(long framesCount, int windowSize, int windowShift)
{
	// no windows fit frames
	if (framesCount < windowSize)
		return 0;

	int wndCount = 1 + (framesCount - windowSize) / windowShift;
	assert((wndCount - 1) * windowShift + windowSize <= framesCount && "The must be frames for each sliding window");
	return wndCount;
}

void slidingWindows(long startFrameInd, long framesCount, int windowSize, int windowShift, wv::slice<TwoFrameInds> windowBounds)
{
	int wndCount = slidingWindowsCount(framesCount, windowSize, windowShift);
	assert(wndCount == windowBounds.size() && "Preallocate space for windows bounds");
	if (wndCount >= 1)
		assert((wndCount - 1) * windowShift + windowSize <= framesCount && "The must be frames for each sliding window");

	for (int i = 0; i < wndCount; ++i)
	{
		TwoFrameInds& bnds = windowBounds[i];
		bnds.Start = startFrameInd;
		bnds.Count = windowSize;
		
		startFrameInd += windowShift;
	}
}

void computeMfccVelocityAccel(const wv::slice<short> samples, int frameSize, int frameShift, int framesCount, int mfcc_dim, int mfccVecLen, const TriangularFilterBank& filterBank, wv::slice<float> mfccFeatures)
{
	std::vector<float> samplesFloat(frameSize);

	// for each frames there are mfccVecLen MFCCs:
	// staticCoefCount static coefficients +
	// staticCoefCount velocity coefficients +
	// staticCoefCount acceleration coefficients

	int staticCoefCount = mfcc_dim + 1; // +1 for zero cepstral coeff
	for (int frameInd = 0; frameInd < framesCount; ++frameInd)
	{
		const short* sampleBeg = samples.data() + frameInd * frameShift;
		auto sampleBegIt = stdext::make_checked_array_iterator(sampleBeg, frameSize);
		std::copy_n(sampleBegIt, frameSize, std::begin(samplesFloat));

		float* mfccPerFrameBeg = mfccFeatures.data() + frameInd * mfccVecLen;
		wv::slice<float> mfccPerFrame = wv::make_view(mfccPerFrameBeg, staticCoefCount);
		float cepstral0 = -1;
		computeMfcc(samplesFloat, filterBank, mfcc_dim, mfccPerFrame, &cepstral0);

		// last coef is c0
		mfccPerFrame[mfcc_dim + 0] = cepstral0;
	}

	// finds velocity and acceleration for each static coef

	std::vector<float> coefBufInTime(framesCount * 3); // *3 for static+velocity+acceleration
	wv::slice<float> staticInTime = wv::make_view(coefBufInTime.data(), framesCount);
	wv::slice<float> velocInTime = wv::make_view(coefBufInTime.data() + framesCount * 1, framesCount);
	wv::slice<float> accelInTime = wv::make_view(coefBufInTime.data() + framesCount * 2, framesCount);

	for (int staticInd = 0; staticInd < staticCoefCount; ++staticInd)
	{
		// compute delta (velocity) coefficients
		for (int time = 0; time < framesCount; ++time)
		{
			wv::slice<float> mfccPerFrame = wv::make_view(mfccFeatures.data() + time * mfccVecLen, mfccVecLen);
			staticInTime[time] = mfccPerFrame[staticInd];
		}

		// now: first third of the buffer has static coef across all time

		// compute velocity
		const int windowHalf = 2;
		rateOfChangeWindowed(staticInTime, windowHalf, velocInTime);

		// compute acceleration
		rateOfChangeWindowed(velocInTime, windowHalf, accelInTime);

		// populate back mfcc with velocity (delta) and acceleration (delta-delta) coefs
		for (int time = 0; time < framesCount; ++time)
		{
			wv::slice<float> mfccPerFrame = wv::make_view(mfccFeatures.data() + time * mfccVecLen, mfccVecLen);

			mfccPerFrame[1 * staticCoefCount + staticInd] = velocInTime[time];
			mfccPerFrame[2 * staticCoefCount + staticInd] = accelInTime[time];
		}
	}
}

}

