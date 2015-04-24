#include <vector>
#include <array>
#include <functional>
#include <boost/optional.hpp>
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

		inline int getInsertSymbolCost(Letter x) const {
			return 1;
		}
		inline int getRemoveSymbolCost(Letter x) const {
			return 1;
		}
	};

	template <typename Letter>
	class StringEditCosts : public StringLevenshteinCosts < Letter > {
	public:
		inline int getSubstituteSymbolCost(Letter x, Letter y) const {
			return x == y ? 0 : 2;
		}
	};

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
		void estimateAllDistances(wv::slice<Letter> first, wv::slice<Letter> second, const EditCosts& editCosts)
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

	// Aligns two sequences of symbols. The function to convert symbol to string is given. Two string representations of symbols are separated
	// with sepcified char. If text representation of two symbols have different size, then the shorter one is padded with 'padChar'.
	template <typename Symbol, typename TextChar>
	void alignWords(const wv::slice<Symbol> first, const wv::slice<Symbol> second,
		std::function<void (Symbol, std::vector<TextChar>&)> symbolToStrFun,
		const std::vector<EditStep>& editRecipe, TextChar padChar,
		std::vector<TextChar>& firstAlignedStr, std::vector<TextChar>& secondAlignedStr,
		boost::optional<TextChar> sepChar = nullptr)
	{
		std::vector<TextChar> charBuff1;
		std::vector<TextChar> charBuff2;

		for (size_t editInd = 0; editInd < editRecipe.size(); ++editInd)
		{
			const EditStep& step = editRecipe[editInd];
			if (step.Change == StringEditOp::RemoveOp)
			{
				Symbol remSymb = first[step.Remove.FirstRemoveInd];
				
				charBuff1.clear();
				symbolToStrFun(remSymb, charBuff1);
				std::reverse(charBuff1.begin(), charBuff1.end());

				// append removed symbol
				for (size_t i = 0; i < charBuff1.size(); ++i)
					firstAlignedStr.push_back(charBuff1[i]);

				// pad second sequence with size=removeSymb.str.size
				for (size_t i = 0; i < charBuff1.size(); ++i)
					secondAlignedStr.push_back(padChar);
			}
			else if (step.Change == StringEditOp::InsertOp)
			{
				Symbol insSymb = second[step.Insert.SecondSourceInd];

				charBuff1.clear();
				symbolToStrFun(insSymb, charBuff1);
				std::reverse(charBuff1.begin(), charBuff1.end());

				// pad first sequence with size=insertSymb.str.size
				for (size_t i = 0; i < charBuff1.size(); ++i)
					firstAlignedStr.push_back(padChar);

				// append inserter symbol
				for (size_t i = 0; i < charBuff1.size(); ++i)
					secondAlignedStr.push_back(charBuff1[i]);
			}
			else if (step.Change == StringEditOp::SubstituteOp)
			{
				// do aligned append
				Symbol symb1 = first[step.Substitute.FirstInd];
				Symbol symb2 = second[step.Substitute.SecondInd];

				charBuff1.clear();
				symbolToStrFun(symb1, charBuff1);
				std::reverse(charBuff1.begin(), charBuff1.end());

				charBuff2.clear();
				symbolToStrFun(symb2, charBuff2);
				std::reverse(charBuff2.begin(), charBuff2.end());

				size_t symb1Size = charBuff1.size();
				size_t symb2Size = charBuff2.size();

				std::vector<TextChar>& shorterSymbStr = symb1Size < symb2Size ? firstAlignedStr : secondAlignedStr;
				size_t padSize = symb1Size < symb2Size ? symb2Size - symb1Size : symb1Size - symb2Size;

				// pad shorter sequence
				for (size_t i = 0; i < padSize; ++i)
					shorterSymbStr.push_back(padChar);

				// append symbols
				for (size_t i = 0; i < charBuff1.size(); ++i)
					firstAlignedStr.push_back(charBuff1[i]);
				for (size_t i = 0; i < charBuff2.size(); ++i)
					secondAlignedStr.push_back(charBuff2[i]);
			}
			if (sepChar != nullptr && editInd < editRecipe.size() - 1)
			{
				firstAlignedStr.push_back(sepChar.get());
				secondAlignedStr.push_back(sepChar.get());
			}
		}
		// compensate for reversed way of construction
		std::reverse(firstAlignedStr.begin(), firstAlignedStr.end());
		std::reverse(secondAlignedStr.begin(), secondAlignedStr.end());

		PG_DbgAssert(firstAlignedStr.size() == secondAlignedStr.size() && "Two words must have equal size");
	}

	// overload for decltype(Symbol) = decltype(TextChar)
	template <typename Symbol, typename TextChar, 
		class = typename std::enable_if<
			std::is_same<
				Symbol, 
				TextChar
				>::value
		>::type>
	void alignWords(const wv::slice<Symbol> first, const wv::slice<Symbol> second, 
		const std::vector<EditStep>& editRecipe, TextChar padChar, 
		std::vector<TextChar>& firstAlignedStr, std::vector<TextChar>& secondAlignedStr,
		boost::optional<TextChar> sepChar = nullptr)
	{
		// the default function just copies a symbol to an output
		std::function<void(Symbol, std::vector<TextChar>&)> symb2str = [](Symbol value, std::vector<TextChar>& str)
		{
			str.push_back(value); // decltype(Symbol)=decltype(TextChar)
		};
		alignWords(first, second, symb2str, editRecipe, padChar, firstAlignedStr, secondAlignedStr, sepChar);
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