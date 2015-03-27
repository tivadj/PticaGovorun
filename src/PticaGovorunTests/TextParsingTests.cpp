#include "catch.hpp"
#include <vector>

#include "ClnUtils.h"
#include "TextProcessing.h"

namespace PticaGovorun
{
	TEST_CASE("extract one sentence")
	{
		TextParser wordsReader;
		std::vector<wv::slice<wchar_t>> words;
		words.reserve(64);

		SECTION("simple") {
			std::wstring s1(L"john");
			wordsReader.setInputText(s1);
			REQUIRE(wordsReader.parseSentence(words));
			REQUIRE(words.size() == 1);
			REQUIRE(L"john" == toString(words[0]));
			REQUIRE(!wordsReader.parseSentence(words));
		}
		SECTION("simple with dot") {
			std::wstring s1(L"john.");
			wordsReader.setInputText(s1);
			REQUIRE(wordsReader.parseSentence(words));
			REQUIRE(words.size() == 1);
			REQUIRE(L"john" == toString(words[0]));
			REQUIRE(!wordsReader.parseSentence(words));
		}
		SECTION("simple with exclamation") {
			std::wstring s1(L"john!");
			wordsReader.setInputText(s1);
			REQUIRE(wordsReader.parseSentence(words));
			REQUIRE(words.size() == 1);
			REQUIRE(L"john" == toString(words[0]));
			REQUIRE(!wordsReader.parseSentence(words));
		}
		SECTION("simple with question") {
			std::wstring s1(L"one two?");
			wordsReader.setInputText(s1);
			REQUIRE(wordsReader.parseSentence(words));
			REQUIRE(words.size() == 2);
			REQUIRE(L"one" == toString(words[0]));
			REQUIRE(L"two" == toString(words[1]));
			REQUIRE(!wordsReader.parseSentence(words));
		}
		SECTION("simple with comma and dash") {
			std::wstring s1(L"one, - two.");
			wordsReader.setInputText(s1);
			REQUIRE(wordsReader.parseSentence(words));
			REQUIRE(words.size() == 2);
			REQUIRE(L"one" == toString(words[0]));
			REQUIRE(L"two" == toString(words[1]));
			REQUIRE(!wordsReader.parseSentence(words));
		}
		SECTION("empty string has no words") {
			std::wstring s1(L" \t\n");
			wordsReader.setInputText(s1);
			REQUIRE(wordsReader.parseSentence(words));
			REQUIRE(words.size() == 0);
			REQUIRE(!wordsReader.parseSentence(words));
		}
		SECTION("treat CR LF as a separator") {
			std::wstring s1(L"\rpress\r\r\renter\n\n\n");
			wordsReader.setInputText(s1);
			REQUIRE(wordsReader.parseSentence(words));
			REQUIRE(words.size() == 2);
			REQUIRE(L"press" == toString(words[0]));
			REQUIRE(L"enter" == toString(words[1]));
			REQUIRE(!wordsReader.parseSentence(words));
		}
	}

	TEST_CASE("extract multiple sentences")
	{
		TextParser wordsReader;
		std::vector<wv::slice<wchar_t>> words;
		words.reserve(64);

		SECTION("remainder of multiple sentences")
		{
			std::wstring s1 = std::wstring(L"one two. three four!");
			wordsReader.setInputText(s1);

			REQUIRE(wordsReader.parseSentence(words));
			REQUIRE(words.size() == 2);
			REQUIRE(L"one" == toString(words[0]));
			REQUIRE(L"two" == toString(words[1]));

			words.clear();
			REQUIRE(wordsReader.parseSentence(words));
			REQUIRE(words.size() == 2);
			REQUIRE(L"three" == toString(words[0]));
			REQUIRE(L"four" == toString(words[1]));
			REQUIRE(!wordsReader.parseSentence(words));
		}
	}

