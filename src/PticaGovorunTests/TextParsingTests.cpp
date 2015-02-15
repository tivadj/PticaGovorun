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

		SECTION("empty string has no words") {
			
			std::wstring s1(L" \t\n");
			wordsReader.setInputText(s1);
			REQUIRE(wordsReader.parseSentence(words));
			REQUIRE(words.size() == 0);
			REQUIRE(!wordsReader.parseSentence(words));
		}
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
