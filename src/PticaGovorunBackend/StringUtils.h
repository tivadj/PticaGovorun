#include <vector>

namespace PticaGovorun
{
	template <typename Letter>
	class StringLevenshteinCosts {
	public:
		typedef int CostType;

		static int getZeroCosts() {
			return 0;
		}

		inline int getInsertSymbolCost(Letter x) {
			return 1;
		}
		inline int getRemoveSymbolCost(Letter x) {
			return 1;
		}
	};

	template <typename Letter>
	class StringEditCosts : public StringLevenshteinCosts < Letter > {
	public:
		inline int getSubstituteSymbolCost(Letter x, Letter y) {
			return x == y ? 0 : 2;
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

		for (int j = 0; j < size2 + 1; j++)
			costs[0][j] = j;

		for (int i = 1; i < size1 + 1; i++)
		{
			costs[i][0] = i;

			for (int j = 1; j < size2 + 1; j++) {
				auto up = costs[i - 1][j] + editCosts.getRemoveSymbolCost(s1Begin[i - 1]);
				auto left = costs[i][j - 1] + editCosts.getInsertSymbolCost(s2Begin[j - 1]);
				auto diag = costs[i - 1][j - 1] + editCosts.getSubstituteSymbolCost(s1Begin[i - 1], s2Begin[j - 1]);
				costs[i][j] = std::min(std::min(up, left), diag);
			}
		}

		auto result = costs[size1][size2];
		return result;
	}
}