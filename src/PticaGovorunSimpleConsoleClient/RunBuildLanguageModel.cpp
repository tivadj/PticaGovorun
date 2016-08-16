#include "stdafx.h"
#include <iostream>
#include <fstream>
#include <string>
#include <memory>
#include <unordered_set>
#include <string>
#include <vector>
#include <set>
#include <chrono> // std::chrono::system_clock
#include <windows.h>
#include <QDebug>
#include <QDirIterator>
#include <QXmlStreamReader>
#include <QFile>
#include <QString>
#include <QTextStream>
#include <QTextCodec>

#include "TextProcessing.h"
#include "LangStat.h"
#include "PhoneticService.h"
#include "CoreUtils.h"
#include "ArpaLanguageModel.h"
#include "assertImpl.h"

namespace RunBuildLanguageModelNS
{
	using namespace PticaGovorun;

	void buildLangModel(const wchar_t* textFilesDir, WordsUsageInfo& wordUsage)
	{
		QXmlStreamReader xml;

		std::vector<wv::slice<wchar_t>> words;
		words.reserve(64);
		TextParser wordsReader;

		QString textFilesDirQ = QString::fromStdWString(textFilesDir);
		QDirIterator it(textFilesDirQ, QStringList() << "*.fb2", QDir::Files, QDirIterator::Subdirectories);
		int processedFiles = 0;
		while (it.hasNext())
		{
			//if (processedFiles >= 2) break; // process less work

			QString txtPath = it.next();
			if (txtPath.contains("BROKEN", Qt::CaseSensitive))
			{
				qDebug() <<"SKIPPED " << txtPath;
				continue;
			}
			qDebug() << txtPath;

			//
			QFile file(txtPath);
			if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
			{
				qDebug() << "Can't open file " << txtPath;
				return;
			}

			// parse file
			xml.setDevice(&file);
			while (!xml.atEnd())
			{
				xml.readNext();
				if (xml.isCharacters())
				{
					// BUG: if the lines inside the text are separated using LF only on windows machines
					//      the function below will concatenate the lines. For now, fix LF->CRLF for such files externally
					QStringRef elementText = xml.text();

					wv::slice<wchar_t> textToParse = wv::make_view((wchar_t*)elementText.data(), elementText.size());
					wordsReader.setInputText(textToParse);

					// extract all sentences from paragraph
					while (true)
					{
						words.clear();
						if (!wordsReader.parseSentence(words))
							break;

						for (int i = 0; i < words.size(); ++i)
						{
							const wv::slice<wchar_t>& wordSlice = words[i];
							PG_Assert(!wordSlice.empty());

							// keep ukrainian words only
							//auto filterOutWord = [](wv::slice<wchar_t> word)
							//{
							//	for (size_t charInd = 0; charInd < word.size(); ++charInd)
							//	{
							//		wchar_t ch = word[charInd];
							//		bool isDigit = isDigitChar(ch);
							//		//bool isLatin = isEnglishChar(ch);
							//		bool isSureLatin = isExclusiveEnglishChar(ch);
							//		bool isSureRus = isExclusiveRussianChar(ch);
							//		//if (isDigit || isLatin || isSureRus)
							//		if (isDigit || isSureLatin || isSureRus)
							//			return true;
							//	}
							//	return false;
							//};
							//if (filterOutWord(wordSlice))
							//{
							//	::OutputDebugStringW(toString(wordSlice).c_str());
							//	::OutputDebugStringW(L"\n");
							//	continue;
							//}

							auto wordCharUsage = [](wv::slice<wchar_t> word, int& digitsCount,
								int& engCount, int& exclEngCount,
								int& rusCount, int& exclRusCount,
								int& hyphenCount)
							{
								for (size_t charInd = 0; charInd < word.size(); ++charInd)
								{
									wchar_t ch = word[charInd];
									bool isDigit = isDigitChar(ch);
									bool isEng = isEnglishChar(ch);
									bool isExclEng = isExclusiveEnglishChar(ch);
									bool isRus = isRussianChar(ch);
									bool isExclRus = isExclusiveRussianChar(ch);
									if (isDigit)
										digitsCount++;
									if (isEng)
										engCount++;
									if (isExclEng)
										exclEngCount++;
									if (isRus)
										rusCount++;
									if (isExclRus)
										exclRusCount++;
									if (ch == L'-' || ch == L'\'')
										hyphenCount++;
								}
							};
							int digitsCount = 0;
							int engCount = 0;
							int exclEngCount = 0;
							int rusCount = 0;
							int exclRusCount = 0;
							int hyphenCount = 0;
							wordCharUsage(wordSlice, digitsCount, engCount, exclEngCount, rusCount, exclRusCount, hyphenCount);
							if (digitsCount > 0 || exclEngCount > 0 || exclRusCount > 0)
							{
								if (digitsCount == wordSlice.size() ||  // number
									(exclEngCount > 0 && (engCount + hyphenCount) == wordSlice.size()) || // english word
									(exclRusCount > 0 && (rusCount + hyphenCount) == wordSlice.size())    // russian word
								   )
								{
									// do not even print the skipped word
									continue;
								}
								else
								{
									::OutputDebugStringW(toString(wordSlice).c_str());
									::OutputDebugStringW(L"\n");
								}
								continue;
							}

							std::wstring str = toString(wordSlice);

							const WordPart* wordPart = wordUsage.getOrAddWordPart(str, WordPartSide::WholeWord);
							
							WordSeqKey wordIds({ wordPart->id() });
							WordSeqUsage* wordSeq = wordUsage.getOrAddWordSequence(wordIds);
							wordSeq->UsedCount++;
						}
					}
				}
			}

			++processedFiles;
		}
	}

