#include <regex>
#include <memory>
#include <cctype> // std::isalpha
#include <numeric> // std::accumulate

#include <QDir>
#include <QDirIterator>

#include "SpeechProcessing.h"
#include "XmlAudioMarkup.h"
#include "WavUtils.h"
#include "JuliusToolNativeWrapper.h"
#include "InteropPython.h"
#include "PhoneticService.h"
#include "InteropPython.h" // phoneNameToPhoneId
#include "assertImpl.h"
#include "ComponentsInfrastructure.h"
#include <boost/lexical_cast/try_lexical_convert.hpp>
#include <boost/format.hpp>

namespace PticaGovorun 
{
	ptrdiff_t AnnotatedAudioSegment::durationSamples() const
	{
		PG_DbgAssert(EndSampleInd != -1);
		PG_DbgAssert(StartSampleInd != -1);
		return EndSampleInd - StartSampleInd;
	}

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

	boost::string_view toString(ResourceUsagePhase phase)
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

	boost::optional<ResourceUsagePhase> resourceUsagePhaseFromString(boost::string_view phase)
	{
		if (phase.compare("train") == 0)
			return ResourceUsagePhase::Train;
		if (phase.compare("test") == 0)
			return ResourceUsagePhase::Test;
		return boost::none;
	}

	bool includeInTrainOrTest(const TimePointMarker& marker)
	{
		if (marker.TranscripText.contains(QChar('#')))
			return false;
		return true;
	}

	namespace
	{
		bool tryParsePronuncDescriptor(boost::wstring_view word, GrowOnlyPinArena<wchar_t>& arena, boost::wstring_view& arenaWord)
		{
			// eg "{w:slova,s:2}"

			if (!word.starts_with(L'{'))
				return false;
			auto closePos = word.find(L'}');
			if (closePos == boost::wstring_view::npos)
				return false;

			//
			boost::wstring_view pronCodeStr;
			boost::wstring_view stressStr;
			for (auto pos = 1; pos<closePos; )
			{
				auto pairEnd = word.find(L',', pos);
				if (pairEnd == boost::wstring_view::npos)
					pairEnd = closePos;

				auto pair = word.substr(pos, pairEnd - pos);
				if (pair.empty())
					break;
				
				auto colonPos = pair.find(L':');
				if (colonPos == boost::wstring_view::npos)
					return false;

				auto keyWord = trim(pair.substr(0, colonPos));
				auto valWord = trim(pair.substr(colonPos+1));
				if (keyWord == L"w")
					pronCodeStr = valWord;
				else if (keyWord == L"s")
					stressStr = valWord;
				pos = pairEnd + 1;
			}
			if (pronCodeStr.empty() || stressStr.empty())
				return false;

			std::wstring buf;
			buf.append(pronCodeStr.data(), pronCodeStr.size());
			buf.push_back(L'(');
			buf.append(stressStr.data(), stressStr.size());
			buf.push_back(L')');

			arenaWord = registerWordThrow2<wchar_t>(arena, buf);
			return true;
		}
	}

