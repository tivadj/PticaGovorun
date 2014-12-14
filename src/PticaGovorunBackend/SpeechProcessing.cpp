#include "stdafx.h"
#include "SpeechProcessing.h"
#include <regex>
#include <memory>
#include <cctype> // std::isalpha
#include <QDir>

#include <opencv2\core.hpp> // cv::Mat
#include <opencv2\ml.hpp> // cv::EM

#include "XmlAudioMarkup.h"
#include "WavUtils.h"
#include "MLUtils.h"
#include "JuliusToolNativeWrapper.h"

namespace PticaGovorun {

	bool getDefaultMarkerStopsPlayback(MarkerLevelOfDetail levelOfDetail)
	{
		if (levelOfDetail == MarkerLevelOfDetail::Word)
			return true;

		// do not stop on phone markers
		return true;
	}

	std::tuple<bool, std::wstring> convertTextToPhoneList(const std::wstring& text, std::function<auto (const std::wstring&, std::vector<std::string>&) -> void> wordToPhoneListFun, bool insertShortPause, std::vector<std::string>& speechPhones)
{
	std::wsmatch matchRes;
	static std::wregex r(LR"regex(\w+)regex"); // match words

	// iterate through words

	std::wstring word;
	word.reserve(32);

	std::vector<std::string> wordPhones;
	wordPhones.reserve(word.capacity());

	// insert the starting silence phone
	speechPhones.push_back(PGPhoneSilence);

	auto wordBeg = std::cbegin(text);
	while (std::regex_search(wordBeg, std::cend(text), matchRes, r))
	{
		auto& wordSlice = matchRes[0];
		word.assign(wordSlice.first, wordSlice.second);

		// convert word to phones
		wordPhones.clear();
		wordToPhoneListFun(word, wordPhones);

		std::copy(std::begin(wordPhones), std::end(wordPhones), std::back_inserter(speechPhones));

		// insert the silence phone between words
		if (insertShortPause)
			speechPhones.push_back(PGShortPause);

		wordBeg = wordSlice.second;
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
		std::vector<TimePointMarker> syncPoints;
		std::tuple<bool, const char*> loadOp = loadAudioMarkupFromXml(audioMarkupFilePathAbs.toStdWString(), syncPoints);
		if (!std::get<0>(loadOp))
			return loadOp;

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
				
				auto it = phoneNameToFeaturesVector.find(phoneName);
				if (it == std::end(phoneNameToFeaturesVector))
					phoneNameToFeaturesVector.insert(std::make_pair(phoneName, std::vector<float>()));

				std::vector<float>& mfccFeatures = phoneNameToFeaturesVector[phoneName];
				long begSampleInd = marker.SampleInd;
				long endSampleInd = syncPoints[i + 1].SampleInd;

				int len = endSampleInd - begSampleInd;

				// collect features
				// note, this call requires initialization of Julius library
				int framesCount;
				auto featsOp = PticaGovorun::computeMfccFeaturesPub(&audioSamples[begSampleInd], len, frameSize, frameShift, mfccVecLen, mfccFeatures, framesCount);
				if (!std::get<0>(featsOp))
					return featsOp;

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
		std::map<std::string, std::unique_ptr<PticaGovorun::EMQuick>>& phoneNameToEMObj)
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
			std::unique_ptr<PticaGovorun::EMQuick> pEm = std::make_unique<PticaGovorun::EMQuick>(nclusters, cv::EM::COV_MAT_DIAGONAL, termCrit);
			bool trainOp = pEm->train(mfccFeaturesMat);
			if (!trainOp)
				return std::make_tuple(false, "Can't train the cv::EM for phone");

			phoneNameToEMObj.insert(std::make_pair(phoneName, std::move(pEm)));
		}

		std::make_tuple(true, "");
	}
}

