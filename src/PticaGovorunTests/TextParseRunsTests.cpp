#include <vector>
#include <gtest/gtest.h>
#include "TextProcessing.h"

namespace PticaGovorunTests
{
	using namespace PticaGovorun;

	bool parseTillDotNoSpace(TextParserNew& wordsReader, std::vector<RawTextRun>& words)
	{
		bool op = wordsReader.parseTokensTillDot(words);
		removeWhitespaceLexemes(words);
		return op;
	}

	struct TextParseRunsTest : testing::Test
	{
		TextParserNew wordsReader;
		std::vector<RawTextRun> words;
	};

	TEST_F(TextParseRunsTest, parsesOneWord)
	{
		std::wstring s1(L"john");
		wordsReader.setInputText(s1);
		EXPECT_FALSE(parseTillDotNoSpace(wordsReader, words));
		ASSERT_EQ(1, words.size());
		EXPECT_EQ(L"john", str(words[0]));
	}

	TEST_F(TextParseRunsTest, parsesOneWordAndDot)
	{
		std::wstring s1(L"john.");
		wordsReader.setInputText(s1);
		EXPECT_TRUE(parseTillDotNoSpace(wordsReader, words));
		ASSERT_EQ(2, words.size());
		EXPECT_EQ(L"john", str(words[0]));
		EXPECT_EQ(L".", str(words[1]));
		EXPECT_FALSE(parseTillDotNoSpace(wordsReader, words));
		ASSERT_EQ(2, words.size());
	}

	TEST_F(TextParseRunsTest, parsesOneWordAndExclamation)
	{
		std::wstring s1(L"john!");
		wordsReader.setInputText(s1);
		EXPECT_TRUE(parseTillDotNoSpace(wordsReader, words));
		ASSERT_EQ(2, words.size());
		EXPECT_EQ(L"john", str(words[0]));
		EXPECT_EQ(L"!", str(words[1]));
		EXPECT_TRUE(!parseTillDotNoSpace(wordsReader, words));
		ASSERT_EQ(2, words.size());
	}

	TEST_F(TextParseRunsTest, parsesTwoWordsAndQuestion)
	{
		std::wstring s1(L"one two?");
		wordsReader.setInputText(s1);
		EXPECT_TRUE(parseTillDotNoSpace(wordsReader, words));
		ASSERT_EQ(3, words.size());
		EXPECT_EQ(L"one", str(words[0]));
		EXPECT_EQ(L"two", str(words[1]));
		EXPECT_EQ(L"?", str(words[2]));
		EXPECT_TRUE(!parseTillDotNoSpace(wordsReader, words));
		ASSERT_EQ(3, words.size());
	}

	TEST_F(TextParseRunsTest, parsesSimpleWithCommaAndDash)
	{
		std::wstring s1(L"one, - two.");
		wordsReader.setInputText(s1);
		EXPECT_TRUE(parseTillDotNoSpace(wordsReader, words));
		ASSERT_EQ(5, words.size());
		EXPECT_EQ(L"one", str(words[0]));
		EXPECT_EQ(L",", str(words[1]));
		EXPECT_EQ(L"-", str(words[2]));
		EXPECT_EQ(L"two", str(words[3]));
		EXPECT_EQ(L".", str(words[4]));
		EXPECT_FALSE(parseTillDotNoSpace(wordsReader, words));
		ASSERT_EQ(5, words.size());
	}

	TEST_F(TextParseRunsTest, emptyStringHasNoWords)
	{
		std::wstring s1(L" \t\n");
		wordsReader.setInputText(s1);
		EXPECT_FALSE(parseTillDotNoSpace(wordsReader, words));
		ASSERT_EQ(0, words.size());
	}

	TEST_F(TextParseRunsTest, treats_CR_LF_as_a_separator)
	{
		std::wstring s1(L"\rpress\r\r\renter\n\n\n");
		wordsReader.setInputText(s1);
		EXPECT_FALSE(parseTillDotNoSpace(wordsReader, words));
		ASSERT_EQ(2, words.size());
		EXPECT_EQ(L"press", str(words[0]));
		EXPECT_EQ(L"enter", str(words[1]));
	}