	void doWordsPhoneticSplit(const WordsUsageInfo& wordsStat, wv::slice<WordSeqUsage*> wordSeqOrder)
	{
		// split all words into left-right subwords
		std::wstring leftPart, rightPart;
		std::vector<int> sepInds;

		WordsUsageInfo splitWordsStat;
		for (const WordSeqUsage* nodePtr : wordSeqOrder)
		{
			sepInds.clear();

			int wordPartId = nodePtr->Key.PartIds[0];
			const WordPart* wordPart = wordsStat.wordPartById(wordPartId);
			const std::wstring& word = wordPart->partText();

			// 4-characters should already be analyzed for splitting
			// дума-думи
			if (word.size() <= 3)
			{
				// do not split short words
				// they will not create many reusable subwords
				sepInds.push_back((int)word.size());
			}
			else
			{
				int sepInd = phoneticSplitOfWord(word, boost::none);
				if (sepInd != -1)
					sepInds.push_back(sepInd);
				else
				{
					sepInds.push_back((int)word.size());
					// generate all parts
					// ignore short prefixes (because it is not obvious what is reused)
					//int startFrom = 2;
					//for (int sepPos = startFrom; sepPos <= word.size(); ++sepPos)
					//	sepInds.push_back(sepPos);
				}
			}

			//
			for (int sepPos : sepInds)
			{
				int rightPartStartInd = sepPos;

				if (word[sepPos] == L'-' || word[sepPos] == L'\'')
				{
					rightPartStartInd++; // skip hyphen and apostrophe
				}

				wchar_t rightPartChar = word[rightPartStartInd];
				if (isUkrainianConsonant(rightPartChar)) // skip suffixes starting from consonant
					continue;

				const wv::slice<wchar_t>  leftSlice = wv::make_view(word.data(), sepPos);
				const wv::slice<wchar_t> rightSlice = wv::make_view(word.data() + rightPartStartInd, word.size() - rightPartStartInd);

				if (leftSlice.size() > 40 || rightSlice.size() > 40)
				{
					int zzz = 0;
				}
				
				leftPart = toString(leftSlice);

				if (rightSlice.empty())
				{
					const WordPart* part = splitWordsStat.getOrAddWordPart(leftPart, WordPartSide::WholeWord);
					WordSeqUsage* stat = splitWordsStat.getOrAddWordSequence({ part->id() });
					stat->UsedCount++;
				}
				else
				{
					const WordPart* part1 = splitWordsStat.getOrAddWordPart(leftPart, WordPartSide::LeftPart);
					WordSeqUsage* stat1 = splitWordsStat.getOrAddWordSequence({ part1->id() });
					stat1->UsedCount++;

					rightPart = toString(rightSlice);

					const WordPart* part2 = splitWordsStat.getOrAddWordPart(rightPart, WordPartSide::RightPart);
					WordSeqUsage* stat2 = splitWordsStat.getOrAddWordSequence({ part2->id() });
					stat2->UsedCount++;
				}
			}
		}

		std::vector<const WordSeqUsage*> wordSeq;
		wordSeq.reserve(splitWordsStat.wordSeqCount());
		splitWordsStat.copyWordSeq(wordSeq);
		std::sort(std::begin(wordSeq), std::end(wordSeq), [](const WordSeqUsage* a, const WordSeqUsage* b)
		{
			return a->UsedCount > b->UsedCount;
		});

		QFile partsFreq("wordParts_freq.txt");
		if (!partsFreq.open(QIODevice::WriteOnly))
			return;
		QTextStream partsFreqStream(&partsFreq);
		for (const WordSeqUsage* wordStat : wordSeq)
		{
			int wordId = wordStat->Key.PartIds[0];
			const WordPart* wordPart = splitWordsStat.wordPartById(wordId);
			if (wordPart->partSide() == WordPartSide::RightPart)
				partsFreqStream << "~";

			partsFreqStream << QString::fromStdWString(wordPart->partText());

			if (wordPart->partSide() == WordPartSide::LeftPart)
				partsFreqStream << "~";

			partsFreqStream << " " << wordStat->UsedCount << endl;
		}
		std::wcout << "wrote successfully" << std::endl;
	}


