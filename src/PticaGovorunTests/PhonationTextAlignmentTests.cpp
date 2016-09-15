#include <vector>
#include <array>
#include <string>
#include <iostream>
#include <functional>
#include <gtest/gtest.h>
#include "StringUtils.h"
#include <TextProcessing.h>

namespace PticaGovorunTests
{
	using namespace PticaGovorun;
	struct PhonationTextAlignmentTest : public testing::Test
	{
	};

	class CharPhonationGroupCosts {
	public:
		typedef int CostType;
		static int getZeroCosts() {
			return 0;
		}
		inline int getInsertSymbolCost(wchar_t x) const {
			return 1;
		}
		inline int getRemoveSymbolCost(wchar_t x) const {
			return 1;
		}
		inline int getSubstituteSymbolCost(wchar_t x, wchar_t y) const
		{
			if (x == y) return 0;
			bool isXVowel = isEnglishVowel(x, true);
			bool isYVowel = isEnglishVowel(y, true);

			// allow cheap vowels substitution
			if (isXVowel && isYVowel) // chars from the same vowels group
				return 1;
			// prohibit substitution for chars from different groups
			return 99;
		}
	};

	void testAlignOneWay(const wchar_t* w1, const wchar_t* w2, const wchar_t* w1Align, const wchar_t* w2Align, const char* editOpOrder)
	{
		CharPhonationGroupCosts editCost;
		EditDistance<wchar_t, CharPhonationGroupCosts> editDist;
		setEditOpOrder(editDist, editOpOrder);

		std::wstring first(w1);
		std::wstring second(w2);
		editDist.estimateAllDistances(first, second, editCost);

		std::vector<EditStep> editRecipe;
		editDist.minCostRecipe(editRecipe);

		std::vector<wchar_t> align1;
		std::vector<wchar_t> align2;
		alignWords(wv::make_view(first), wv::make_view(second), editRecipe, L'~', align1, align2);
		std::wstring align1Str(align1.begin(), align1.end());
		std::wstring align2Str(align2.begin(), align2.end());

		std::wstring expect1Str(w1Align);
		std::wstring expect2Str(w2Align);
		EXPECT_EQ(expect1Str, align1Str);
		EXPECT_EQ(expect2Str, align2Str);
	}
	
	void testAlign(const wchar_t* w1, const wchar_t* w2, const wchar_t* w1Align, const wchar_t* w2Align,
		bool symmetricTest = true, const char* editOpOrder = "SRI")
	{
		testAlignOneWay(w1, w2, w1Align, w2Align, editOpOrder);
		if (symmetricTest)
			testAlignOneWay(w2, w1, w2Align, w1Align, editOpOrder); // ensure that the algorithm is symmetric
	}

	TEST_F(PhonationTextAlignmentTest, simpleArtificial)
	{
		// pA -> pA~
		// Aq -> ~Aq
		testAlign(L"pA", L"Aq", L"pA~", L"~Aq");

		// ppA -> ppA~~
		// Aqq -> ~~Aqq
		testAlign(L"ppA", L"Aqq", L"ppA~~", L"~~Aqq");

		// ppAB -> ppA~~B
		// AqqB -> ~~AqqB
		testAlign(L"ppAB", L"AqqB", L"ppA~~B", L"~~AqqB");

		// pAAp -> pA~~Ap
		// AqqA -> ~AqqA~
		testAlign(L"pAAp", L"AqqA", L"pA~~Ap", L"~AqqA~");

		//  AqqA -> ~AqqA~~
		// pAApp -> pA~~App
		testAlign(L"AqqA", L"pAApp", L"~AqqA~~", L"pA~~App");
	}

