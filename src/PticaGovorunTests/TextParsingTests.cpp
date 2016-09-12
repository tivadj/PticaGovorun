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
}
