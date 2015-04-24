#include "stdafx.h"
#include <iostream>
#include <tuple>
#include <QFile>
#include <QTextStream>
#include "PhoneticService.h"
#include <CoreUtils.h>

namespace PhoneticSpellerTestsNS
{
	using namespace PticaGovorun;

	void phoneListToStr(const wv::slice<std::string>& phonesList, std::stringstream& result)
	{
		if (phonesList.empty())
			return;
		result << phonesList[0];
		for (size_t i = 1; i < phonesList.size(); ++i)
		{
			result << " " << phonesList[i];
		}
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
		normalizePronunciationVocabulary(wordToPhoneListDict, true, false);

		std::stringstream dumpFileName;
		dumpFileName << "spellErrors.";
		appendTimeStampNow(dumpFileName);
		dumpFileName << ".txt";

		QFile dumpFile(dumpFileName.str().c_str());
		if (!dumpFile.open(QIODevice::WriteOnly | QIODevice::Text))
		{
			std::cerr << "Can't open ouput file" << std::endl;
			return;
		}
		QTextStream dumpFileStream(&dumpFile);
		dumpFileStream.setCodec("UTF-8");

		PhoneRegistry phoneReg;
		initPhoneRegistryUk(phoneReg, true, true);

		int errorThresh = 0;
		for (const auto& pair : wordToPhoneListDict)
		{
			if (pair.second.size() != 1)
				continue;
			const std::wstring& word = pair.first;
			if (word == L"SIL" || word == L"<s>" || word == L"</s>")
				continue;

			const Pronunc& pronDict = pair.second[0];

			std::vector<PhoneId> phones;
			bool spellOp;
			std::tie(spellOp, errMsg) = spellWordUk(phoneReg, word, phones);
			if (!spellOp)
			{
				dumpFileStream << "ERROR: can't spell word='" << QString::fromStdWString(word) << "'" <<errMsg << "\n";
				continue;
			}

			std::string pronAutomatic;
			if (!phoneListToStr(phoneReg, phones, pronAutomatic))
			{
				dumpFileStream << "ERROR: pronuncToStr (Pronunc.ToString)" << "\n";
				continue;
			}

			dumpFileName.str("");
			phoneListToStr(pronDict.Phones, dumpFileName);
			std::string shrekkyPhones = dumpFileName.str();

			bool eqS = shrekkyPhones == pronAutomatic;
			if (!eqS)
			{
				if (false && ++errorThresh > 100)
					break;

				dumpFileStream << "Expect=" << QString::fromLatin1(shrekkyPhones.c_str()) << "\t" << QString::fromLatin1(pronDict.StrDebug.c_str()) << "\t" << QString::fromStdWString(word) << errMsg << "\n";
				dumpFileStream << "Actual=" << QString::fromLatin1(pronAutomatic.c_str()) << "\n";
				dumpFileStream << "\n";
			}
		}
	}
	void run()
	{
		//simple1();
		testShrekky();
	}
}