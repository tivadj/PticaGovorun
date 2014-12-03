#include "stdafx.h"
#include "SpeechProcessing.h"
#include <regex>

namespace PticaGovorun {

std::tuple<bool, std::wstring> convertTextToPhoneList(const std::wstring& text, std::function<auto (const std::wstring&, std::vector<std::string>&) -> void> wordToPhoneListFun, std::vector<std::string>& speechPhones)
{
	std::wsmatch matchRes;
	std::wregex r(LR"regex(\w+)regex"); // match words

	// iterate through words

	std::wstring word;
	word.reserve(32);

	std::vector<std::string> wordPhones;
	wordPhones.reserve(word.capacity());

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

	if (!speechPhones.empty())
	{
		// remove last silence phone
		speechPhones.pop_back();
	}

	return std::make_tuple(true, L"");
}

}