	TEST_F(TextParseRunsTest, hyphenInASingleWordOne)
	{
		std::wstring s1(L"ding-dong"); // single word
		wordsReader.setInputText(s1);
		EXPECT_FALSE(parseTillDotNoSpace(wordsReader, words));
		ASSERT_EQ(3, words.size());
		EXPECT_EQ(L"ding", str(words[0]));
		EXPECT_EQ(L"-", str(words[1]));
		EXPECT_EQ(L"dong", str(words[2]));
	}

	TEST_F(TextParseRunsTest, hyphenInASingleWordTwo)
	{
		std::wstring s2(L"Van-der-Waals");
		wordsReader.setInputText(s2);
		EXPECT_FALSE(parseTillDotNoSpace(wordsReader, words));
		ASSERT_EQ(5, words.size());
		EXPECT_EQ(L"Van", str(words[0]));
		EXPECT_EQ(L"-", str(words[1]));
		EXPECT_EQ(L"der", str(words[2]));
		EXPECT_EQ(L"-", str(words[3]));
		EXPECT_EQ(L"Waals", str(words[4]));
	}

	TEST_F(TextParseRunsTest, optHyphenInASingleWord)
	{
		std::wstring s1(L"tw¬in"); // single word
		wordsReader.setInputText(s1);
		EXPECT_FALSE(parseTillDotNoSpace(wordsReader, words));
		ASSERT_EQ(3, words.size());
		EXPECT_EQ(L"tw", str(words[0]));
		EXPECT_EQ(L"¬", str(words[1]));
		EXPECT_EQ(L"in", str(words[2]));
	}

	TEST_F(TextParseRunsTest, abbreviation1)
	{
		std::wstring s1(L"ó ò. ÷. áóëî");
		wordsReader.setInputText(s1);
		EXPECT_TRUE(parseTillDotNoSpace(wordsReader, words));
		ASSERT_EQ(3, words.size());
		EXPECT_EQ(L"ó", str(words[0]));
		EXPECT_EQ(L"ò", str(words[1]));
		EXPECT_EQ(L".", str(words[2]));
		EXPECT_TRUE(parseTillDotNoSpace(wordsReader, words));
		ASSERT_EQ(5, words.size());
		EXPECT_EQ(L"÷", str(words[3]));
		EXPECT_EQ(L".", str(words[4]));
		EXPECT_FALSE (parseTillDotNoSpace(wordsReader, words));
		ASSERT_EQ(6, words.size());
		EXPECT_EQ(L"áóëî", str(words[5]));
	}

	TEST_F(TextParseRunsTest, abbreviation2)
	{
		std::wstring s1(L"ó 30-õ ðð. XX ñò."); // single word
		wordsReader.setInputText(s1);
		EXPECT_TRUE(parseTillDotNoSpace(wordsReader, words));
		ASSERT_EQ(6, words.size());
		EXPECT_EQ(L"ó", str(words[0]));
		EXPECT_EQ(L"30", str(words[1]));
		EXPECT_EQ(L"-", str(words[2]));
		EXPECT_EQ(L"õ", str(words[3]));
		EXPECT_EQ(L"ðð", str(words[4]));
		EXPECT_EQ(L".", str(words[5]));
		EXPECT_TRUE(parseTillDotNoSpace(wordsReader, words));
		ASSERT_EQ(9, words.size());
		EXPECT_EQ(L"XX", str(words[6]));
		EXPECT_EQ(L"ñò", str(words[7]));
		EXPECT_EQ(L".", str(words[8]));
		EXPECT_FALSE(parseTillDotNoSpace(wordsReader, words));
		ASSERT_EQ(9, words.size());
	}

	TEST_F(TextParseRunsTest, abbreviation3)
	{
		std::wstring s1(L"St. Andrew's Church, Kiev"); // single word
		wordsReader.setInputText(s1);
		EXPECT_TRUE(parseTillDotNoSpace(wordsReader, words));
		ASSERT_EQ(2, words.size());
		EXPECT_EQ(L"St", str(words[0]));
		EXPECT_EQ(L".", str(words[1]));
		EXPECT_FALSE(parseTillDotNoSpace(wordsReader, words));
		ASSERT_EQ(8, words.size());
		EXPECT_EQ(L"Andrew", str(words[2]));
		EXPECT_EQ(L"'", str(words[3]));
		EXPECT_EQ(L"s", str(words[4]));
		EXPECT_EQ(L"Church", str(words[5]));
		EXPECT_EQ(L",", str(words[6]));
		EXPECT_EQ(L"Kiev", str(words[7]));
	}
}
