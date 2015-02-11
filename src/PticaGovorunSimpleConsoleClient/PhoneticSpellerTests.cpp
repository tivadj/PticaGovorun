#include "stdafx.h"
#include <iostream>
#include <tuple>
#include "PhoneticService.h"

namespace PhoneticSpellerTestsNS
{
	using namespace PticaGovorun;
	void simple1()
	{
		std::wstring word = L"чорнобиль";
		std::vector<UkrainianPhoneId> phones;
		bool op;
		const char* errMsg;
		std::tie(op, errMsg) = PticaGovorun::spellWord(word, phones);
	}
	void testShrekky()
	{
		QTextCodec* pTextCodec = QTextCodec::codecForName("windows-1251");
		std::map<std::wstring, std::vector<Pronunc>> wordToPhoneListDict;
		const wchar_t* shrekkyDic = LR"path(C:\devb\PticaGovorunProj\data\shrekky\shrekkyDic.voca)path";
		//const wchar_t* shrekkyDic = LR"path(C:\devb\PticaGovorunProj\data\phoneticDic\test1.txt)path";
		bool loadOp;
		const char* errMsg;
		std::tie(loadOp, errMsg) = loadPronunciationVocabulary2(shrekkyDic, wordToPhoneListDict, *pTextCodec);
		if (!loadOp)
			return;
		normalizePronunciationVocabulary(wordToPhoneListDict);
		
		for (const auto& pair : wordToPhoneListDict)
		{
			if (pair.second.size() != 1)
				continue;
			if (pair.first == L"SIL" || pair.first == L"<s>" || pair.first == L"</s>")
				continue;

			const Pronunc& pron1 = pair.second[0];

			std::vector<UkrainianPhoneId> phones;
			bool spellOp;
			std::tie(spellOp, errMsg) = spellWord(pair.first, phones);
			if (!spellOp)
			{
				int z = 0;
			}

			Pronunc pron2;
			if (!pronuncToStr(phones, pron2))
			{
				int z = 0;
			}

			bool eqS = pron1 == pron2;
			if (!eqS)
			{
				int z = 0;
			}
		}
	}
	void run()
	{
		//simple1();
		testShrekky();
	}
}