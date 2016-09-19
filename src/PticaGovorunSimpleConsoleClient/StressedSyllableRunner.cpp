#include <iostream>
#include <map>
#include <tuple>
#include <QFile>
#include <QTextStream>
#include <QXmlStreamWriter>
#include "PhoneticService.h"
#include <CoreUtils.h>
#include "assertImpl.h"

namespace StressedSyllableRunnerNS
{
	using namespace PticaGovorun;

	int vowelsCount(const PhoneRegistry& phoneReg, const std::vector<PhoneId>& phoneIds)
	{
		int numVowels = 0;
		for (size_t i = 0; i < phoneIds.size(); ++i)
		{
			PhoneId phoneId = phoneIds[i];

			const Phone* phone = phoneReg.phoneById(phoneId);
			const BasicPhone* basicPhone = phoneReg.basicPhone(phone->BasicPhoneId);
			if (basicPhone->DerivedFromChar == CharGroup::Vowel)
				numVowels++;
		}
		return numVowels;
	}

	void populateStressedVowels(const PhoneRegistry& phoneReg, const std::vector<PhoneId>& phoneIds, std::vector<int>& charInds)
	{
		for (size_t i = 0; i < phoneIds.size(); ++i)
		{
			PhoneId phoneId = phoneIds[i];

			const Phone* phone = phoneReg.phoneById(phoneId);
			if (phone->IsStressed == true)
				charInds.push_back((int)i);
		}
	}
	
	bool populateStressedVowels(const PhoneRegistry& phoneReg, const std::vector<PhoneId>& phoneDict,
		const std::vector<PhoneId>& phoneAuto, const WordPhoneticTranscriber& phoneticTranscriber,
		std::vector<int>& charInds)
	{
		struct PhoneAndBasic
		{
			const Phone* phone;
			const BasicPhone* basicPhone;
			int phoneOriginalInd;
		};
		auto collectVowels = [phoneReg](const std::vector<PhoneId>& phoneIds, std::vector<PhoneAndBasic>& outBasicPhones)
		{
			for (int phoneInd = 0; phoneInd < (int)phoneIds.size(); ++phoneInd)
			{
				PhoneId phoneId = phoneIds[phoneInd];
				const Phone* phone = phoneReg.phoneById(phoneId);
				const BasicPhone* basicPhone = phoneReg.basicPhone(phone->BasicPhoneId);
				if (basicPhone->DerivedFromChar == CharGroup::Vowel)
					outBasicPhones.push_back(PhoneAndBasic{ phone, basicPhone, phoneInd });
			}
		};
		std::vector<PhoneAndBasic> vowelsDict;
		collectVowels(phoneDict, vowelsDict);
		std::vector<PhoneAndBasic> vowelsAuto;
		collectVowels(phoneAuto, vowelsAuto);
		if (vowelsDict.size() != vowelsAuto.size())
			return false;

		for (size_t i = 0; i < vowelsDict.size(); ++i)
		{
			bool match = vowelsDict[i].basicPhone->Id == vowelsAuto[i].basicPhone->Id;
			if (!match)
			{
				if (vowelsDict[i].basicPhone->Name == "Y" && vowelsAuto[i].basicPhone->Name == "I" ||
					vowelsDict[i].basicPhone->Name == "I" && vowelsAuto[i].basicPhone->Name == "Y")
					match = true;
			}			
			if (!match)
				return false;

			if (vowelsDict[i].phone->IsStressed == true)
			{
				// for phone find the corresponding letter index
				int phoneInd = vowelsAuto[i].phoneOriginalInd;
				int letterInd = phoneticTranscriber.getVowelLetterInd(phoneInd);
				if (letterInd == -1)
					return false;
				charInds.push_back(letterInd);
			}
		}

		return !charInds.empty();
	}

	class PronuncStressedSyllableExtractor
	{
		WordPhoneticTranscriber phoneticTranscriber;
		PhoneRegistry phoneReg;
		QTextStream logFileStream;
	public:
		void extractStressedSyllables();
	private:
		bool inferSpelling(const PhoneticWord& phWord, std::vector<int>& firstStressedVowelInds, boost::wstring_view& firstWord);
		void printPron(const std::vector<PhoneId>& phonesDict, boost::wstring_view pronAsWord, boost::wstring_view word, const std::vector<PhoneId>* phonesAuto);
	};

	bool PronuncStressedSyllableExtractor::inferSpelling(const PhoneticWord& phWord, std::vector<int>& firstStressedVowelInds, boost::wstring_view& firstWord)
	{
		// the result is a success if at least one word pronunciation can be processed

		for (const PronunciationFlavour& pron : phWord.Pronunciations)
		{
			phoneticTranscriber.transcribe(phoneReg, toStdWString(pron.PronCode));
			if (phoneticTranscriber.hasError())
			{
				logFileStream << "Warn: can't transcribe" << QString::fromStdWString(phoneticTranscriber.errorString()) << " pronId=" << toQString(pron.PronCode) << " word=" << toQString(phWord.Word) << "\n";
				continue;
			}

			std::vector<PhoneId> phonesAuto;
			phoneticTranscriber.copyOutputPhoneIds(phonesAuto);

			std::vector<int> charInds;
			bool gotStressInds = populateStressedVowels(phoneReg, pron.Phones, phonesAuto, phoneticTranscriber, charInds);
			if (!gotStressInds)
			{
				logFileStream << "Warn: can't infer stressed vowel" << "\n";
				printPron(pron.Phones, pron.PronCode, phWord.Word, &phonesAuto);
				continue;
			}

			bool isFirst = firstStressedVowelInds.empty();
			if (isFirst)
			{
				firstStressedVowelInds = charInds;
				firstWord = pron.PronCode;
			}
			else
			{
				if (charInds != firstStressedVowelInds)
				{
					logFileStream << "Warn: Multiple stress specs defined. pronId=" << toQString(pron.PronCode) << "\t" << toQString(phWord.Word) << "\n";
					return false;
				}
			}
		}
		return !firstStressedVowelInds.empty();
	}
	