	std::tuple<bool, const char*> saveWordStatisticsXml(const WordsUsageInfo& wordStats, wv::slice<WordSeqUsage*> wordSeqOrder, const std::wstring& filePath)
	{
		QFile file(QString::fromStdWString(filePath));
		if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
			return std::make_tuple(false, "Can't open file for writing");

		static const char* XmlDocName = "wordStat";
		const char* WordName = "word";
		const char* WordIdName = "id";
		const char* WordUsedCountName = "usedCount";

		QXmlStreamWriter xmlWriter(&file);
		xmlWriter.setCodec("utf-8");
		xmlWriter.setAutoFormatting(true);
		xmlWriter.setAutoFormattingIndent(3);

		xmlWriter.writeStartDocument("1.0");
		xmlWriter.writeStartElement(XmlDocName);

		QString wordQ;
		QString usedCountQ;
		for (const WordSeqUsage* wordSeq : wordSeqOrder)
		{
			PG_Assert2(wordSeq->Key.PartCount == 1, "Not implemented");

			xmlWriter.writeStartElement(WordName);

			const WordPart* wordPart = wordStats.wordPartById(wordSeq->Key.PartIds[0]);
			wordQ.setUtf16((ushort*)wordPart->partText().data(), (int)wordPart->partText().size());
			usedCountQ.setNum(wordSeq->UsedCount);

			xmlWriter.writeAttribute(WordIdName, wordQ);
			xmlWriter.writeAttribute(WordUsedCountName, usedCountQ);

			xmlWriter.writeEndElement(); // word
		}

		xmlWriter.writeEndElement(); // XmlDocName
		xmlWriter.writeEndDocument();

		return std::make_tuple(true, nullptr);
	}

	std::tuple<bool, const char*> loadWordStatisticsXml(const std::wstring& filePath, WordsUsageInfo& wordsStat, std::vector<const WordSeqUsage*>& wordSeqOrder)
	{
		QFile file(QString::fromStdWString(filePath));
		if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
			return std::make_tuple(false, "Can't open file");

		QXmlStreamReader xml;
		xml.setDevice(&file);

		// got to document root

		xml.readNextStartElement(); // document root

		//
		while (!xml.atEnd())
		{
			xml.readNext();
			if (xml.isStartElement() && xml.name() == "word")
			{
				QXmlStreamAttributes attrs = xml.attributes();
				QStringRef idRef = attrs.value("id");
				bool parseNum;
				int usedCount = attrs.value("usedCount").toInt(&parseNum);
				if (!parseNum)
					return std::make_tuple(false, "usedCount must be integer");

				QString wordQ = QString::fromRawData(idRef.constData(), idRef.size());
				std::wstring wordW((const wchar_t*)wordQ.utf16(), wordQ.size());

				bool wasAdded = false;
				const WordPart* wordPart = wordsStat.getOrAddWordPart(wordW, WordPartSide::WholeWord, &wasAdded);
				if (!wasAdded)
					return std::make_tuple(false, "Error: duplicate words in words statistic");
				
				WordSeqKey wordIds({ wordPart->id() });

				WordSeqUsage* wordUsage = wordsStat.getOrAddWordSequence(wordIds, &wasAdded);
				if (!wasAdded)
					return std::make_tuple(false, "Error: duplicate words in words statistic");

				wordUsage->UsedCount = usedCount;

				wordSeqOrder.push_back(wordUsage);
			}
		}
		if (xml.hasError())
			return std::make_tuple(false, "Error in XML parsing");

		return std::make_tuple(true, nullptr);
	}

