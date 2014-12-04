#include "stdafx.h"
#include "SpeechProcessing.h"
#include <regex>

namespace PticaGovorun {

std::tuple<bool, std::wstring> convertTextToPhoneList(const std::wstring& text, std::function<auto (const std::wstring&, std::vector<std::string>&) -> void> wordToPhoneListFun, std::vector<std::string>& speechPhones)
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
		speechPhones.push_back(PGPhoneSilence);

		wordBeg = wordSlice.second;
	}

	// there is a silence phone after the last word, keep it

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
}