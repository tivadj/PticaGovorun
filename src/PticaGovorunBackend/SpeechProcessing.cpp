#include "stdafx.h"
#include "SpeechProcessing.h"
#include <regex>
#include <cctype> // std::isalpha

namespace PticaGovorun {

bool getDefaultMarkerStopsPlayback(MarkerLevelOfDetail levelOfDetail)
{
	if (levelOfDetail == MarkerLevelOfDetail::Word)
		return true;

	// do not stop on phone markers
	return false;
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
}

