#include "catch.hpp"
#include <vector>
#include <string>

#include "StringUtils.h"
#include <TranscriberUI/PhoneticDictionaryViewModel.h>

namespace PticaGovorun
{
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

	TEST_CASE("simple1")
	{
		REQUIRE(1 == charDist("a1", "b1"));

		//   rests
		// stres~s
		REQUIRE(3 == charDist("rests", "stress"));

		// tea t~ea
		// toe toe~
		REQUIRE(2 == charDist("tea", "toe"));
	}
	TEST_CASE("word as letter")
	{
		REQUIRE(1 == wordDist({ L"a", L"1" },
		{ L"b", L"1" }));
		// inserted
		REQUIRE(1 == wordDist({ L"a", L"1" },
		{ L"pre", L"a", L"1" }));
		// removed
		REQUIRE(1 == wordDist({ L"a", L"1" }, { L"1" }));
	}
}