	void splitUtteranceIntoPronuncList(boost::wstring_view text, GrowOnlyPinArena<wchar_t>& arena, std::vector<boost::wstring_view>& words)
	{
		// clothes clothes(1)
		// apostrophe (eg: п'ять)
		// dash (eg: to-do)
		// angle bracket, slash <s> <sil> </sil>
		// square bracket [sp] [inh]
		// leading pound marks 'exclude from build' and ignored "#one two"
		
		size_t pos = 0;

		// ignore leading pound
		if (!text.empty() && text[0] == L'#')
			pos += 1;

		for (size_t wordEnd = pos; wordEnd < text.size(); )
		{
			size_t sepPos = text.find(L' ', pos);

			wordEnd = sepPos;
			if (sepPos == boost::wstring_view::npos)
				wordEnd = text.size(); // for last word

			size_t len = wordEnd - pos;
			if (len > 0)
			{
				auto word = text.substr(pos, len);
				boost::wstring_view arenaWord;
				if (tryParsePronuncDescriptor(word, arena, arenaWord))
					words.push_back(arenaWord);
				else
					words.push_back(word);
			}

			pos = wordEnd + 1;
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

		GrowOnlyPinArena<wchar_t> arena(1024);
		std::vector<boost::wstring_view> words;
		splitUtteranceIntoPronuncList(text, arena, words);

		// iterate through words
		for (boost::wstring_view wordRef : words)
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

	void padSilence(gsl::span<short> audioSamples, int silenceSamplesCount, std::vector<short>& paddedAudio)
	{
		PG_Assert(silenceSamplesCount >= 0);

		paddedAudio.resize(audioSamples.size() + 2 * silenceSamplesCount);

		static const auto SampleSize = sizeof(short);
		
		// silence before the audio
		std::memset(paddedAudio.data(), 0, silenceSamplesCount * SampleSize);

		// audio
		std::memcpy(paddedAudio.data() + silenceSamplesCount, audioSamples.data(), audioSamples.size() * SampleSize);

		// silence after the audio
		std::memset(paddedAudio.data() + silenceSamplesCount + audioSamples.size(), 0, silenceSamplesCount * SampleSize);
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
			PG_Assert2(false, "Unrecognized value of LastFrameSample enum");
		}
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

	bool loadAudioAnnotation(const boost::filesystem::path& annotFilePathAbs, AudioFileAnnotation& annot, ErrMsgList* errMsg)
	{
		SpeechAnnotation speechAnnot;
		std::tuple<bool, const char*> loadOp = loadAudioMarkupFromXml(annotFilePathAbs.wstring(), speechAnnot);
		if (!std::get<0>(loadOp))
		{
			if (errMsg != nullptr)
				errMsg->utf8Msg = std::get<1>(loadOp);
			return false;
		}

		//
		annot.AnnotFilePath = annotFilePathAbs;
		annot.AudioFilePath = annotFilePathAbs.parent_path() / speechAnnot.audioFilePathRel();
		annot.SampleRate = speechAnnot.audioSampleRate();

		std::vector<const TimePointMarker*> wordMarkers;
		for (size_t i = 0; i < speechAnnot.markersSize(); ++i)
		{
			const auto& marker = speechAnnot.marker(i);
			if (marker.LevelOfDetail == MarkerLevelOfDetail::Word)
				wordMarkers.push_back(&marker);
		}

		const TimePointMarker* prevMarker = nullptr;
		const TimePointMarker* nextMarker = nullptr;
		AnnotatedAudioSegment* prevSeg = nullptr;
		for (size_t i=0; i<wordMarkers.size(); ++i, prevMarker = nextMarker)
		{
			nextMarker = wordMarkers[i];
			if (prevMarker == nullptr ||
				prevMarker->TranscripText.isEmpty() ||
				!includeInTrainOrTest(*prevMarker))
			{
				prevSeg = nullptr;
				continue;
			}

			auto seg = std::make_unique<AnnotatedAudioSegment>();
			seg->FileContainer = &annot;
			seg->TranscriptText = toUtf8StdString(prevMarker->TranscripText);
			seg->TranscriptTextDebug = prevMarker->TranscripText.toStdWString();
			seg->Language = prevMarker->Language;
			seg->SpeakerBriefId = toUtf8StdString(prevMarker->SpeakerBriefId);
			seg->StartMarkerId = prevMarker->Id;
			seg->EndMarkerId = nextMarker->Id;
			seg->StartSampleInd = prevMarker->SampleInd;
			seg->EndSampleInd = nextMarker->SampleInd;

			if (prevSeg != nullptr)
				prevSeg->NextSegment = seg.get();
			seg->PrevSegment = prevSeg;

			prevSeg = seg.get();

			annot.Segments.push_back(std::move(seg));
		}

#if PG_DEBUG
		for (const auto& seg : annot.Segments)
		{
			if (seg->PrevSegment != nullptr)
			{
				PG_Assert(seg->PrevSegment->NextSegment == seg.get());
				PG_Assert(seg->PrevSegment->EndSampleInd == seg->StartSampleInd);
			}
		}
		PG_Assert(annot.Segments.back()->NextSegment == nullptr);
#endif
		return true;
	}

	bool loadAudioAnnotation(const boost::filesystem::path& annotDirOrPathAbs, std::vector<std::unique_ptr<AudioFileAnnotation>>& audioAnnots, ErrMsgList* errMsg)
	{
		auto processFile = [&](const auto& filePathAbs) -> bool
		{
			auto annot = std::make_unique<AudioFileAnnotation>();
			if (!loadAudioAnnotation(filePathAbs, *annot, errMsg))
				return false;

			audioAnnots.push_back(std::move(annot));
			return true;
		};
		if (boost::filesystem::is_directory(annotDirOrPathAbs))
		{
			QDirIterator fileIter(toQString(annotDirOrPathAbs.wstring()), QStringList() << "*.xml", QDir::Files, QDirIterator::Subdirectories);
			for (; fileIter.hasNext(); )
			{
				auto annotFilePath = fileIter.next();
				if (!processFile(annotFilePath.toStdWString()))
					return false;
			}
		}
		else
		{
			if (!processFile(annotDirOrPathAbs))
				return false;
		}
		return true;
	}

	bool loadSpeechAndAnnotation(const QFileInfo& folderOrWavFilePath, const std::wstring& wavRootDir, const std::wstring& annotRootDir, 
		MarkerLevelOfDetail targetLevelOfDetail, bool loadAudio, bool removeSilenceAnnot, bool padSilStart, bool padSilEnd, float maxNoiseLevelDb,
		std::function<auto(const AnnotatedSpeechSegment& seg)->bool> segPredBefore, std::vector<AnnotatedSpeechSegment>& segments, ErrMsgList* errMsg)
	{
		if (folderOrWavFilePath.isDir())
		{
			QString dirAbsPath = folderOrWavFilePath.absoluteFilePath();
			QDir dir(dirAbsPath);

			QFileInfoList items = dir.entryInfoList(QDir::Filter::Files | QDir::Filter::Dirs | QDir::Filter::NoDotAndDotDot);
			for (const QFileInfo item : items)
			{
				if (!loadSpeechAndAnnotation(item, wavRootDir, annotRootDir, targetLevelOfDetail, loadAudio, removeSilenceAnnot, padSilStart, padSilEnd, maxNoiseLevelDb, segPredBefore, segments, errMsg))
					return false;
			}
			return true;
		}

		// process wav file path

		QString audioFilePath = folderOrWavFilePath.absoluteFilePath();

		// skip non audio files
		if (!isSupportedAudioFile(audioFilePath.toStdWString().c_str()))
			return true;
			
		QString annotFilePath = QString::fromStdWString(speechAnnotationFilePathAbs(folderOrWavFilePath.absoluteFilePath().toStdWString(), wavRootDir, annotRootDir));
		QFileInfo xmlFilePathInfo(annotFilePath);
		if (!xmlFilePathInfo.exists()) // wav has no corresponding markup
			return true;

		// load audio markup
		// not all wav files has a markup, hence try to load the markup first
		QString audioMarkupFilePathAbs = xmlFilePathInfo.absoluteFilePath();
		SpeechAnnotation speechAnnot;
		if (!loadAudioMarkupXml(toBfs(audioMarkupFilePathAbs), speechAnnot, errMsg))
		{
			pushErrorMsg(errMsg, str(boost::format("Can't load audio annotation from file (%1%)") % toUtf8StdString(audioMarkupFilePathAbs)));
			return false;
		}

		// skip audio files with a lot of noise
		if (maxNoiseLevelDb != 0)
		{
			const SpeechAnnotationParameter* param = speechAnnot.getParameter("NoiseLevelDb");
			if (param != nullptr)
			{
				float noiseLevelDb;
				if (!boost::conversion::try_lexical_convert(param->Value, noiseLevelDb))
				{
					pushErrorMsg(errMsg, str(boost::format("Can't convert str->float for parameter NoiseLevelDb in file (%1%)") % toUtf8StdString(audioMarkupFilePathAbs)));
					return false;
				}

				if (noiseLevelDb > maxNoiseLevelDb) // current file is too noisy and a user requested to ignore it
				{
					std::wcout << L"File is ignored due to noise (" << toString(audioMarkupFilePathAbs) << ")\n";
					return true;
				}
			}
		}

		const std::vector<TimePointMarker>& syncPoints = speechAnnot.markers();

		struct TimePointMarkerAndItsInd
		{
			const TimePointMarker& marker;
			size_t markerInd; // index of the marker in the collection of markers in speech annotation
		};
		// take only markers of requested type
		std::vector<TimePointMarkerAndItsInd> markersOfInterest;
		markersOfInterest.reserve(speechAnnot.markersSize());
		for (size_t i=0; i<speechAnnot.markersSize(); ++i)
		{
			const auto& marker = speechAnnot.marker(i);
			if (marker.LevelOfDetail == targetLevelOfDetail)
			{
				markersOfInterest.push_back({ marker, i });
			}
		}

		// determine the segment's bounds
		// if a text segment is flanked with the silence segment then the text segment is aumented with silence
		struct SegmentBlank
		{
			const TimePointMarker* ContentMarker = nullptr; // the marker with text

			// start/end markers may be different from content marker, if text segment is flanked with silence to the start/end
			const TimePointMarker* StartMarker = nullptr;
			const TimePointMarker* EndMarker = nullptr;
			size_t StartMarkerInd = (size_t)-1;
			size_t EndMarkerInd = (size_t)-1;
			bool HasStartSilence = false;
			bool HasEndSilence = false;
			long StartSilenceSamplesCount = 0;
			long EndSilenceSamplesCount = 0;
		};
		ShortPauseUsage shortPauseUsage = ShortPauseUsage::UseAsSilence;
		QString fillerSilQ = toQString(fillerSilence());
		QString fillerSpQ = toQString(fillerShortPause());
		QString fillerSpaceQ = toQString(fillerSingleSpace());
		QString keywordIgnoreQ = utf8ToQString(keywordIgnore());
		std::vector<SegmentBlank> blankSegs;
		for (int i = 0; i < (int)markersOfInterest.size() - 1; ++i)
		{
			const auto& markerInfo = markersOfInterest[i];

			if (markerInfo.marker.TranscripText.isEmpty()) // ignore empty segments
				continue;
			if (!includeInTrainOrTest(markerInfo.marker))
				continue;
			if (markerInfo.marker.TranscripText == fillerSilQ) // skip silence segments, attach it to neighbour text segments
				continue;
			if (markerInfo.marker.TranscripText == fillerSpQ) // later [sp] is ignored or used as silence
				continue;

			const auto& endMarkerInfo = markersOfInterest[i + 1];

			SegmentBlank blankSeg;
			blankSeg.ContentMarker = &markerInfo.marker;
			blankSeg.StartMarker = &markerInfo.marker;
			blankSeg.StartMarkerInd = markerInfo.markerInd;
			blankSeg.EndMarker = &endMarkerInfo.marker;
			blankSeg.EndMarkerInd = endMarkerInfo.markerInd;

			if (padSilStart && i > 0)
			{
				const auto& prevMarkerInfo = markersOfInterest[i - 1];
				if (prevMarkerInfo.marker.TranscripText == fillerSilQ ||
					prevMarkerInfo.marker.TranscripText == fillerSpQ && shortPauseUsage == ShortPauseUsage::UseAsSilence)
				{
					blankSeg.StartMarker = &prevMarkerInfo.marker;
					blankSeg.HasStartSilence = true;
					blankSeg.StartSilenceSamplesCount = markerInfo.marker.SampleInd - prevMarkerInfo.marker.SampleInd;
				}
			}
			if (padSilEnd &&
				i + 2 < markersOfInterest.size() && (
				markersOfInterest[i + 1].marker.TranscripText == fillerSilQ ||
				markersOfInterest[i + 1].marker.TranscripText == fillerSpQ && shortPauseUsage == ShortPauseUsage::UseAsSilence
				))
			{
				const auto& nextNextMarkerInfo = markersOfInterest[i + 2]; // the end of next silence segment
				blankSeg.EndMarker = &nextNextMarkerInfo.marker;
				blankSeg.HasEndSilence = true;
				blankSeg.EndSilenceSamplesCount = nextNextMarkerInfo.marker.SampleInd - markersOfInterest[i+1].marker.SampleInd;
			}
			blankSegs.push_back(blankSeg);
		}

		// audio frames are lazy loaded
		float sampleRateAnnot = speechAnnot.audioSampleRate();
		std::vector<short> audioSamples;

		// remove [sp]
		std::vector<boost::wstring_view> words;
		std::vector<boost::wstring_view> wordsNoSp;

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
			seg.AnnotFilePath = annotFilePath.toStdWString();
			seg.AudioFilePath = audioFilePath.toStdWString();
			seg.AudioStartsWithSilence = blankSeg.HasStartSilence;
			seg.AudioEndsWithSilence = blankSeg.HasEndSilence;
			seg.StartSilenceFramesCount = blankSeg.StartSilenceSamplesCount;
			seg.EndSilenceFramesCount = blankSeg.EndSilenceSamplesCount;
			seg.SampleRate = sampleRateAnnot;
			QString txt = blankSeg.ContentMarker->TranscripText;

			// modify marker's annotation
			{
				std::wstring txtW = txt.toStdWString();
				words.clear();
				wordsNoSp.clear();
				GrowOnlyPinArena<wchar_t> arena(1024);
				splitUtteranceIntoPronuncList(txtW, arena, words);

				bool skipShortPause = true;
				for (size_t i = 0; i < words.size(); ++i)
				{
					auto word = words[i];
					if (removeSilenceAnnot)
					{
						// [sp] -> [], <sil> -> [], _s -> []
						if (word == fillerShortPause() ||
							word == fillerSilence() ||
							word == fillerSingleSpace())
							continue;
					}
					else if (skipShortPause)
					{
						// [sp] -> <sil>, _s -> <sil>
						if (word == fillerShortPause() || word == fillerSingleSpace())
						{
							wordsNoSp.push_back(fillerSilence());
							continue;
						}
					}
					wordsNoSp.push_back(word);
				}

				seg.TranscriptionStartsWithSilence = wordsNoSp.front() == fillerStartSilence();
				seg.TranscriptionEndsWithSilence = wordsNoSp.back() == fillerEndSilence();

				std::wostringstream buf;
				join(wordsNoSp.begin(), wordsNoSp.end(), boost::wstring_view(L" "), buf);

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
				if (audioSamples.empty())
				{
					// load wav file
					float sampleRateAudio;
					if (!readAllSamplesFormatAware(audioFilePath.toStdWString(), audioSamples, &sampleRateAudio, errMsg))
					{
						pushErrorMsg(errMsg, std::string("Can't read wav file: ") + audioFilePath.toUtf8().toStdString());
						return false;
					}
					if (sampleRateAnnot != sampleRateAudio)
					{
						errMsg->utf8Msg = std::string("SampleRate mismatch in audio and annotation for file: ") + audioFilePath.toUtf8().toStdString();
						return false;
					}
				}

				std::vector<short> segSamples;
				segSamples.reserve(blankSeg.EndMarker->SampleInd - blankSeg.StartMarker->SampleInd);

				auto pushSamples = [&segSamples,&audioSamples](ptrdiff_t startSampInd, ptrdiff_t endSampInd)
				{
					std::copy(&audioSamples[startSampInd], &audioSamples[endSampInd], std::back_inserter(segSamples));
				};

				// inter-speech silence's segment consists of two phone markers
				// the first marker has text='_s', the second marker has text='' (empty string).
				// these regions of silence may be detected using VAD (Voice Activity Detetion)
				bool removeInterSpeechSilence = removeSilenceAnnot;
				for (size_t ind = blankSeg.StartMarkerInd; ind < speechAnnot.markersSize()-1; ++ind)
				{
					const auto& m1 = speechAnnot.marker(ind);
					const auto& m2 = speechAnnot.marker(ind + 1);

					if (removeInterSpeechSilence &&
						m1.LevelOfDetail == MarkerLevelOfDetail::Phone &&
						m2.LevelOfDetail == MarkerLevelOfDetail::Phone &&
						m1.TranscripText == fillerSpaceQ)
					{
						// skip inter-speech silence
						continue;
					}
					if (m1.LevelOfDetail == MarkerLevelOfDetail::Phone &&
						m2.LevelOfDetail == MarkerLevelOfDetail::Phone &&
						m1.TranscripText == keywordIgnoreQ)
					{
						// ignore portion of audio due to clicks, noise etc.
						continue;
					}

					pushSamples(m1.SampleInd, m2.SampleInd);

					if (m2.Id == blankSeg.EndMarker->Id) // reached the end of a segment
						break;
				}
				seg.Samples = std::move(segSamples);
			}
			segments.push_back(seg);
		}

		return true;
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

		for (int markerInd = 0; markerInd < (int)markersOfInterest.size() - 1; ++markerInd)
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
	bool collectMfccFeatures(const QFileInfo& folderOrWavFilePath, int frameSize, int frameShift, int mfccVecLen, std::map<std::string, std::vector<float>>& phoneNameToFeaturesVector, std::wstring* errMsg)
	{
		if (folderOrWavFilePath.isDir())
		{
			QString dirAbsPath = folderOrWavFilePath.absoluteFilePath();
			QDir dir(dirAbsPath);
			
			QFileInfoList items = dir.entryInfoList(QDir::Filter::Files | QDir::Filter::Dirs | QDir::Filter::NoDotAndDotDot);
			for (const QFileInfo item : items)
			{
				if (!collectMfccFeatures(item, frameSize, frameShift, mfccVecLen, phoneNameToFeaturesVector, errMsg))
					return false; 
			}
			return true;
		}

		// process wav file path

		QString wavFilePath = folderOrWavFilePath.absoluteFilePath();

		QString extension = folderOrWavFilePath.suffix();
		if (!isSupportedAudioFile(wavFilePath.toStdWString().c_str())) // skip non wav files
			return true;
		
		QDir parentDir = folderOrWavFilePath.absoluteDir();
		QString xmlFileName = folderOrWavFilePath.completeBaseName() + ".xml";

		QString xmlFilePath = parentDir.absoluteFilePath(xmlFileName);
		QFileInfo xmlFilePathInfo(xmlFilePath);
		if (!xmlFilePathInfo.exists()) // wav has no corresponding markup
			return true;

		// load audio markup
		// not all wav files has a markup, hence try to load the markup first

		QString audioMarkupFilePathAbs = xmlFilePathInfo.absoluteFilePath();
		SpeechAnnotation speechAnnot;
		std::tuple<bool, const char*> loadOp = loadAudioMarkupFromXml(audioMarkupFilePathAbs.toStdWString(), speechAnnot);
		if (!std::get<0>(loadOp))
		{
			*errMsg = QString::fromLatin1(std::get<1>(loadOp)).toStdWString();
			return false;
		}

		const std::vector<TimePointMarker>& syncPoints = speechAnnot.markers();

		// load wav file
		std::vector<short> audioSamples;
		float sampleRate = -1;
		ErrMsgList errMsgL;
		if (!readAllSamplesFormatAware(wavFilePath.toStdWString(), audioSamples, &sampleRate, &errMsgL))
		{
			if (errMsg != nullptr)
			{
				*errMsg = QString("Can't read wav file. %1").arg(combineErrorMessages(errMsgL)).toStdWString();
			}
			return false;
		}

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
				//ss << "_" << folderOrWavFilePath.completeBaseName().toUtf8StdString();
				//ss << "_" << begSampleInd << "-" << endSampleInd;
				//ss << ".wav";
				//auto writeOp = writeAllSamplesWav(&audioSamples[begSampleInd], len, ss.str(), SampleRate);
			}
		}

		return true;
	}