	TEST_CASE("normalization: words transformed to lowercase")
	{
		TextParser wordsReader;
		std::vector<wv::slice<wchar_t>> words;
		words.reserve(64);

		std::wstring s1(L"John Smith");
		wordsReader.setInputText(s1);
		REQUIRE(wordsReader.parseSentence(words));
		REQUIRE(words.size() == 2);
		REQUIRE(L"john" == toString(words[0]));
		REQUIRE(L"smith" == toString(words[1]));
	}

	TEST_CASE("normalization: there is only one style of apostrophes")
	{
		TextParser wordsReader;
		std::vector<wv::slice<wchar_t>> words;
		words.reserve(64);
		
		std::wstring s1(L"O`Brien O’Brien");
		wordsReader.setInputText(s1);
		REQUIRE(wordsReader.parseSentence(words));
		REQUIRE(L"o'brien" == toString(words[0]));
		REQUIRE(L"o'brien" == toString(words[1]));
	}

	TEST_CASE("apostrophe on the word boundary is treated as word separator")
	{
		TextParser wordsReader;
		std::vector<wv::slice<wchar_t>> words;
		words.reserve(64);
		
		std::wstring s1(L"'yin yang'");
		wordsReader.setInputText(s1);
		REQUIRE(wordsReader.parseSentence(words));
		REQUIRE(L"yin"  == toString(words[0]));
		REQUIRE(L"yang" == toString(words[1]));
		REQUIRE(!wordsReader.parseSentence(words));
	}

	TEST_CASE("hyphen in a single word")
	{
		TextParser wordsReader;
		std::vector<wv::slice<wchar_t>> words;
		words.reserve(64);
		
		std::wstring s1(L"ding-dong"); // single word
		wordsReader.setInputText(s1);
		REQUIRE(wordsReader.parseSentence(words));
		REQUIRE(1 == words.size());
		REQUIRE(L"ding-dong"  == toString(words[0]));
		REQUIRE(!wordsReader.parseSentence(words));
	}

	TEST_CASE("hyphen on a word boundary is treated as separator")
	{
		TextParser wordsReader;
		std::vector<wv::slice<wchar_t>> words;
		words.reserve(64);
		
		std::wstring s1(L"-suffix prefix-");
		wordsReader.setInputText(s1);
		REQUIRE(wordsReader.parseSentence(words));
		REQUIRE(2 == words.size());
		REQUIRE(L"suffix"  == toString(words[0]));
		REQUIRE(L"prefix"  == toString(words[1]));
		REQUIRE(!wordsReader.parseSentence(words));
	}

	TEST_CASE("merge word split by optional hyphen")
	{
		TextParser wordsReader;
		std::vector<wv::slice<wchar_t>> words;
		words.reserve(64);

		SECTION("one word") {
			std::wstring s1(L"tw¬in");
			wordsReader.setInputText(s1);
			REQUIRE(wordsReader.parseSentence(words));
			REQUIRE(words.size() == 1);
			REQUIRE(L"twin" == toString(words[0]));
		}
		SECTION("multiple words") {
			std::wstring s1(L"sia¬mese tw¬in");
			wordsReader.setInputText(s1);
			REQUIRE(wordsReader.parseSentence(words));
			REQUIRE(words.size() == 2);
			REQUIRE(L"siamese" == toString(words[0]));
			REQUIRE(L"twin" == toString(words[1]));
		}
		SECTION("contrive: optional hyphen as prefix") {
			std::wstring s1(L"¬tail");
			wordsReader.setInputText(s1);
			REQUIRE(wordsReader.parseSentence(words));
			REQUIRE(words.size() == 1);
			REQUIRE(L"tail" == toString(words[0]));
		}
		SECTION("contrive: optional hyphen as suffix") {
			std::wstring s1(L"head¬");
			wordsReader.setInputText(s1);
			REQUIRE(wordsReader.parseSentence(words));
			REQUIRE(words.size() == 1);
			REQUIRE(L"head" == toString(words[0]));
		}
	}
}
