#include <vector>
#include <boost/utility/string_view.hpp>
#include <gtest/gtest.h>

#include "SpeechProcessing.h"

namespace PticaGovorunTests
{
	using namespace PticaGovorun;
	struct SplitUtteranceIntoPronuncListTest : testing::Test
	{
		GrowOnlyPinArena<wchar_t> arena_;
	};

	TEST_F(SplitUtteranceIntoPronuncListTest, simple)
	{
		std::vector<boost::wstring_view> pronCodes;
		splitUtteranceIntoPronuncList(L"пір'я із-за", arena_, pronCodes);
		ASSERT_EQ(2, pronCodes.size());
		EXPECT_EQ(L"пір'я", pronCodes[0]);
		EXPECT_EQ(L"із-за", pronCodes[1]);
	}
	TEST_F(SplitUtteranceIntoPronuncListTest, fillerWords1)
	{
		std::vector<boost::wstring_view> pronCodes;
		splitUtteranceIntoPronuncList(L"<s> </s> <sil> [sp]", arena_, pronCodes);
		ASSERT_EQ(4, pronCodes.size());
		EXPECT_EQ(L"<s>", pronCodes[0]);
		EXPECT_EQ(L"</s>", pronCodes[1]);
		EXPECT_EQ(L"<sil>", pronCodes[2]);
		EXPECT_EQ(L"[sp]", pronCodes[3]);
	}
	TEST_F(SplitUtteranceIntoPronuncListTest, fillerWords2)
	{
		std::vector<boost::wstring_view> pronCodes;
		splitUtteranceIntoPronuncList(L"<s> happy <sil> sheep </s>", arena_, pronCodes);
		ASSERT_EQ(5, pronCodes.size());
		EXPECT_EQ(L"<s>", pronCodes[0]);
		EXPECT_EQ(L"happy", pronCodes[1]);
		EXPECT_EQ(L"<sil>", pronCodes[2]);
		EXPECT_EQ(L"sheep", pronCodes[3]);
		EXPECT_EQ(L"</s>", pronCodes[4]);
	}
	TEST_F(SplitUtteranceIntoPronuncListTest, ignoreMultipleSpaces)
	{
		std::vector<boost::wstring_view> pronCodes;
		splitUtteranceIntoPronuncList(L"one  two   three", arena_, pronCodes);
		ASSERT_EQ(3, pronCodes.size());
		EXPECT_EQ(L"one", pronCodes[0]);
		EXPECT_EQ(L"two", pronCodes[1]);
		EXPECT_EQ(L"three", pronCodes[2]);
	}
	TEST_F(SplitUtteranceIntoPronuncListTest, ignoreLeadingPound)
	{
		std::vector<boost::wstring_view> pronCodes;
		splitUtteranceIntoPronuncList(L"#one two", arena_, pronCodes);
		ASSERT_EQ(2, pronCodes.size());
		EXPECT_EQ(L"one", pronCodes[0]);
		EXPECT_EQ(L"two", pronCodes[1]);
	}
	TEST_F(SplitUtteranceIntoPronuncListTest, pronunciationMiniLang)
	{
		std::vector<boost::wstring_view> pronCodes;
		splitUtteranceIntoPronuncList(L"{w:slova,s:2}", arena_, pronCodes);
		ASSERT_EQ(1, pronCodes.size());
		EXPECT_EQ(L"slova(2)", pronCodes[0]);

		pronCodes.clear();
		splitUtteranceIntoPronuncList(L"one {w:slova,s:2} two", arena_, pronCodes);
		ASSERT_EQ(3, pronCodes.size());
		EXPECT_EQ(L"one", pronCodes[0]);
		EXPECT_EQ(L"slova(2)", pronCodes[1]);
		EXPECT_EQ(L"two", pronCodes[2]);
	}
}