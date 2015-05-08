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
		phoneReg.setPalatalSupport(PalatalSupport::AsPalatal);
		initPhoneRegistryUk(phoneReg, true, true);

		bool loadOp;
		const char* errMsg;

		const wchar_t* stressDict = LR"path(C:\devb\PticaGovorunProj\data\stressDictUk.xml)path";
		std::unordered_map<std::wstring, int> wordToStressedSyllable;
		std::tie(loadOp, errMsg) = loadStressedSyllableDictionaryXml(stressDict, wordToStressedSyllable);
		if (!loadOp)
		{
			std::cerr << errMsg << std::endl;
			return;
		}

		auto getStressedSyllableIndFun = [&wordToStressedSyllable](boost::wstring_ref word, std::vector<int>& stressedSyllableInds) -> bool
		{
			auto it = wordToStressedSyllable.find(std::wstring(word.data(), word.size()));
			if (it == wordToStressedSyllable.end())
				return false;
			stressedSyllableInds.push_back(it->second);
			return true;
		};

		QTextCodec* pTextCodec = QTextCodec::codecForName("windows-1251");
		const wchar_t* shrekkyDic = LR"path(C:\devb\PticaGovorunProj\data\shrekky\shrekkyDic.voca)path";
		//const wchar_t* shrekkyDic = LR"path(C:\devb\PticaGovorunProj\data\phoneticDic\test1.txt)path";
		const wchar_t* knownDict = LR"path(C:\devb\PticaGovorunProj\srcrep\data\phoneticDictUkKnown.xml)path";
		//const wchar_t* knownDict = LR"path(C:\devb\PticaGovorunProj\data\TrainSphinx\SpeechModels\phoneticDictUkBroken.yml)path";

		GrowOnlyPinArena<wchar_t> stringArena(10000);
		std::vector<PhoneticWord> wordTranscrip;
		std::vector<std::string> brokenLines;
		//std::tie(loadOp, errMsg) = loadPhoneticDictionaryPronIdPerLine(shrekkyDic, phoneReg, *pTextCodec, wordTranscrip, brokenLines);
		std::tie(loadOp, errMsg) = loadPhoneticDictionaryXml(knownDict, phoneReg, wordTranscrip, stringArena);
		if (!loadOp)
		{
			std::cerr << errMsg << std::endl;
			return;
		}

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

		WordPhoneticTranscriber phoneticTranscriber;
		phoneticTranscriber.setStressedSyllableIndFun(getStressedSyllableIndFun);

		int errorThresh = 0;
		for (const PhoneticWord& phWord : wordTranscrip)
		{
			for (const PronunciationFlavour& pron : phWord.Pronunciations)
			{
				int hasStress = isWordStressAssigned(phoneReg, pron.Phones);
				if (!hasStress)
				{
					dumpFileStream << "No stress for pronAsWord=" << toQString(pron.PronCode) <<"\n";
				}
				
				boost::wstring_ref pronAsWord = pron.PronCode;

				boost::wstring_ref pronName;
				parsePronId(pronAsWord, pronName);
				std::wstring pronNameStr(pronName.data(), pronName.size());

				phoneticTranscriber.transcribe(phoneReg, pronNameStr);
				if (phoneticTranscriber.hasError())
				{
					dumpFileStream << "ERROR: can't spell word='" << QString::fromWCharArray(pronName.cbegin(), pronNameStr.size()) << "'" << errMsg << " ";
					dumpFileStream << QString::fromStdWString(phoneticTranscriber.errorString()) << "\n";
					continue;
				}
				std::vector<PhoneId> phonesAuto;
				phoneticTranscriber.copyOutputPhoneIds(phonesAuto);

				bool keepVowelStress = true;
				updatePhoneModifiers(phoneReg, true, keepVowelStress, phonesAuto);

				std::vector<PhoneId> phonesDict = pron.Phones;
				updatePhoneModifiers(phoneReg, true, keepVowelStress, phonesDict);

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

					dumpFileStream << "Dict=" << QString::fromLatin1(pronDictStr.c_str()) << "\t" << toQString(pron.PronCode) << "\t" << toQString(phWord.Word) << "\n";
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