	// Gets the number of frames from a total number of features and number of MFCC features per frame.
	int featuresFramesCount(int featuresTotalCount, int mfccVecLen)
	{
		int framesCount = featuresTotalCount / mfccVecLen;
		PG_Assert2(framesCount * mfccVecLen == featuresTotalCount, "There must be features for the whole number of frames");
		return framesCount;
	}

#if PG_HAS_OPENCV
	std::tuple<bool, const char*> trainMonophoneClassifier(const std::map<std::string, std::vector<float>>& phoneNameToFeaturesVector, int mfccVecLen, int numClusters,
		std::map<std::string, cv::Ptr<cv::ml::EM>>& phoneNameToEMObj)
	{
		std::vector<float> mfccFeaturesFloat;

		// train GMMs

		int responsePhoneId = 1;
		for (const auto& pair : phoneNameToFeaturesVector)
		{
			const auto& phoneName = pair.first;

			const std::vector<float>& mfccFeatures = pair.second;

			// cv::EM requires features of double type
			mfccFeaturesFloat.resize(mfccFeatures.size());
			std::transform(std::begin(mfccFeatures), std::end(mfccFeatures), std::begin(mfccFeaturesFloat), [](float x) { return (float)x; });

			cv::Mat mfccFeaturesMat(mfccFeaturesFloat, false);

			int framesCount = featuresFramesCount(mfccFeaturesFloat.size(), mfccVecLen);
			mfccFeaturesMat = mfccFeaturesMat.reshape(1, framesCount);

			//
			int nclusters = numClusters;
			if (framesCount < nclusters)
				return std::make_tuple(false, "Not enough frames to train phone");

			auto termCrit = cv::TermCriteria(cv::TermCriteria::COUNT + cv::TermCriteria::EPS, 1000, FLT_EPSILON);

			cv::Ptr<cv::ml::EM> pEm = cv::ml::EM::create();
			pEm->setClustersNumber(nclusters);
			pEm->setTermCriteria(termCrit);
			pEm->setCovarianceMatrixType(cv::ml::EM::COV_MAT_DIAGONAL);

			std::vector<float> responses(framesCount, responsePhoneId); // TODO: check test; construct correct responseMat for training
			auto trainData = cv::ml::TrainData::create(mfccFeaturesMat, cv::ml::SampleTypes::COL_SAMPLE, responses);
			bool trainOp = pEm->train(trainData);
			if (!trainOp)
				return std::make_tuple(false, "Can't train the cv::EM for phone");

			phoneNameToEMObj.insert(std::make_pair(phoneName, pEm));
			responsePhoneId++;
		}

		std::make_tuple(true, "");
	}
#endif // PG_HAS_OPENCV


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