	void tinkerWithPhoneticSplit(int argc, wchar_t* argv[])
	{
		const wchar_t* txtDir = LR"path(C:\devb\PticaGovorunProj\data\textWorld\)path";
		if (argc > 1)
			txtDir = argv[1];

		WordsUsageInfo wordsStat;
		std::vector<const WordSeqUsage*> wordSeqOrder; // order from most - least used

		const wchar_t* wordCountsPath = L"wordCounts.xml";
		if (QFile::exists(QString::fromWCharArray(wordCountsPath)))
		{
			bool loadOp;
			const char *errMsg;
			std::tie(loadOp, errMsg) = loadWordStatisticsXml(wordCountsPath, wordsStat, wordSeqOrder);
			if (!loadOp)
			{
				std::cerr << errMsg;
				return;
			}
		}
		else
		{
			typedef std::chrono::system_clock Clock;
			std::chrono::time_point<Clock> now1 = Clock::now();

			buildLangModel(txtDir, wordsStat);

			std::chrono::time_point<Clock> now2 = Clock::now();
			auto elapsedSec = std::chrono::duration_cast<std::chrono::seconds>(now2 - now1).count();
			std::wcout << "elapsedSec=" << elapsedSec << "s" << std::endl;

			std::wcout << "Lang model has " << wordsStat.wordSeqCount() << " words" << std::endl;

			wordSeqOrder.reserve(wordsStat.wordSeqCount());
			wordsStat.copyWordSeq(wordSeqOrder);
			std::sort(std::begin(wordSeqOrder), std::end(wordSeqOrder), [](const WordSeqUsage* a, const WordSeqUsage* b)
			{
				return a->UsedCount > b->UsedCount;
			});
			//std::wstringstream buf;
			//for (size_t i = 0; i < take; ++i)
			//{
			//	buf.str(L"");
			//	buf << L"i=" << i << L" " << toString(nodePtrs[i]->WordBounds[0]) <<" " <<nodePtrs[i]->UsedCount << std::endl;
			//	std::wcout <<buf.str();
			//	::OutputDebugStringW(buf.str().c_str());
			//}

			// skip incidental words
			//auto firstIncidentalIt = std::find_if(std::begin(nodePtrs), std::end(nodePtrs), [](WordSeqUsage* a)
			//{
			//	return a->UsedCount <= 1;
			//});
			//int usedWordsCount = std::distance(std::begin(nodePtrs), firstIncidentalIt);

			size_t usedWordsCount = wordSeqOrder.size();
			saveWordStatisticsXml(wordsStat, wv::make_view(wordSeqOrder.data(), usedWordsCount), wordCountsPath);
		}

		std::wcout << "Processing the word statistics" << std::endl;

		typedef std::chrono::system_clock Clock;
		std::chrono::time_point<Clock> now1 = Clock::now();

		doWordsPhoneticSplit(wordsStat, wordSeqOrder);

		std::chrono::time_point<Clock> now2 = Clock::now();
		auto elapsedSec = std::chrono::duration_cast<std::chrono::seconds>(now2 - now1).count();
		std::wcout << "words parts done in " << elapsedSec << "s" << std::endl;
	}

	void runMain(int argc, wchar_t* argv[])
	{
		tinkerWithPhoneticSplit(argc, argv);
	}
}