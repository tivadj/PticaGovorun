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

	void testShrekky()
	{
		PhoneRegistry phoneReg;
		initPhoneRegistryUk(phoneReg, true, true);

		QTextCodec* pTextCodec = QTextCodec::codecForName("windows-1251");
		const wchar_t* shrekkyDic = LR"path(C:\devb\PticaGovorunProj\data\shrekky\shrekkyDic.voca)path";
		//const wchar_t* shrekkyDic = LR"path(C:\devb\PticaGovorunProj\data\phoneticDic\test1.txt)path";
		const wchar_t* knownDict = LR"path(C:\devb\PticaGovorunProj\data\TrainSphinx\SpeechModels\phoneticKnown.yml)path";

		std::vector<PhoneticWord> wordTranscrip;
		std::vector<std::string> brokenLines;
		bool loadOp;
		const char* errMsg;
		//std::tie(loadOp, errMsg) = loadPhoneticDictionaryPronIdPerLine(shrekkyDic, phoneReg, *pTextCodec, wordTranscrip, brokenLines);
		std::tie(loadOp, errMsg) = loadPhoneticDictionaryYaml(knownDict, phoneReg, wordTranscrip);
		if (!loadOp)
			return;

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

		int errorThresh = 0;
		for (const PhoneticWord& phWord : wordTranscrip)
		{
			const std::wstring& word = phWord.Word;
			std::vector<PhoneId> phonesAuto;
			bool spellOp;
			std::tie(spellOp, errMsg) = spellWordUk(phoneReg, word, phonesAuto);
			if (!spellOp)
			{
				dumpFileStream << "ERROR: can't spell word='" << QString::fromStdWString(word) << "'" << errMsg << "\n";
				continue;
			}
			updatePhoneModifiers(phoneReg, true, false, phonesAuto);

			for (const PronunciationFlavour& pron : phWord.Pronunciations)
			{
				std::vector<PhoneId> phonesDict = pron.Phones;
				updatePhoneModifiers(phoneReg, true, false, phonesDict);

				bool eqS = phonesAuto == phonesDict;
				if (!eqS)
				{
					if (false && ++errorThresh > 100)
						break;

					std::string pronDictStr;
					if (!phoneListToStr(phoneReg, phonesDict, pronDictStr))
					{
						dumpFileStream << "ERROR: pronuncToStr (Pronunc.ToString)" << "\n";
						continue;
					}
					std::string pronAutoStr;
					if (!phoneListToStr(phoneReg, phonesAuto, pronAutoStr))
					{
						dumpFileStream << "ERROR: pronuncToStr (Pronunc.ToString)" << "\n";
						continue;
					}

					dumpFileStream << "Dict=" << QString::fromLatin1(pronDictStr.c_str()) << "\t" << QString::fromStdWString(pron.PronAsWord) << "\t" << QString::fromStdWString(word) << "\n";
					dumpFileStream << "Actl=" << QString::fromLatin1(pronAutoStr.c_str()) << "\n";
					dumpFileStream << "\n";
				}
			}
		}
	}
	void run()
	{
		//simple1();
		testShrekky();
	}
}