	TEST_F(PhonationTextAlignmentTest, simpleReal)
	{
		// test removal
		// eat eat
		// et  e~t
		testAlign(L"eat", L"et", L"eat", L"e~t");

		// test insertion
		// et  e~t
		// eat eat
		testAlign(L"et", L"eat", L"e~t", L"eat");

		// rests  ~~rests
		// stress stres~s
		testAlign(L"rests", L"stress", L"~~rests", L"stres~s");

		// cheap vowels substitution
		// at nose -> at nose
		// etnos   -> et~nos~
		testAlign(L"at nose", L"etnos", L"at nose", L"et~nos~");
	}

	TEST_F(PhonationTextAlignmentTest, burdenArtificial)
	{
		// order=SRI: remove P, insert Q
		// pA|ppA|AP|Ap -> pA~ppA~~~A~PAp
		// Aq|Aqq|qA|QA -> ~Aq~~AqqqAQ~A~
		testAlign(L"pAppAAPAp", L"AqAqqqAQA", L"pA~ppA~~~A~PAp", L"~Aq~~AqqqAQ~A~", false, "SRI");
	}

	TEST_F(PhonationTextAlignmentTest, editOperationOrderFromRightToLeft)
	{
		// remove p first
		// ppp -> ~~~ppp
		// qqq -> qqq~~~
		testAlign(L"ppp", L"qqq", L"~~~ppp", L"qqq~~~", false, "SRI");
		// insert q first
		// ppp -> ppp~~~
		// qqq -> ~~~qqq
		testAlign(L"ppp", L"qqq", L"ppp~~~", L"~~~qqq", false, "SIR");
	}

	class ManhattanCosts {
	public:
		typedef float CostType;
		static CostType getZeroCosts() {
			return 0;
		}
		inline CostType getInsertSymbolCost(int x) const {
			return x;
		}
		inline int getRemoveSymbolCost(int x) const {
			return x;
		}
		inline CostType getSubstituteSymbolCost(int x, int y) const
		{
			return std::abs(x - y);
		}
	};

	void intAlignOneWay(std::initializer_list<int> w1, std::initializer_list<int> w2, const wchar_t* w1Align, const wchar_t* w2Align)
	{
		ManhattanCosts editCost;
		EditDistance<int, ManhattanCosts> editDist;

		std::vector<int> w1Vec(w1);
		std::vector<int> w2Vec(w2);
		editDist.estimateAllDistances(w1Vec, w2Vec, editCost);
		std::vector<EditStep> editRecipe;
		editDist.minCostRecipe(editRecipe);

		std::function<void (int, std::vector<wchar_t>&)> int2str = [](int value, std::vector<wchar_t>& str)
		{
			std::wstringstream sbuf;
			sbuf << value;
			std::wstring res = sbuf.str();
			std::copy(res.begin(), res.end(), std::back_inserter(str));
		};

		std::vector<wchar_t> align1;
		std::vector<wchar_t> align2;
		alignWords(wv::make_view(w1Vec), wv::make_view(w2Vec), int2str, editRecipe, L'~', align1, align2, boost::make_optional(L'~'));
		std::wstring align1Str(align1.begin(), align1.end());
		std::wstring align2Str(align2.begin(), align2.end());

		std::wstring expect1Str(w1Align);
		std::wstring expect2Str(w2Align);
		EXPECT_EQ(expect1Str, align1Str);
		EXPECT_EQ(expect2Str, align2Str);
	}
	void intAlign(std::initializer_list<int> w1, std::initializer_list<int> w2, const wchar_t* align1Str, const wchar_t* align2Str)
	{
		intAlignOneWay(w1, w2, align1Str, align2Str);
		intAlignOneWay(w2, w1, align2Str, align1Str);
	}

	TEST_F(PhonationTextAlignmentTest, alignIntSeqs)
	{
		// {77, 2, 79, 13}    -> ~~~77~2~~79~13
		// {12, 66, 11, 68,6} -> 12~66~11~68~6~
		intAlign({ 77, 2, 79, 13 }, { 12, 66, 11, 68, 6 }, L"~~~77~2~~79~13", L"12~66~11~68~6~");
	}
}