	void PronuncStressedSyllableExtractor::printPron(const std::vector<PhoneId>& phonesDict, boost::wstring_view pronAsWord, boost::wstring_view word,
		const std::vector<PhoneId>* phonesAuto)
	{
		std::string pronDictStr;
		if (!phoneListToStr(phoneReg, phonesDict, pronDictStr))
		{
			logFileStream << "ERROR: pronuncToStr (Pronunc.ToString)" << "\n";
			return;
		}
		std::string pronAutoStr;
		if (phonesAuto != nullptr)
			if (!phoneListToStr(phoneReg, *phonesAuto, pronAutoStr))
			{
				logFileStream << "ERROR: pronuncToStr (Pronunc.ToString)" << "\n";
				return;
			}

		logFileStream << "Dict=" << QString::fromLatin1(pronDictStr.c_str()) << "\t" << toQString(pronAsWord) << "\t" << toQString(word) << "\n";
		if (phonesAuto != nullptr)
			logFileStream << "Auto=" << QString::fromLatin1(pronAutoStr.c_str()) << "\n";
	};

	void PronuncStressedSyllableExtractor::extractStressedSyllables()
	{
		phoneReg.setPalatalSupport(PalatalSupport::AsHard);
		initPhoneRegistryUk(phoneReg, true, true);

		QTextCodec* pTextCodec = QTextCodec::codecForName("windows-1251");
		const wchar_t* shrekkyDic = LR"path(C:\devb\PticaGovorunProj\data\shrekky\shrekkyDic2.voca)path";

		GrowOnlyPinArena<wchar_t> stringArena(10000);
		std::vector<PhoneticWord> wordTranscrip;
		std::vector<std::string> brokenLines;
		bool loadOp;
		const char* errMsg;
		std::tie(loadOp, errMsg) = loadPhoneticDictionaryPronIdPerLine(shrekkyDic, phoneReg, *pTextCodec, wordTranscrip, brokenLines, stringArena);
		if (!loadOp)
		{
			std::cerr << errMsg << std::endl;
			return;
		}

		std::string timeStampStr;
		appendTimeStampNow(timeStampStr);

		std::stringstream dumpFileName;
		dumpFileName << "vowelStress." << timeStampStr << ".Errors.txt";

		QFile dumpFile(dumpFileName.str().c_str());
		if (!dumpFile.open(QIODevice::WriteOnly | QIODevice::Text))
		{
			std::cerr << "Can't open ouput file" << std::endl;
			return;
		}
		logFileStream.setDevice(&dumpFile);
		logFileStream.setCodec("UTF-8");

		//
		dumpFileName.str("");
		dumpFileName << "vowelStress." << timeStampStr << ".StressOrder.xml";

		QFile dumpFileStressOrder(dumpFileName.str().c_str());
		if (!dumpFileStressOrder.open(QIODevice::WriteOnly | QIODevice::Text))
		{
			std::cerr << "Can't open ouput file" << std::endl;
			return;
		}
		QXmlStreamWriter xmlWriter(&dumpFileStressOrder);
		xmlWriter.setCodec("UTF-8");
		xmlWriter.setAutoFormatting(true);
		xmlWriter.setAutoFormattingIndent(3);
		xmlWriter.writeStartDocument("1.0");
		xmlWriter.writeStartElement("stressDict");

		std::vector<int> firstStressedVowelInds;
		boost::wstring_view firstWord;
		int extractError = 0;
		for (const PhoneticWord& phWord : wordTranscrip)
		{
			int vowelCharInd = -1;
			if (getStressedVowelCharIndAtMostOne(phWord.Word, vowelCharInd)) // skip trivially determined stressed syllable
				continue;

			firstStressedVowelInds.clear();
			firstWord.clear();
			bool suc = inferSpelling(phWord, firstStressedVowelInds, firstWord);
			if (!suc)
			{
				extractError++;
				for (const auto& pron : phWord.Pronunciations)
				{
					printPron(pron.Phones, pron.PronCode, phWord.Word, nullptr);
				}
				logFileStream << "\n";
				continue;
			}

			// convert char indices to vowel ordinal numbers
			if (!firstStressedVowelInds.empty())
			{
				std::vector<int> vowelOrder;
				std::transform(firstStressedVowelInds.begin(), firstStressedVowelInds.end(), std::back_inserter(vowelOrder), [&firstWord](int charInd)
				{
					int ord = vowelCharIndToSyllableIndUk(firstWord, charInd);
					PG_Assert2(ord != -1, "The index of input char must be vowel");
					return ord+1; // +1 for 1-based index
				});
				
				std::ostringstream stressIndsBuf;
				PticaGovorun::join(vowelOrder.begin(), vowelOrder.end(), boost::string_view(" "), stressIndsBuf);

				xmlWriter.writeStartElement("word");
				xmlWriter.writeAttribute("name", toQString(phWord.Word));
				xmlWriter.writeAttribute("stressedSyllable", QString::fromStdString(stressIndsBuf.str()));
				xmlWriter.writeEndElement();
			}
		}

		xmlWriter.writeEndElement(); // root
		xmlWriter.writeEndDocument();

		std::cout << extractError << std::endl;
	}

	void run()
	{
		PronuncStressedSyllableExtractor extractor;
		extractor.extractStressedSyllables();
	}
}
