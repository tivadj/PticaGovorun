#include <vector>
#include <string>
#include <gtest/gtest.h>
#include "StringUtils.h"
#include <TranscriberUI/PhoneticDictionaryViewModel.h>

namespace PticaGovorunTests
{
	using namespace PticaGovorun;

	struct StringEditDistanceTest : public testing::Test
	{
	};

	template <typename Letter>
	class WordErrorCosts {
	public:
		typedef float CostType;

		static CostType getZeroCosts() {
			return 0;
		}
		inline CostType getInsertSymbolCost(Letter x) const {
			return 1;
		}
		inline CostType getRemoveSymbolCost(Letter x) const {
			return 1;
		}
		inline CostType getSubstituteSymbolCost(Letter x, Letter y) const {
			return x == y ? 0 : 1;
		}
	};

	float charDist(const char* str1, const char* str2)
	{
		using namespace std;
		WordErrorCosts<char> c;
		float result = findEditDistanceNew<char, WordErrorCosts<char> >(string(str1), string(str2), c);
		return result;
	}
	float wordDist(const std::vector<std::wstring>& str1, const std::vector<std::wstring>& str2)
	{
		using namespace std;
		WordErrorCosts<std::wstring> c;
		float result = findEditDistanceNew<std::wstring, WordErrorCosts<std::wstring> >(str1, str2, c);
		return result;
	}

	TEST_F(StringEditDistanceTest, simple1)
	{
		ASSERT_EQ(1, charDist("a1", "b1"));

		//   rests
		// stres~s
		ASSERT_EQ(3, charDist("rests", "stress"));

		// tea t~ea
		// toe toe~
		ASSERT_EQ(2, charDist("tea", "toe"));
	}
	TEST_F(StringEditDistanceTest, wordAsLetter)
	{
		ASSERT_EQ(1, wordDist({ L"a", L"1" },
		{ L"b", L"1" }));
		// inserted
		ASSERT_EQ(1, wordDist({ L"a", L"1" },
		{ L"pre", L"a", L"1" }));
		// removed
		ASSERT_EQ(1, wordDist({ L"a", L"1" }, { L"1" }));
	}
}
