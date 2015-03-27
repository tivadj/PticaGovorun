#include "stdafx.h"
#include <vector>
#include <array>
#include <cassert>
#include <iostream>
#include "StringUtils.h"

namespace EditDistanceTestsNS
{
	using namespace PticaGovorun;

	template <typename Letter>
	class WordErrorCosts {
	public:
		typedef float CostType;

		static CostType getZeroCosts() {
			return 0;
		}
		inline CostType getInsertSymbolCost(Letter x) {
			return 1;
		}
		inline CostType getRemoveSymbolCost(Letter x) {
			return 1;
		}
		inline CostType getSubstituteSymbolCost(Letter x, Letter y) {
			return x == y ? 0 : 1;
		}
	};

	template <typename RandIter, typename EditCosts /*= StringEditCosts<char>*/>
	typename EditCosts::CostType findEditDistance(RandIter s1Begin, RandIter s1End, RandIter s2Begin, RandIter s2End, EditCosts& editCosts)
	{
		typedef typename EditCosts::CostType CostType;

		int size1 = s1End - s1Begin;
		int size2 = s2End - s2Begin;

		std::vector<std::vector<CostType>> costs(size1 + 1, std::vector<CostType>(size2 + 1, EditCosts::getZeroCosts()));

		// init first line

		for (int j = 0; j<size2 + 1; j++)
			costs[0][j] = j;

		for (int i = 1; i<size1 + 1; i++)
		{
			costs[i][0] = i;

			for (int j = 1; j<size2 + 1; j++) {
				auto up = costs[i - 1][j] + editCosts.getRemoveSymbolCost(s1Begin[i - 1]);
				auto left = costs[i][j - 1] + editCosts.getInsertSymbolCost(s2Begin[j - 1]);
				auto diag = costs[i - 1][j - 1] + editCosts.getSubstituteSymbolCost(s1Begin[i - 1], s2Begin[j - 1]);
				costs[i][j] = std::min(std::min(up, left), diag);
			}
		}

		auto result = costs[size1][size2];
		return result;
	}

	void simpleChar() {
		std::vector<char> s1 = {'a', '1'};
		std::vector<char> s2 = { 'b', '1' };

		WordErrorCosts<char> c;
		int d1 = findEditDistance(s1.begin(), s1.end(), s2.begin(), s2.end(), c);
		assert(1 == d1);
	}

	void simpleString() {
		std::vector<std::wstring> s1 = { L"a", L"1"};
		std::vector<std::wstring> s2 = { L"b", L"1" }; // changed
		std::vector<std::wstring> s3 = { L"pre", L"a", L"1" }; // inserted
		std::vector<std::wstring> s4 = { L"1" }; // removed

		WordErrorCosts<std::wstring> c;
		int d1 = findEditDistance(s1.begin(), s1.end(), s2.begin(), s2.end(), c);
		assert(1 == d1);
		int d3 = findEditDistance(s1.begin(), s1.end(), s3.begin(), s3.end(), c);
		assert(1 == d3);
		int d4 = findEditDistance(s1.begin(), s1.end(), s4.begin(), s4.end(), c);
		assert(1 == d4);
	}

	void stringDistance1() {
		std::vector<char> s1 = {           'r', 'e', 's', 't', 's' };
		std::vector<char> s2 = { 's', 't', 'r', 'e', 's', 's' };

		WordErrorCosts<char> c;
		int d1 = findEditDistance(s1.begin(), s1.end(), s2.begin(), s2.end(), c);
		assert(3, d1);
	}

	void run()
	{
		simpleChar();
		simpleString();
		stringDistance1();
	}
}