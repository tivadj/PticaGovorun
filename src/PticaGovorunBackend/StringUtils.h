#include <vector>
#include <array>
#include "ClnUtils.h"

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
	
	// the enum value is the index in the priorities array
	enum class StringEditOp
	{
		RemoveOp = 0,
		InsertOp = 1,
		SubstituteOp = 2
	};

	struct EditStep
	{
		StringEditOp Change;
		union
		{
			struct RemoveChangeType
			{
				size_t FirstRemoveInd;
			} Remove;
			struct InsertChangeType
			{
				size_t FirstInsertInd;
				size_t SecondSourceInd; // index in the second word to get letter from
			} Insert;
			struct SubstituteChangeType
			{
				size_t FirstInd;
				size_t SecondInd;
			} Substitute;
		};
	};


	template <typename Letter>
	bool validateRecipe(const std::vector<EditStep>& recipe, wv::slice<Letter> first, wv::slice<Letter> second)
	{
		std::vector<Letter> buff(first.begin(), first.end());
		for (const EditStep& edit : recipe)
		{
			if (edit.Change == StringEditOp::RemoveOp) // remove A(colsIndex); horizontal move
			{
				size_t removeInd = edit.Remove.FirstRemoveInd;

				bool validInd = removeInd >= 0 && removeInd < buff.size();
				if (!validInd) // remove index is out of range
					return false;

				buff.erase(buff.begin() + removeInd);
			}
			else if (edit.Change == StringEditOp::InsertOp) // insert into A(colsIndex+1) the letter B(rowsInd); vertical move
			{
				size_t insInd = edit.Insert.FirstInsertInd;
				bool validInd = insInd >= 0 && insInd <= buff.size(); // note, i=size is a valid insert pos
				if (!validInd) // insert index is out of range
					return false;

				size_t sourceInd = edit.Insert.SecondSourceInd;
				validInd = sourceInd >= 0 && sourceInd < second.size();
				if (!validInd) // source char index is out of range
					return false;

				Letter let = second[sourceInd];
				buff.insert(buff.begin() + insInd, let);
			}
			else if (edit.Change == StringEditOp::SubstituteOp)
			{
				// substitute A(colsInd) with B(rowInd)
				size_t firstInd = edit.Substitute.FirstInd;
				size_t secondInd = edit.Substitute.SecondInd;

				bool validInd = firstInd >= 0 && firstInd < buff.size();
				if (!validInd) // remove index is out of range
					return false;
				validInd = secondInd >= 0 && secondInd < second.size();
				if (!validInd) // source char index is out of range
					return false;

				Letter let = second[secondInd];
				buff[firstInd] = let;
			}
		}
		bool result = std::equal(buff.begin(), buff.end(), second.begin());
		return result;
	}

	template <typename Letter, typename EditCosts>
	class EditDistance
	{
	public:
		typedef typename EditCosts::CostType CostType;
	private:
		struct Cell
		{
			CostType Cost;
			StringEditOp PrevCellEditOp;
		};
	private:
		std::vector<Cell> buff_;
		size_t colsCount_;
		size_t rowsCount_;
		wv::slice<Letter> first_;
		wv::slice<Letter> second_;
		std::array<int, 3> editOpOrder_; // the values are priorities of each change strategy
	public:
		EditDistance()
		{
			setEditOpOrder(1, 2, 3);
		}

		// If two strategies have equal cost, the one with lesser order is chosen.
		void setEditOpOrder(int removeOrder, int insertOrder, int substituteOrder)
		{
			editOpOrder_[(size_t)StringEditOp::InsertOp] = insertOrder;
			editOpOrder_[(size_t)StringEditOp::RemoveOp] = removeOrder;
			editOpOrder_[(size_t)StringEditOp::SubstituteOp] = substituteOrder;
		}

		// Fills the cost matrix for given two words.
		void estimateAllDistances(wv::slice<Letter> first, wv::slice<Letter> second, EditCosts& editCosts)
		{
			size_t sizeFirst = first.size();
			size_t sizeSecond = second.size();

			// the buffer (2D matrix) to keep distance costs between all prefixes of two words
			// the first word is in the column headers (to the bottom of 2D matrix), from left to right
			// the second word is in the row headers (to the left of imaginary 2D matrix), from bottom to top
			// +1 designates the prefix of zero size
			// the origin (0,0) is in the (left, bottom) corner; matrix is filled by rows
			rowsCount_ = (sizeSecond + 1);
			colsCount_ = sizeFirst + 1;
			first_ = first;
			second_ = second;
			buff_.resize(colsCount_ * rowsCount_);

			// init first line (in the bottom of the matrix)
			// every prefix of A can be morphed into an empty string by iteratively remving the last letter
			for (size_t firstInd = 0; firstInd < colsCount_; firstInd++)
				buff_[dim1Index(firstInd, 0)] = Cell{ static_cast<CostType>(firstInd), StringEditOp::RemoveOp };

			// propogate other costs
			for (size_t secondInd = 1; secondInd < sizeSecond + 1; secondInd++)
			{
				// first element of each row
				// every empty string can be converted into any prefix of B
				buff_[dim1Index(0, secondInd)] = Cell{ static_cast<CostType>(secondInd), StringEditOp::InsertOp };

				for (size_t firstInd = 1; firstInd < colsCount_; firstInd++)
				{
					std::array<Cell, 3> candidChages;

					// remove letter A(x) from first word to get A(0:x-1) and then morph A(0:x-1) into B(0:y)
					// moves in the current row from left to right
					candidChages[0].Cost = editCosts.getRemoveSymbolCost(first[firstInd - 1]) + buff_[dim1Index(firstInd - 1, secondInd)].Cost;
					candidChages[0].PrevCellEditOp = StringEditOp::RemoveOp;

					// morph A(0:x) into B(0:y-1) and then append (insert) B(y) to the second word B(0:y-1)
					// moves in the current column from bottom to top
					candidChages[1].Cost = buff_[dim1Index(firstInd, secondInd - 1)].Cost + editCosts.getInsertSymbolCost(second[secondInd - 1]);
					candidChages[1].PrevCellEditOp = StringEditOp::InsertOp;

					// substitute A(x) into B(y) and then morph A(0:x-1) into B(0:y-1)
					// does diagonal move: from left to right and from bottom to top
					candidChages[2].Cost = editCosts.getSubstituteSymbolCost(first[firstInd - 1], second[secondInd - 1]) + buff_[dim1Index(firstInd - 1, secondInd - 1)].Cost;
					candidChages[2].PrevCellEditOp = StringEditOp::SubstituteOp;
					
					auto minCostMoveIt = std::min_element(candidChages.begin(), candidChages.end(), [this](Cell& c1, Cell& c2)
					{
						// first, order by cost (less cost is better)
						if (c1.Cost != c2.Cost)
							return c1.Cost < c2.Cost;

						// for equal costs, choose user specified order
						return editOpOrder_[(int)c1.PrevCellEditOp] < editOpOrder_[(int)c2.PrevCellEditOp];
					});

					Cell& curCell = buff_[dim1Index(firstInd, secondInd)];
					curCell.Cost = minCostMoveIt->Cost;
					curCell.PrevCellEditOp = minCostMoveIt->PrevCellEditOp;
				}
			}
		}

		CostType distance() const
		{
			PG_Assert(rowsCount_ >= 1 && colsCount_ >= 1 && "Call estimateAllDistances() first");
			size_t topRightInd = dim1Index(colsCount_ - 1, rowsCount_ - 1);
			PG_Assert(topRightInd < buff_.size());
			return buff_[topRightInd].Cost;
		}

		void minCostRecipe(std::vector<EditStep>& recipe)
		{
			PG_Assert(rowsCount_ >= 1 && colsCount_ >= 1 && "Call estimateAllDistances() first");
			for (size_t rowInd = rowsCount_ - 1, colInd = colsCount_ - 1; rowInd >= 1 || colInd >= 1; )
			{
				size_t cellInd = dim1Index(colInd, rowInd);
				PG_Assert(cellInd < buff_.size());
				
				const Cell& cell = buff_[cellInd];

				EditStep step;
				step.Change = cell.PrevCellEditOp;
				if (cell.PrevCellEditOp == StringEditOp::RemoveOp) // remove A(colsIndex); horizontal move
				{
					step.Remove.FirstRemoveInd = colInd - 1;
					colInd--;
				}
				else if (cell.PrevCellEditOp == StringEditOp::InsertOp) // insert into A(colsIndex+1) the letter B(rowsInd); vertical move
				{
					// -1 to compensate for empty set position
					// +1 to get insert position
					step.Insert.FirstInsertInd = colInd;

					step.Insert.SecondSourceInd = rowInd - 1;
					rowInd--;
				}
				else if (cell.PrevCellEditOp == StringEditOp::SubstituteOp)
				{
					// substitute A(colsInd) with B(rowInd)
					step.Substitute.FirstInd = colInd - 1;
					step.Substitute.SecondInd = rowInd - 1;
					rowInd--;
					colInd--;
				}
				recipe.push_back(step);
			}

			bool isValid = validateRecipe(recipe, first_, second_);
			PG_DbgAssert(isValid && "Created invalid recipe");
		}
	private:
		inline size_t dim1Index(size_t firstInd, size_t secondInd) const
		{
			return firstInd + secondInd * colsCount_;
		}
	};

	template <typename Letter, typename EditCosts /*= StringEditCosts<char>*/>
	typename EditCosts::CostType findEditDistanceNew(wv::slice<Letter> left, wv::slice<Letter> right, EditCosts& editCosts)
	{
		typedef typename EditCosts::CostType CostType;

		EditDistance<Letter, EditCosts> editDist;
		editDist.estimateAllDistances(left, right, editCosts);
		CostType result = editDist.distance();
		return result;
	}

	template <typename Letter>
	void alignWords(const wv::slice<Letter> first, const wv::slice<Letter> second, const std::vector<EditStep>& editRecipe, Letter padChar, std::vector<Letter>& firstAligned, std::vector<Letter>& secondAligned)
	{
		size_t commonSize = std::max(first.size(), second.size());
		size_t firstPad = commonSize - first.size();
		size_t secondPad = commonSize - second.size();

		std::copy(first.begin(), first.end(), std::back_inserter(firstAligned));
		std::reverse(firstAligned.begin(), firstAligned.end());
		for (size_t i = 0; i < firstPad; ++i)
			firstAligned.push_back(padChar);

		std::copy(second.begin(), second.end(), std::back_inserter(secondAligned));
		std::reverse(secondAligned.begin(), secondAligned.end());
		for (size_t i = 0; i < secondPad; ++i)
			secondAligned.push_back(padChar);

		auto shiftRight = [](std::vector<Letter>& data, size_t ind)
		{
			for (size_t i = data.size() - 1; i >= ind + 1; i--)
				data[i] = data[i - 1];
		};

		// assume that both words are aligned by the right side
		size_t alignIndexDebug = commonSize; // chars starting from this are aligned

		for (int i = 0; i < editRecipe.size(); ++i, --alignIndexDebug)
		{
			const EditStep& step = editRecipe[i];
			if (step.Change == StringEditOp::RemoveOp)
			{
				size_t removePos = step.Remove.FirstRemoveInd;
				size_t firstSuffixSize = firstPad + removePos;
				size_t shiftPosR = firstAligned.size() - firstSuffixSize - 1; // shift pos in a reversed pos in B

				// shift B to the left
				if (secondPad == 0) // extend aligned sequences
				{
					firstPad++;
					firstAligned.push_back(padChar);
					secondPad++;
					secondAligned.push_back(padChar);
				}
				PG_DbgAssert(secondPad > 0 && "Must have space for shifting");
				shiftRight(secondAligned, shiftPosR);
				secondPad--; // shifting overwrote one pad char
				secondAligned[shiftPosR] = padChar;
			}
			else if (step.Change == StringEditOp::InsertOp)
			{
				size_t insertPos = step.Insert.FirstInsertInd;
				size_t firstSuffixSize = firstPad + insertPos;
				size_t shiftPosR = firstAligned.size() - firstSuffixSize - 1;
				shiftPosR += 1; // shift pos is next to insert pos

				// shift A(0:insertPos-1) to the left by one char
				if (firstPad == 0) // extend aligned sequences
				{
					firstPad++;
					firstAligned.push_back(padChar);
					secondPad++;
					secondAligned.push_back(padChar);
				}

				PG_DbgAssert(firstPad > 0 && "Must have space for shifting");
				shiftRight(firstAligned, shiftPosR);
				firstPad--; // shifting overwrote one pad char

				firstAligned[shiftPosR] = padChar;
			}
			else if (step.Change == StringEditOp::SubstituteOp)
			{
			}
		}
		//PG_DbgAssert(alignIndexDebug == 0 && "Must align two words");
		std::reverse(firstAligned.begin(), firstAligned.end());
		std::reverse(secondAligned.begin(), secondAligned.end());

		PG_DbgAssert(firstAligned.size() == secondAligned.size() && "Two words must have equal size");
		for (size_t i = 0; i < firstAligned.size(); ++i)
		{
			bool bothPad = firstAligned[i] == padChar && secondAligned[i] == padChar;
			PG_DbgAssert(!bothPad && "Two aligned chars can't be the pad char simultaneously");
		}
	}

	template <typename Letter, typename EditCosts>
	void setEditOpOrder(EditDistance<Letter,EditCosts>& editDist, const std::string& editOpOrderRIS)
	{
		PG_Assert(editOpOrderRIS.size() == 3);
		int removeOrder = -1;
		int insertOrder = -1;
		int substituteOrder = -1;

		int opCounter = 1;
		for (size_t i = 0; i < editOpOrderRIS.size(); ++i)
		{
			char op = editOpOrderRIS[i];
			if (op == 'R')
				removeOrder = opCounter;
			else if (op == 'I')
				insertOrder = opCounter;
			else if (op == 'S')
				substituteOrder = opCounter;
			else PG_Assert("Unknown character in editOpOrderRIS");
			opCounter++;
		}
		PG_Assert(removeOrder != -1);
		PG_Assert(insertOrder != -1);
		PG_Assert(substituteOrder != -1);
		editDist.setEditOpOrder(removeOrder, insertOrder, substituteOrder);
	}
}