			PG_DbgAssert2(curBin + 1 < binPointFreqsHz.size(), "There are DFT frequencis greater then the last filter point");

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
			PG_DbgAssert(filterHit.RightBinInd != FilterHitInfo::NoBin);
			PG_DbgAssert(filterHit.LeftFilterResponse == 0);
		}
		if (filterHit.RightBinInd == FilterHitInfo::NoBin)
		{
			PG_DbgAssert(filterHit.LeftBinInd != FilterHitInfo::NoBin);
			PG_DbgAssert(filterHit.RightFilterResponse == 0);
		}

		if (filterHit.LeftBinInd != FilterHitInfo::NoBin && filterHit.RightBinInd != FilterHitInfo::NoBin)
		{
			// if freq hits two bins, then response from two bins must be unity
			float resp = filterHit.LeftFilterResponse + filterHit.RightFilterResponse;
			PG_DbgAssert(std::abs(resp - 1) < 0.001);
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
	PG_DbgAssert2(windowHalf >= 1, "Radius of averaging must be positive");
	PG_DbgAssert2(ys.size() >= xs.size(), "Allocate space for the result");
	// assert xs and ys do not overlap

	float denom = 0;
	{
		for (int r = 1; r <= windowHalf; r++)
			denom += r * r;
		denom *= 2;
	}
	// denom = 0.5sum(r=1..windowHalf, r^2)
	float denom2 = windowHalf * (windowHalf + 1) * (2 * windowHalf + 1) / 3;
	PG_DbgAssert(denom == denom2);

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
	PG_Assert(!bankBinCoeffs.empty());

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
	PG_Assert(mfcc.size() >= mfccCount && "Not enough space for all MFCC");
	
	const float preEmph = 0.97;
	preEmphasisInplace(samplesFloat, preEmph);

	hammingInplace(samplesFloat);

	// compute DFT
	int fftNum = filterBank.FftNum;
	int dftHalf = fftNum / 2;
	const float DftPadValue = 0;
	std::vector<float> wavePointsRe(fftNum, DftPadValue);
	std::vector<float> wavePointsIm(fftNum, DftPadValue);

    PG_DbgAssert(samplesFloat.size() <= wavePointsRe.size());
	std::copy_n(std::begin(samplesFloat), samplesFloat.size(), wavePointsRe.data());

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
	PG_DbgAssert2((wndCount - 1) * windowShift + windowSize <= framesCount, "The must be frames for each sliding window");
	return wndCount;
}

void slidingWindows(long startFrameInd, long framesCount, int windowSize, int windowShift, wv::slice<TwoFrameInds> windowBounds)
{
	int wndCount = slidingWindowsCount(framesCount, windowSize, windowShift);
	PG_Assert(wndCount == windowBounds.size() && "Preallocate space for windows bounds");
	if (wndCount >= 1)
		PG_Assert((wndCount - 1) * windowShift + windowSize <= framesCount && "The must be frames for each sliding window");

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
		gsl::span<const short> sampleBeg(samples.data() + frameInd * frameShift, frameSize);
        PG_DbgAssert(sampleBeg.size() <= samplesFloat.size());
		std::copy_n(sampleBeg.data(), frameSize, std::begin(samplesFloat));

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

