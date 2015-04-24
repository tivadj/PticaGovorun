#include "stdafx.h"
#include <vector>
#include <array>
#include <cassert>
#include <iostream>
#include "StringUtils.h"
#include <TranscriberUI/PhoneticDictionaryViewModel.h>

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

	void editRecipe1() {
		WordErrorCosts<char> c;
		EditDistance<char, WordErrorCosts<char>> editDist;
		//std::string first( "eat"); // eat
		//std::string second("et");  // e~t
		//std::string first( "et");  // e~t
		//std::string second("eat"); // eat
		//std::string first( "tea"); // t~ea
		//std::string second("toe"); // toe~
		//std::string first( "at nose"); // at nose
		//std::string second("etnos");   // et~nos~
		std::string first( "rests");  // ~~rests
		std::string second("stress"); // stres~s
		editDist.estimateAllDistances(first, second, c);
		float d1 = editDist.distance();

		std::vector<EditStep> editRecipe;
		editDist.minCostRecipe(editRecipe);

		std::vector<char> w1;
		std::vector<char> w2;
		alignWords(wv::make_view(first), wv::make_view(second), editRecipe, '~', w1, w2);

		std::string w1Str(w1.begin(), w1.end());
		std::string w2Str(w2.begin(), w2.end());
		std::cout << w1Str << std::endl;
		std::cout << w2Str << std::endl;
	}

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
		inline int getSubstituteSymbolCostCore(wchar_t x, wchar_t y) const
		{
			if (x == y) return 0;
			boost::optional<CharGroup> xClass = classifyUkrainianChar(x);
			boost::optional<CharGroup> yClass = classifyUkrainianChar(y);
			if (xClass && xClass == yClass) // chars from the same group
				return 1;
			return 99; // chars from different groups
		}
		inline int getSubstituteSymbolCost(wchar_t x, wchar_t y) const
		{
			int cost = getSubstituteSymbolCostCore(x, y);
			std::array<wchar_t, 100> buff;
			swprintf(buff.data(), buff.size(), L"%c %c cost=%d\n", x, y, cost);
			std::wcout << buff.data();
			return cost;
		}
	};

	void editRecipeWide() {
		CharPhonationGroupCosts c;
		EditDistance<wchar_t, CharPhonationGroupCosts> editDist;
		std::wstring first(L"наш граматичний словник містить близько ста сорока тисяч слів");
		//////std::wstring second(L"аж граматичне словник нести безкоста залякати це слів");
		std::wstring second(L"<s> аж граматичне словник нести безкоста залякати це слів");
		//std::wstring first(L"наш");
		//std::wstring second(L"<s> аж");
		//std::wstring first(L"наш граматичний");
		//std::wstring second(L"<s> аж граматичне");
		//std::wstring first(L"наш граматичний словник");
		//std::wstring second(L"<s> аж граматичне словник");
		editDist.estimateAllDistances(first, second, c);
		float d1 = editDist.distance();

		std::vector<EditStep> editRecipe;
		editDist.minCostRecipe(editRecipe);

		std::vector<wchar_t> w1;
		std::vector<wchar_t> w2;
		alignWords(wv::make_view(first), wv::make_view(second), editRecipe, L'~', w1, w2);

		std::wstring w1Str(w1.begin(), w1.end());
		std::wstring w2Str(w2.begin(), w2.end());
		std::wcout << w1Str << std::endl;
		std::wcout << w2Str << std::endl;
	}

	void run()
	{
		//simpleChar();
		//simpleString();
		//stringDistance1();
		//editRecipe1();
		editRecipeWide();
	}
}