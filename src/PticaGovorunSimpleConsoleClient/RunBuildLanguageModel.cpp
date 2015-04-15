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
#include <boost/optional.hpp>

#include "TextProcessing.h"
#include "LangStat.h"
#include "PhoneticService.h"
#include "PticaGovorunCore.h"
#include "CoreUtils.h"

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
				int sepInd = phoneticSplitOfWord(word, nullptr);
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

		std::vector<WordSeqUsage*> wordSeq;
		wordSeq.reserve(splitWordsStat.wordSeqCount());
		splitWordsStat.copyWordSeq(wordSeq);
		std::sort(std::begin(wordSeq), std::end(wordSeq), [](WordSeqUsage* a, WordSeqUsage* b)
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
		std::cout << "wrote successfully" << std::endl;
		return;

		//// 
		//// find which pronounced words are not in parsed text
		//const wchar_t* persianDictPath = LR"path(C:\devb\PticaGovorunProj\data\TrainSphinx\SpeechModels\phoneticKnown.yml)path";
		//std::vector<PhoneticWord> phoneticDict;
		//bool loadPhoneDict;
		//const char* errMsg;
		//std::tie(loadPhoneDict, errMsg) = loadPhoneticDictionaryYaml(persianDictPath, phoneticDict);
		//if (!loadPhoneDict)
		//{
		//	std::cerr << "Can't load phonetic dictionary " << errMsg << std::endl;
		//	return;
		//}

		//QTextCodec* pTextCodec = QTextCodec::codecForName("windows-1251");
		//std::map<std::wstring, std::vector<Pronunc>> shrekkyPhoneticDict;
		//const wchar_t* shrekkyDic = LR"path(C:\devb\PticaGovorunProj\data\shrekky\shrekkyDic.voca)path";
		////const wchar_t* shrekkyDic = LR"path(C:\devb\PticaGovorunProj\data\phoneticDic\test1.txt)path";
		//bool loadOp;
		//std::tie(loadOp, errMsg) = loadPronunciationVocabulary2(shrekkyDic, shrekkyPhoneticDict, *pTextCodec);
		//if (!loadOp)
		//{
		//	std::cerr << "Can't load shrekky dict: " << errMsg << std::endl;
		//	return;
		//}


		//// words set=persian+shrekky
		//std::set<std::wstring> langModelWords;
		//for (const PhoneticWord& w : phoneticDict)
		//	langModelWords.insert(w.Word);
		//for (const auto& pair : shrekkyPhoneticDict)
		//{
		//	const std::wstring &word = pair.first;
		//	if (langModelWords.find(word) != std::end(langModelWords))
		//		continue;

		//	langModelWords.insert(word);
		//}

		////
		//int noLangStatWordsCount = 0;
		//for (const auto& word : langModelWords)
		//{
		//	auto wordStatIt = wordSeqToCount.find(word);
		//	if (wordStatIt == std::end(wordSeqToCount))
		//	{
		//		// the statistics for the word is not available
		//		::OutputDebugStringW(word.c_str());
		//		::OutputDebugStringW(L"\n");
		//		noLangStatWordsCount++;
		//	}
		//}

		//std::cout << "can't find LangModel statistics for " << noLangStatWordsCount << " words" << std::endl;
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
			PG_Assert(wordSeq->Key.PartCount == 1 && "Not implemented");

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

	std::tuple<bool, const char*> loadWordStatisticsXml(const std::wstring& filePath, WordsUsageInfo& wordsStat, std::vector<WordSeqUsage*>& wordSeqOrder)
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

	bool loadWordList(const std::wstring& filePath, std::unordered_set<std::wstring>& words)
	{
		QFile file(QString::fromStdWString(filePath));
		if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
			return false;

		QTextCodec* textCodec = QTextCodec::codecForName("utf-8");
		std::array<char, 1024> lineBuff;
		while (true)
		{
			int readBytes = file.readLine(lineBuff.data(), lineBuff.size());
			if (readBytes == -1) // EOF
				return true;
			QString word = textCodec->toUnicode(lineBuff.data(), readBytes).trimmed();
			words.insert(word.toStdWString());
		}

		return true;
	}

	namespace
	{
		template <typename T, size_t FixedSize>
		struct ShortArray
		{
			std::array<T, FixedSize> Array;
			size_t ActualSize;
		};
	}

	// Generates ARPA (or Doug Paul) format.
	// LM must contain </s>, otherwise Sphinx emits error on LM loading
	// ERROR: "ngram_search.c", line 221: Language model/set does not contain </s>, recognition will fail
	// LM must contain <s>, otherwise Sphinx emits error on samples decoding
	// ERROR: "ngram_search.c", line 1157 : Couldn't find <s> in first frame
	void generateArpaLanguageModel(const wchar_t* lmFilePath, wv::slice<const WordPart*> seedUnigrams, const UkrainianPhoneticSplitter& phoneticSplitter, const WordsUsageInfo& wordUsage)
	{
		struct NGramRow
		{
			ShortArray<const WordPart*, 2> WordParts;
			long UsageCounter = 0;
			boost::optional<float> LogProb;
			long BackOffCounter = 0;
			boost::optional<float> BackOffLogProb;
			NGramRow* LowOrderNGram = nullptr; // pointer to the one part shorter n-gram
		};

		//

		long unigramTotalUsageCounter = 0;
		for (const WordPart* wordPart : seedUnigrams)
		{
			WordSeqKey seqKey({ wordPart->id() });
			long seqUsage = wordUsage.getWordSequenceUsage(seqKey);
			unigramTotalUsageCounter += seqUsage;
		}

		const double LogProbMinusInf = -99; // Interpreted as log(0)

		// create unigrams

		std::vector<std::unique_ptr<NGramRow>> unigrams;
		unigrams.reserve(seedUnigrams.size());
		for (const WordPart* wordPart : seedUnigrams)
		{
			WordSeqKey seqKey({ wordPart->id() });
			long seqUsage = wordUsage.getWordSequenceUsage(seqKey);

			double prob = seqUsage / (double)unigramTotalUsageCounter;
			PG_Assert(prob >= 0 && prob <= 1);
			double logProb = std::log10(prob);

			// attempt to block isolated right parts
			// bad, sentences become shorter even more
			//if (wordPart->partSide() == WordPartSide::RightPart)
			//	logProb = LogProbMinusInf; 

			auto gram = std::make_unique<NGramRow>();
			gram->WordParts.ActualSize = 1;
			gram->WordParts.Array[0] = wordPart;
			gram->LogProb = logProb;
			unigrams.push_back(std::move(gram));
		}

		// assume that processed text covers P portion of the whole language
		const float TextStatisticCover = 0.1;
		//const float TextStatisticCover = 1;

		// note, the order of bigrams must be the same as for unigrams

		auto assertLeftBackOffImpossible = []() {};

		long unigramTotalBackOffCounter = 0;
		std::vector<NGramRow> bigrams;
		bigrams.reserve(unigrams.size());
		for (size_t uniIndex1 = 0; uniIndex1 < unigrams.size(); ++uniIndex1)
		{
			NGramRow& uni1 = *unigrams[uniIndex1];
			
			for (size_t uniIndex2 = 0; uniIndex2 < unigrams.size(); ++uniIndex2)
			{
				// assert: case uniIndex1 == uniIndex2 is allowed
				// (word1,word1) combinations is necessary to exclude impossible combination of word parts, eg (~R,~R)

				const NGramRow& uni2 = *unigrams[uniIndex2];

				// bigram (<s>,~Right) is impossible; handled via back off
				if (uni1.WordParts.Array[0] == phoneticSplitter.sentStartWordPart() &&
					uni2.WordParts.Array[0]->partSide() == WordPartSide::RightPart)
				{
					// as <s> is ordinary word, all possible bigrams must be discarded by enumeration
					NGramRow bigram;
					bigram.LowOrderNGram = &uni1;
					bigram.WordParts.ActualSize = 2;
					bigram.WordParts.Array[0] = uni1.WordParts.Array[0];
					bigram.WordParts.Array[1] = uni2.WordParts.Array[0];
					bigram.LogProb = LogProbMinusInf;
					bigrams.push_back(bigram);
					continue;
				}
				// bigram (Left~,</s>) is impossible; handled via back off
				if (uni1.WordParts.Array[0]->partSide() == WordPartSide::LeftPart &&
					uni2.WordParts.Array[0] == phoneticSplitter.sentEndWordPart())
				{
					assertLeftBackOffImpossible();
					continue;
				}

				// specific case to discard (</s>,*)
				// (</s>,<s>) is ok
				if (uni1.WordParts.Array[0] == phoneticSplitter.sentEndWordPart())
				{
					if (uni2.WordParts.Array[0] == phoneticSplitter.sentStartWordPart())
					{
						WordSeqKey seqKey({ uni1.WordParts.Array[0]->id(), uni2.WordParts.Array[0]->id() });
						long seqUsage = wordUsage.getWordSequenceUsage(seqKey);

						uni1.UsageCounter += seqUsage;

						NGramRow bigram;
						bigram.LowOrderNGram = &uni1;
						bigram.WordParts.ActualSize = 2;
						bigram.WordParts.Array[0] = uni1.WordParts.Array[0];
						bigram.WordParts.Array[1] = uni2.WordParts.Array[0];
						bigram.UsageCounter = seqUsage;
						bigrams.push_back(bigram);
					}

					// bigram (</s>,*) is not possible
					continue;
				}

				//
				if (uni1.WordParts.Array[0]->partSide() == WordPartSide::LeftPart)
				{
					// 2nd: Left or Whole are handled via back off probability
					if (uni2.WordParts.Array[0]->partSide() == WordPartSide::RightPart)
					{
						NGramRow bigram;
						bigram.LowOrderNGram = &uni1;
						bigram.WordParts.ActualSize = 2;
						bigram.WordParts.Array[0] = uni1.WordParts.Array[0];
						bigram.WordParts.Array[1] = uni2.WordParts.Array[0];

						WordSeqKey seqKey({ uni1.WordParts.Array[0]->id(), uni2.WordParts.Array[0]->id() });
						long seqUsage = wordUsage.getWordSequenceUsage(seqKey);
						if (seqUsage > 0)
						{
							uni1.UsageCounter += seqUsage;
							bigram.UsageCounter = seqUsage;
						}
						else
						{
							// prohibit impossible bigram (L~,~R) by enumeration
							bigram.LogProb = LogProbMinusInf;
						}
						bigrams.push_back(bigram);
					}
				}
				else if (uni1.WordParts.Array[0]->partSide() == WordPartSide::RightPart ||
					     uni1.WordParts.Array[0]->partSide() == WordPartSide::WholeWord)
				{
					if (uni2.WordParts.Array[0]->partSide() == WordPartSide::RightPart)
					{
						// prohibit such case
						NGramRow bigram;
						bigram.LowOrderNGram = &uni1;
						bigram.WordParts.ActualSize = 2;
						bigram.WordParts.Array[0] = uni1.WordParts.Array[0];
						bigram.WordParts.Array[1] = uni2.WordParts.Array[0];
						bigram.LogProb = LogProbMinusInf;
						bigrams.push_back(bigram);
					}
					else if (uni2.WordParts.Array[0]->partSide() == WordPartSide::LeftPart ||
						     uni2.WordParts.Array[0]->partSide() == WordPartSide::WholeWord)
					{
						WordSeqKey seqKey({ uni1.WordParts.Array[0]->id(), uni2.WordParts.Array[0]->id() });
						long seqUsage = wordUsage.getWordSequenceUsage(seqKey);
						if (seqUsage > 0)
						{
							uni1.UsageCounter += seqUsage;

							NGramRow bigram;
							bigram.LowOrderNGram = &uni1;
							bigram.WordParts.ActualSize = 2;
							bigram.WordParts.Array[0] = uni1.WordParts.Array[0];
							bigram.WordParts.Array[1] = uni2.WordParts.Array[0];
							bigram.UsageCounter = seqUsage;
							bigrams.push_back(bigram);
						}
					}
				}
			}
		}

		// set backoff probabilities
		for (std::unique_ptr<NGramRow>& uniPtr : unigrams)
		{
			NGramRow& uni = *uniPtr;
			PG_Assert(uni.WordParts.ActualSize == 1);
			// unigram (<s>)
			if (uni.WordParts.Array[0] == phoneticSplitter.sentStartWordPart())
			{
				//uni.LogProb = LogProbMinusInf; // start the new sentence the latest (the lowest priority)
				
				//double backOff = 0; // the first word of a sentence may be any word, not just specified one
				// double backOff = 1.4124; // from CMU LM for English
				double backOff = std::log10(1 - TextStatisticCover);
				uni.BackOffLogProb = backOff;
				continue;
			}
			// unigram (</s>)
			//if (uni.WordParts.Array[0] == phoneticSplitter.sentEndWordPart())
			//{
			//	// this back off value is ignored if there is no (</s>, *) bigram
			//	uni.BackOffLogProb = 0; // all prob are in this case
			//	//uni.BackOffLogProb = LogProbMinusInf; // all prob are described in bigrams
			//	continue;
			//}

			if (uni.WordParts.Array[0]->partSide() == WordPartSide::LeftPart)
			{
				// only right part can go after left part; other cases must be prohibited
				assertLeftBackOffImpossible();
				uni.BackOffLogProb = LogProbMinusInf;
			}
			else if (uni.WordParts.Array[0]->partSide() == WordPartSide::RightPart ||
				     uni.WordParts.Array[0]->partSide() == WordPartSide::WholeWord)
			{
				// allow any word composition if text statistic is not available for some word sequence
				//uni.BackOffLogProb = 0; // prob=100%
				uni.BackOffLogProb = std::log10(1 - TextStatisticCover);
			}
			else
			{
				PG_Assert(false);
				if (uni.BackOffLogProb != nullptr)
				{
					// back off > 0 means eg. L~ ~R which do not match
					//PG_Assert(gram.BackOffCounter == 0);
				}
				else
				{
					PG_Assert(uni.BackOffCounter > 0);
					double prob = uni.BackOffCounter / (double)unigramTotalBackOffCounter;
					prob *= (1 - TextStatisticCover);
					PG_Assert(prob > 0);
					double logProb = std::log10(prob);
					uni.BackOffLogProb = logProb;
				}
			}
		}
		for (NGramRow& gram : bigrams)
		{
			PG_Assert(gram.WordParts.ActualSize == 2);
			if (gram.LogProb == nullptr)
			{
				PG_Assert(gram.LowOrderNGram != nullptr);
				double prob = gram.UsageCounter / (double)gram.LowOrderNGram->UsageCounter;

				if (gram.WordParts.Array[0]->partSide() == WordPartSide::LeftPart)
				{
					// Everything is concentrated in the (L~,~R) pairs; backOff(L~)=-99
					// This way we prohibit (L~,~R) words, which are not enumerated - which is bad.
					// But we also prohibit (L~,L~) or (L~,W) - which is good.
					// Do not change probability!
				}
				else if (gram.WordParts.Array[0]->partSide() == WordPartSide::RightPart ||
					     gram.WordParts.Array[0]->partSide() == WordPartSide::WholeWord)
				{
					// backOff(uni1)=log(1-TextStatisticCover), so here we should reduce max probability to TextStatisticCover
					prob *= TextStatisticCover;
				}
				else if (gram.WordParts.Array[0] == phoneticSplitter.sentEndWordPart() ||
					     gram.WordParts.Array[0] == phoneticSplitter.sentStartWordPart())
				{
					
				}
				PG_Assert(prob >= 0 && prob <= 1);
				double logProb = std::log10(prob);
				gram.LogProb = logProb;
			}
		}

		QFile lmFile(QString::fromStdWString(lmFilePath));
		if (!lmFile.open(QIODevice::WriteOnly | QIODevice::Text))
			return;
		QTextStream dumpFileStream(&lmFile);
		dumpFileStream.setCodec("UTF-8");
		dumpFileStream << R"out(\data\)out" << "\n";
		dumpFileStream << "ngram 1=" << unigrams.size() << "\n";
		dumpFileStream << "ngram 2=" << bigrams.size() << "\n"; // +2 for sentence start/end

		auto printNGram = [&dumpFileStream](const NGramRow& gram)
		{
			dumpFileStream.setFieldWidth(6);
			dumpFileStream << gram.LogProb.get();
			dumpFileStream.setFieldWidth(0);
			dumpFileStream << " ";

			for (int partInd = 0; partInd < gram.WordParts.ActualSize; ++partInd)
			{
				const WordPart* oneWordPart = gram.WordParts.Array[partInd];
				printWordPart(oneWordPart, dumpFileStream);
				dumpFileStream << " ";
			}

			if (gram.BackOffLogProb != nullptr)
			{
				dumpFileStream.setFieldWidth(6);
				dumpFileStream << gram.BackOffLogProb.get();
				dumpFileStream.setFieldWidth(0);
				dumpFileStream << " ";
			}

			dumpFileStream << "\n";
		};

		dumpFileStream << "\n"; // blank line
		dumpFileStream << "\\1-grams:" << "\n";
		for (std::unique_ptr<NGramRow>& gram : unigrams)
			printNGram(*gram);

		dumpFileStream << "\n"; // blank line
		dumpFileStream << "\\2-grams:" << "\n";
		for (NGramRow& gram : bigrams)
			printNGram(gram);

		dumpFileStream << "\n"; // blank line
		dumpFileStream << R"out(\end\)out" << "\n";
	}

	void createPhoneticDictionary(const wchar_t* filePath, wv::slice<const WordPart*> seedWordParts, const UkrainianPhoneticSplitter& phoneticSplitter, const WordsUsageInfo& wordUsage)
	{
		QFile lmFile(QString::fromStdWString(filePath));
		if (!lmFile.open(QIODevice::WriteOnly | QIODevice::Text))
			return;

		QTextStream dumpFileStream(&lmFile);
		dumpFileStream.setCodec("UTF-8");

		std::vector<UkrainianPhoneId> phones;
		for (const WordPart* wordPart : seedWordParts)
		{
			if (wordPart == phoneticSplitter.sentStartWordPart() || wordPart == phoneticSplitter.sentEndWordPart())
				continue;

			printWordPart(wordPart, dumpFileStream);

			dumpFileStream << "\t";

			phones.clear();
			bool spellOp;
			const char* errMsg;
			std::tie(spellOp, errMsg) = spellWord(wordPart->partText(), phones);
			PG_Assert(spellOp);

			Pronunc pron;
			bool pronToStrOp = pronuncToStr(phones, pron);
			PG_Assert(pronToStrOp);
			dumpFileStream << QLatin1String(pron.StrDebug.c_str());

			dumpFileStream << "\n";
		}
	}

	void chooseSeedUnigrams(const UkrainianPhoneticSplitter& phoneticSplitter, int maxUnigramsCount, bool allowPhoneticWordSplit, std::vector<const WordPart*>& result)
	{
		const WordsUsageInfo& wordUsage = phoneticSplitter.wordUsage();
		
		std::vector<const WordPart*> allWordParts;
		wordUsage.copyWordParts(allWordParts);

		std::vector<const WordPart*> wordPartSureInclude;
		std::vector<const WordPart*> wordPartCandidates;
		std::vector<UkrainianPhoneId> phonesTmp;

		for (const WordPart* wordPart : allWordParts)
		{
			// include <s>, </s> and ~Right parts
			if (wordPart == phoneticSplitter.sentStartWordPart() ||
				wordPart == phoneticSplitter.sentEndWordPart() ||
				(allowPhoneticWordSplit && wordPart->partSide() == WordPartSide::RightPart))
			{
				wordPartSureInclude.push_back(wordPart);
				continue;
			}

			auto canSpellFun = [&wordUsage, &phoneticSplitter, &phonesTmp](const WordPart* part)
			{
				PG_Assert(part != phoneticSplitter.sentStartWordPart() && part != phoneticSplitter.sentEndWordPart());

				phonesTmp.clear();
				bool spellOp;
				const char* errMsg;
				const std::wstring& partTxt = part->partText();
				std::tie(spellOp, errMsg) = spellWord(partTxt, phonesTmp);
				return spellOp;
			};
			WordSeqKey seqKey({ wordPart->id() });
			long wordSeqUsage = wordUsage.getWordSequenceUsage(seqKey);
			bool isFreqUsage = wordSeqUsage >= 10;

			if (isFreqUsage && canSpellFun(wordPart))
			{
				wordPartCandidates.push_back(wordPart);
			}
		}

		//
		if (maxUnigramsCount > 0)
		{
			int takeUnigrams = maxUnigramsCount - wordPartSureInclude.size();
			if (takeUnigrams < 0)
				takeUnigrams = 0;

			// sort descending by UsageCount
			std::sort(wordPartCandidates.begin(), wordPartCandidates.end(), [&wordUsage](const WordPart* a, const WordPart* b)
			{
				WordSeqKey aSeqKey({ a->id() });
				const WordSeqUsage* aUsage = wordUsage.getWordSequence(aSeqKey);

				WordSeqKey bSeqKey({ b->id() });
				const WordSeqUsage* bUsage = wordUsage.getWordSequence(aSeqKey);

				return aUsage->UsedCount > bUsage->UsedCount;
			});

			// take N most used unigrams
			if (wordPartCandidates.size() > takeUnigrams)
				wordPartCandidates.resize(takeUnigrams);
		}

		wordPartCandidates.insert(wordPartCandidates.end(), wordPartSureInclude.begin(), wordPartSureInclude.end());

		// sort alphabetically
		std::sort(wordPartCandidates.begin(), wordPartCandidates.end(), [&wordUsage](const WordPart* a, const WordPart* b)
		{
			auto opLess = std::less<std::wstring>();
			return opLess(a->partText(), b->partText());
		});

		result.insert(result.end(), std::begin(wordPartCandidates), std::end(wordPartCandidates));
	}

	void testLoadDeclinationDict(int argc, wchar_t* argv[])
	{
		std::wstring targetWord;
		if (argc > 1)
			targetWord = std::wstring(argv[1]);

		wchar_t* processedWordsFile = LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk-done.txt)path";
		std::unordered_set<std::wstring> processedWords;
		if (!loadWordList(processedWordsFile, processedWords))
			return;

		auto dictPathArray = { 
			//LR"path(C:\devb\PticaGovorunProj\srcrep\build\x64\Debug\tmp2.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk01-а-вапно-вапнований).htm.20150228220948.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk02-вапнований-г≥берел≥н-г≥бернац≥€).htm.20150228213543.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk03-г≥бернац≥€-дипромон≥й-дипрофен).htm.20150228141620.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk04-дипрофен-запорошен≥сть-запорошенн€).htm.20150228220515.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk05-запорошенн€-≥онуванн€-≥онувати).htm.20150301145100.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk06-≥онувати-кластогенний-клатрат).htm.20150302001636.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk07-клатрат-макроцистис-макроцит).htm.20150316142905.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk08-макроцит-м'€шкурити-н).htm.20150325111235.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk09-н-нестравн≥сть-нестратиф≥кований).htm.20150301230606.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk10-нестратиф≥кований-однокенотронний-однокислотний)8.htm.20150301230840.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk11-однокислотний-поконанн€-поконати).htm.20150325000544.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk12-поконати-п'€ть-р).htm.20150408175213.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk13-р-р€цькувати-с).htm.20150407185103.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk14-с-строщити-строювати).htm.20150407185337.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk15-строювати-т€х-у).htm.20150407185443.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk16-у-чхун-ш).htm.20150301231717.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk17-ш-€щур€чий-end).htm.20150301232344.xml)path",
		};

		typedef std::chrono::system_clock Clock;
		std::chrono::time_point<Clock> now1 = Clock::now();

		std::unordered_map<std::wstring, std::unique_ptr<WordDeclensionGroup>> declinedWords;
		int declinationFileCount = 0;
		for (auto& dictPath : dictPathArray)
		{
			std::tuple<bool, const char*> loadOp = loadUkrainianWordDeclensionXml(dictPath, declinedWords);
			if (!std::get<0>(loadOp))
			{
				std::cerr << std::get<1>(loadOp) << std::endl;
				return;
			}
			OutputDebugStringW(dictPath);
			OutputDebugStringW(L"\n");
			declinationFileCount++;
			if (false && declinationFileCount <= 3)
				break;
		}

		std::chrono::time_point<Clock> now2 = Clock::now();
		auto elapsedSec = std::chrono::duration_cast<std::chrono::seconds>(now2 - now1).count();
		std::cout << "loaded declination dict in " << elapsedSec << "s wordGroupsCount=" << declinedWords.size() << std::endl;

		//
		now1 = Clock::now();

		int totalDeclWordsCount = -1;
		bool allowPhoneticWordSplit = false;
		UkrainianPhoneticSplitter phoneticSplitter;
		phoneticSplitter.setAllowPhoneticWordSplit(allowPhoneticWordSplit);
		phoneticSplitter.bootstrap(declinedWords, targetWord, processedWords, totalDeclWordsCount);
		now2 = Clock::now();
		elapsedSec = std::chrono::duration_cast<std::chrono::seconds>(now2 - now1).count();
		std::cout << "phonetic split of declinartion dict took=" << elapsedSec <<"s" << std::endl;

		WordsUsageInfo& wordUsage = phoneticSplitter.wordUsage();
		double uniquenessRatio = wordUsage.wordPartsCount() / (double)totalDeclWordsCount;
		std::wcout << L"totalDeclWordsCount=" << totalDeclWordsCount
			<< L" wordParts=" << wordUsage.wordPartsCount()
			<< L" uniquenessRatio=" << uniquenessRatio
			<< std::endl;

		const wchar_t* txtDir = LR"path(C:\devb\PticaGovorunProj\data\textWorld\)path";
		long totalPreSplitWords = 0;

		now1 = Clock::now();
		phoneticSplitter.buildLangModel(txtDir, totalPreSplitWords, -1, true);
		now2 = Clock::now();
		elapsedSec = std::chrono::duration_cast<std::chrono::seconds>(now2 - now1).count();
		std::cout << "buildLangModel took=" << elapsedSec << "s" << std::endl;

		for (int wordsPerSeq = 1; wordsPerSeq <= 2; ++wordsPerSeq)
		{
			long wordsPerNGram = phoneticSplitter.wordSeqCount(wordsPerSeq);
			std::wcout << L"wordsPerNGram[" << wordsPerSeq <<"]=" << wordsPerNGram << std::endl;
		}
		std::wcout << L" wordParts=" << wordUsage.wordPartsCount() << std::endl;

		phoneticSplitter.printSuffixUsageStatistics();

		//int maxUnigramsCount = 30000;
		int maxUnigramsCount = -1;
		std::vector<const WordPart*> seedUnigrams;
		chooseSeedUnigrams(phoneticSplitter, maxUnigramsCount, allowPhoneticWordSplit, seedUnigrams);
		std::wcout << L"seed unigrams count=" << seedUnigrams.size() << std::endl;

		//
		std::wstringstream arpaLMFileName;
		arpaLMFileName << "persianLM.";
		appendTimeStampNow(arpaLMFileName);
		arpaLMFileName << ".txt";
		generateArpaLanguageModel(arpaLMFileName.str().c_str(), seedUnigrams, phoneticSplitter, wordUsage);

		std::wstringstream phonDictFileName;
		phonDictFileName << "persianDic.";
		appendTimeStampNow(phonDictFileName);
		phonDictFileName << ".txt";
		createPhoneticDictionary(phonDictFileName.str().c_str(), seedUnigrams, phoneticSplitter, wordUsage);
	}

	void tinkerWithPhoneticSplit(int argc, wchar_t* argv[])
	{
		const wchar_t* txtDir = LR"path(C:\devb\PticaGovorunProj\data\textWorld\)path";
		if (argc > 1)
			txtDir = argv[1];

		WordsUsageInfo wordsStat;
		std::vector<WordSeqUsage*> wordSeqOrder; // order from most - least used

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
			std::cout << "elapsedSec=" << elapsedSec << "s" << std::endl;

			std::wcout << "Lang model has " << wordsStat.wordSeqCount() << " words" << std::endl;

			wordSeqOrder.reserve(wordsStat.wordSeqCount());
			wordsStat.copyWordSeq(wordSeqOrder);
			std::sort(std::begin(wordSeqOrder), std::end(wordSeqOrder), [](WordSeqUsage* a, WordSeqUsage* b)
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

		std::cout << "Processing the word statistics" << std::endl;

		typedef std::chrono::system_clock Clock;
		std::chrono::time_point<Clock> now1 = Clock::now();

		doWordsPhoneticSplit(wordsStat, wordSeqOrder);

		std::chrono::time_point<Clock> now2 = Clock::now();
		auto elapsedSec = std::chrono::duration_cast<std::chrono::seconds>(now2 - now1).count();
		std::cout << "words parts done in " << elapsedSec << "s" << std::endl;
	}


	void runMain(int argc, wchar_t* argv[])
	{
		//tinkerWithPhoneticSplit(argc, argv);

		testLoadDeclinationDict(argc, argv); 
	}
}