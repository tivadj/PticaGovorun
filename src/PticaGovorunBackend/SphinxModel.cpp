#include "SphinxModel.h"
#include <vector>
#include <set>
#include <map>
#include <random>
#include <chrono> // std::chrono::system_clock
#include <boost/format.hpp>
#include <boost/algorithm/string/join.hpp>
#include <QDir>
#include <QDateTime>
#include "SpeechProcessing.h"
#include "PhoneticService.h"
#include "WavUtils.h"
#include "ClnUtils.h"
#include "ArpaLanguageModel.h"
#include "LangStat.h"
#include "assertImpl.h"
#include "AppHelpers.h"
#include "G729If.h"
#include "XmlAudioMarkup.h"

namespace PticaGovorun
{
	std::wstring sphinxModelVersionStr(boost::wstring_view modelDir)
	{
		QDir modelDirQ(toQString(modelDir));
		if (!modelDirQ.cd("etc"))
			return L"";

		QString trainConfigPath = modelDirQ.absoluteFilePath("sphinx_train.cfg");
		QFile configFile(trainConfigPath);
		if (!configFile.open(QIODevice::ReadOnly))
			return L"";

		// find the line with Sphinx model base path

		// eg the line
		// $CFG_BASE_DIR = "C:/TrainSphinx/persian.v39";
		QString baseDirLine;
		QTextStream txt(&configFile);
		while (!txt.atEnd())
		{
			QString line = txt.readLine();
			int ind1 = line.indexOf("$CFG_BASE_DIR");
			if (ind1 != -1)
			{
				baseDirLine = line;
				break;
			}
		}

		if (baseDirLine.isNull())
			return L"";

		const QChar doubleQuoute('\"');
		int i1 = baseDirLine.indexOf(doubleQuoute);
		int i2 = baseDirLine.lastIndexOf(doubleQuoute);
		if (i1 == -1 || i2 == -1)
			return L"";

		QString baseDirath = baseDirLine.mid(i1 + 1, i2 - i1 - 1);
		QDir dirInfo(baseDirath);
		QString sphinxDirName = dirInfo.dirName(); // name of the deepest dir
		return sphinxDirName.toStdWString();
	}

	// Collects all phones used in the set of phonetic dictionaries.
	class PhoneAccumulator
	{
		std::unordered_set<PhoneId, IdWithDebugStrTraits<PhoneId>::hasher> phoneIdsSet_;
	public:
		void addPhonesFromPhoneticDict(const std::vector<PhoneticWord>& phoneticDictWords)
		{
			for (const PhoneticWord& word : phoneticDictWords)
			{
				for (const PronunciationFlavour& pron : word.Pronunciations)
					addPhones(pron.Phones);
			}
		}
		void addPhones(const std::vector<PhoneId>& phones)
		{
			for (PhoneId ph : phones)
				phoneIdsSet_.insert(ph);
		}
		void buildPhoneList(const PhoneRegistry& phoneReg, std::vector<std::string>& phones)
		{
			phones.clear();
			std::transform(phoneIdsSet_.begin(), phoneIdsSet_.end(), std::back_inserter(phones), [this,&phoneReg](PhoneId phId)
			{
				std::string result;
				bool getPhone = phoneToStr(phoneReg, phId, result);
				PG_Assert(getPhone);
				return result;
			});
			std::sort(phones.begin(), phones.end());
		}
		void copyPhoneList(std::function<void(PhoneId)> result)
		{
			for (const auto& ph : phoneIdsSet_) result(ph);
		}
	};

	// Finds the usage of pronCode using statistics of the corresponding word.
	void promoteWordsToPronCodes(UkrainianPhoneticSplitter& phoneticSplitter, const std::map<boost::wstring_view, PhoneticWord>& phoneticDict)
	{
		WordsUsageInfo& wordUsage = phoneticSplitter.wordUsage();

		// step1: insert pronIds
		for (const auto& pair : phoneticDict)
		{
			const PhoneticWord& word = pair.second;
			std::wstring wordW = toStdWString(word.Word);

			// find word usage
			long wordTimesUsed = -1;
			const WordPart* wordPart = wordUsage.wordPartByValue(wordW, WordPartSide::WholeWord);
			if (wordPart != nullptr)
			{
				// assert: wordPart is not denied
				WordSeqKey key = { wordPart->id() };
				wordTimesUsed = wordUsage.getWordSequenceUsage(key);
			}

			// propogate word usage to corresponding pronCodes

			for (const PronunciationFlavour& pron : word.Pronunciations)
			{
				bool wasAdded = false;
				WordPart* pronPart = wordUsage.getOrAddWordPart(toStdWString(pron.PronCode), WordPartSide::WholeWord, &wasAdded);

				// here we, kind of, create pronCode
				// eg. word1='b' expands into pronunciations PronCode1='B' and PronCode2='P'
				// we may deny word='p' and set it's removed flag to true
				// but PronCode2='p' should be allowed
				pronPart->setRemoved(false);

				// update usage
				if (wasAdded && wordTimesUsed != -1)
				{
					// evenly propogate word usage to pronIds
					WordSeqKey key = { pronPart->id() };
					WordSeqUsage* u = wordUsage.getOrAddWordSequence(key, &wasAdded);
					u->UsedCount = wordTimesUsed / (long)word.Pronunciations.size();
				}
			}
		}

		// propogate word usage on pronIds for bigram

		std::vector<const WordSeqUsage*> wordSeqUsages;
		wordUsage.copyWordSeq(wordSeqUsages);
		for (const WordSeqUsage* seqUsage : wordSeqUsages)
		{
			if (seqUsage->Key.PartCount != 2) // 2=bigram
				continue;

			int wordPart1Id = seqUsage->Key.PartIds[0];
			int wordPart2Id = seqUsage->Key.PartIds[1];

			const WordPart* wordPart1 = wordUsage.wordPartById(wordPart1Id);
			PG_Assert(wordPart1 != nullptr);

			const WordPart* wordPart2 = wordUsage.wordPartById(wordPart2Id);
			PG_Assert(wordPart2 != nullptr);

			// the wordPart may point to a word which can be expanded to pronId in multiple ways
			auto wordIt1 = phoneticDict.find(wordPart1->partText());
			auto wordIt2 = phoneticDict.find(wordPart2->partText());
			bool noExpansion = wordIt1 == phoneticDict.end() && wordIt2 == phoneticDict.end();
			if (noExpansion)
				continue;

			auto collectPronIds = [&phoneticDict, &wordUsage](const PhoneticWord& word, std::vector<const WordPart*>& parts)
			{
				for (const PronunciationFlavour& pron : word.Pronunciations)
				{
					const WordPart* pronPart = wordUsage.wordPartByValue(toStdWString(pron.PronCode), WordPartSide::WholeWord);
					PG_Assert2(pronPart != nullptr, "PronIds were added in step1");
					parts.push_back(pronPart);
				}
			};

			std::vector<const WordPart*> parts1;
			if (wordIt1 != phoneticDict.end())
			{
				const PhoneticWord& word1 = wordIt1->second;
				collectPronIds(word1, parts1);
			}
			else parts1.push_back(wordPart1);

			std::vector<const WordPart*> parts2;
			if (wordIt2 != phoneticDict.end())
			{
				const PhoneticWord& word2 = wordIt2->second;
				collectPronIds(word2, parts2);
			}
			else parts2.push_back(wordPart2);

			// evenly distribute usage across all pronIds
			long usagePerPronId = (long)seqUsage->UsedCount / (parts1.size() * parts2.size());
			if (usagePerPronId == 0) // keep min usage > 0
				usagePerPronId = 1;
			for (size_t i1 = 0; i1 < parts1.size(); ++i1)
				for (size_t i2 = 0; i2 < parts2.size(); ++i2)
				{
					const WordPart* p1 = parts1[i1];
					const WordPart* p2 = parts2[i2];

					WordSeqKey biKey = { p1->id(), p2->id() };
					bool wasAdded = false;
					WordSeqUsage* u = wordUsage.getOrAddWordSequence(biKey, &wasAdded);
					u->UsedCount = usagePerPronId;
				}
		}
	}

	void rejectDeniedWords(const std::unordered_set<std::wstring>& denyWords, WordsUsageInfo& wordUsage)
	{
		std::vector<WordPart*> allWordParts;
		wordUsage.copyWordParts(allWordParts);

		ptrdiff_t deniedWordsCounter = 0;

		for (WordPart* wordPart : allWordParts)
		{
			if (denyWords.find(wordPart->partText()) != std::end(denyWords))
			{
				// ignore deny words
				wordPart->setRemoved(true);
				deniedWordsCounter += 1;
				//std::wcout << L"Ignored denied word: " << wordPart->partText() << std::endl; // debug
			}
		}
		std::wcout << L"Ignored denied words: " << deniedWordsCounter << std::endl; // info
	}

	/// Maps a name of each pronCode to Sphinx-complint name (base name plus other pronCodes with parenthesis).
	void mapPronCodeToSphinxName(const PhoneticWord& word, PronCodeToDisplayNameMap& wordToPronCodeSphinxMapping)
	{
		bool requireMapping = word.Pronunciations.size() > 1 ||
			word.Pronunciations.size() == 1 && word.Word != word.Pronunciations[0].PronCode; // word is different from it's unique pronCode
		if (!requireMapping)
			return;

		std::wstring displayNameBuf;
		std::vector<std::pair<boost::wstring_view, boost::wstring_view>> pronCodeToSphinxNameMap;

		// (word=abup, p1=abup, p2=abub, p3=apup) maps to (abup, abup(2), abup3(3))
		int dispName = 0;
		for (const PronunciationFlavour& p : word.Pronunciations)
		{
			dispName += 1;
			boost::wstring_view pronCode = p.PronCode;

			displayNameBuf.clear();
			displayNameBuf.append(word.Word.data(), word.Word.size()); // Sphinx base name is the word itself

			// Sphinx distinguishes pronCodes by digit in parenthesis
			if (dispName > 1)
			{
				std::array<wchar_t, 8> numBuf;
				int numSize = swprintf(numBuf.data(), numBuf.size(), L"%d", dispName);

				displayNameBuf.append(1, L'(');
				displayNameBuf.append(numBuf.data(), numSize);
				displayNameBuf.append(1, L')');
			}

			boost::wstring_view sphinxName;
			bool op = registerWord<wchar_t>(displayNameBuf, *wordToPronCodeSphinxMapping.stringArena_, sphinxName);
			PG_Assert2(op, "Can't allocate word in arena");

			pronCodeToSphinxNameMap.push_back(std::make_pair(pronCode, sphinxName));
		}

		wordToPronCodeSphinxMapping.map[word.Word] = std::move(pronCodeToSphinxNameMap);
	}

	/// Build pronCode to display name.
	bool buildPronCodeToDisplayName(const PronCodeToDisplayNameMap& pronCodeToSphinxMappingTrain, std::map<boost::wstring_view, boost::wstring_view>& result, ErrMsgList* errMsg)
	{
		// TODO: this code assumes many-one for pronCode-word relation, but really pronCode may be associated with many words
		int errsNum = 0;
		std::string errMsgStr;
		for (const std::pair<boost::wstring_view, std::vector<std::pair<boost::wstring_view, boost::wstring_view>>>& wordToPronsMap : pronCodeToSphinxMappingTrain.map)
		{
			boost::wstring_view word = wordToPronsMap.first;
			const std::vector<std::pair<boost::wstring_view, boost::wstring_view>>& wordMap = wordToPronsMap.second;

			for (const std::pair<boost::wstring_view, boost::wstring_view>& pronMap : wordMap)
			{
				boost::wstring_view pronName = pronMap.first;
				boost::wstring_view dispName = pronMap.second;

				auto pronNameIt = result.find(pronName);
				if (pronNameIt != std::end(result))
				{
					if (errMsg != nullptr)
					{
						if (errsNum == 0)
							errMsgStr = "pronCode with the same name already exist: ";
						errMsgStr += toUtf8StdString(pronName) + " ";
						errsNum += 1;
					}
				}
				result[pronName] = dispName;
			}
		}
		bool hasErr = errsNum > 0;
		if (hasErr)
		{
			PG_DbgAssert(!errMsgStr.empty());
			pushErrorMsg(errMsg, errMsgStr);
			return false;
		}
		return true;
	}

	bool SphinxTrainDataBuilder::chooseSeedUnigramsNew(const UkrainianPhoneticSplitter& phoneticSplitter, int minWordPartUsage, int maxUnigramsCount, bool allowPhoneticWordSplit,
		const PhoneRegistry& phoneReg,
		std::function<auto (boost::wstring_view word) -> const PhoneticWord*> expandWellFormedWord,
		const std::set<PhoneId>& trainPhoneIds, 
		WordPhoneticTranscriber& phoneticTranscriber,
		ResourceUsagePhase phase,
		std::vector<PhoneticWord>& result, ErrMsgList* errMsg)
	{
		const WordsUsageInfo& wordUsage = phoneticSplitter.wordUsage();

		std::vector<const WordPart*> allWordParts;
		wordUsage.copyWordParts(allWordParts);

		// push sure words from manual phonetic dictionaries
		std::vector<PhoneticWord> wordPartSureInclude;
		std::set<boost::wstring_view> chosenPronCodes;
		
		// here we do not include fillers (<sil>,[sp]) because they are not used in phonetic dictionary
		// and only <s> and </s> are used in arpa LM (which were syntatically added when processing raw text)
		//auto sentStartIt = std::find_if(std::begin(speechData_->phoneticDictFillerWords_), std::end(speechData_->phoneticDictFillerWords_), [](auto& w) { return w.Word == fillerStartSilence(); });
		//PG_Assert(sentStartIt != std::end(speechData_->phoneticDictFillerWords_));
		//wordPartSureInclude.push_back(*sentStartIt);
		//
		//auto sentEndIt = std::find_if(std::begin(speechData_->phoneticDictFillerWords_), std::end(speechData_->phoneticDictFillerWords_), [](auto& w) { return w.Word == fillerEndSilence(); });
		//PG_Assert(sentEndIt != std::end(speechData_->phoneticDictFillerWords_));
		//wordPartSureInclude.push_back(*sentEndIt);
		//auto addSyntWords = [&](gsl::span<FillerWord> words, bool mandatory, ErrMsgList* errMsg) -> bool
		//{
		//	for (const FillerWord& word : words)
		//	{
		//		PhoneticWord* phWord = nullptr;
		//		auto it = speechData_->phoneticDictFiller_.find(word.Word);
		//		if (it != std::end(speechData_->phoneticDictFiller_))
		//			phWord = &it->second;

		//		if (phWord == nullptr)
		//		{
		//			// try known dictionary
		//			it = speechData_->phoneticDictWellFormed_.find(word.Word);
		//			if (it != std::end(speechData_->phoneticDictWellFormed_))
		//				phWord = &it->second;
		//		}
		//		if (phWord == nullptr)
		//		{
		//			if (!mandatory)
		//				continue;
		//			else
		//			{
		//				pushErrorMsg(errMsg, str(boost::format("Can't find word %1% in WellFormed or Filler phonetic dictionary") % 
		//					toUtf8StdString(word.Word)));
		//				return false;
		//			}
		//		}
		//		wordPartSureInclude.push_back(*phWord);
		//	}
		//	return true;
		//};
		// add mandatory for arpa <s>, </s>
		//if (!addSyntWords(syntaticPronFillersAdHoc, true, errMsg))
		//	return false;
		//addSyntWords(syntaticProns, false, nullptr); // optional

		//
		// We may or may not use the annotated words for Test phase.
		// If annotated words are used then we need to somehow recover the statistics of their usage.
		// In this case we may theoretically expect from a decoder a 100% recognition rate.
		// If we avoid the annotated words in Test then the text statistics from text corpus may be used as is,
		// but decoder will not recognize words not covered by text corpus (100% recognition rate is not possible even in theory).
		static const bool useAnnotWords = true;
		if (phase == ResourceUsagePhase::Train || 
			phase == ResourceUsagePhase::Test && useAnnotWords)
		{
			std::map<boost::wstring_view, PhoneticWord> phonDict;
			mergePhoneticDictOnlyNew(phonDict, speechData_->phoneticDictWellFormedWords_);
			bool includeBrokenWords = phase == ResourceUsagePhase::Train;
			if (includeBrokenWords)
				mergePhoneticDictOnlyNew(phonDict, speechData_->phoneticDictBrokenWords_);
			//
			// words from Filler phonetic dictionary are not included by definition: they are not a subject to estimate probability of usage
			// <s>,</s> should be in arpa
			// <sil>,[sp] should not be in arpa

			for (const std::pair<boost::wstring_view, PhoneticWord> pair : phonDict)
			{
				const PhoneticWord& word = pair.second;

				// if well-formed pronunc=(word=безпеки, pron=беспеки) then text word=безпеки must not be included
				// if well-formed pronunc=(word=в, pron=в, pron=ф) then text word=ф must not be included
				chosenPronCodes.insert(word.Word);
				for (const PronunciationFlavour& pron : word.Pronunciations)
					chosenPronCodes.insert(pron.PronCode);

				wordPartSureInclude.push_back(word);
			}
		}

		//

		struct PhWordAndCounter
		{
			PhoneticWord PhWord;
			ptrdiff_t UsageCounter;
		};
		std::vector<PhWordAndCounter> wordPartCandidates;

		ptrdiff_t noPromotionCounter = 0;
		for (const WordPart* wordPart : allWordParts)
		{
			if (wordPart->removed())
			{
				noPromotionCounter += 1;
				continue;
			}
			// the word already included?
			auto sureIt = chosenPronCodes.find(wordPart->partText());
			if (sureIt != std::end(chosenPronCodes))
				continue;

			// <s> and </s> are already added as the words from Filler dictionary
			if (wordPart == phoneticSplitter.sentStartWordPart() ||
				wordPart == phoneticSplitter.sentEndWordPart())
			{
//				auto wordIt = phoneticDictFiller_.find(wordPart->partText());
//				PG_Assert(wordIt != std::end(phoneticDictFiller_));
//				const PhoneticWord& word = wordIt->second;
//				wordPartSureInclude.push_back(word);
				continue;
			}

			// the pronCode must expand in phones from the training set
			auto coverTrainPhonesOnlyFun = [](const std::vector<PhoneId>& pronPhones, const std::set<PhoneId>& trainPhoneIds)
			{
				return std::all_of(pronPhones.begin(), pronPhones.end(), [&trainPhoneIds](PhoneId phId) -> bool
				{
					return trainPhoneIds.find(phId) != trainPhoneIds.end();
				});
			};

			// add words which can be spelled with phones from active phone set
			auto spellFun = [&wordUsage, &phoneticSplitter, &phoneReg, &phoneticTranscriber](const WordPart* part, std::vector<PhoneId>& resultPhones) -> bool
			{
				PG_Assert(part != phoneticSplitter.sentStartWordPart() && part != phoneticSplitter.sentEndWordPart());

				bool spellOp;
				const char* errMsg;
				const std::wstring& partTxt = part->partText();

				phoneticTranscriber.transcribe(phoneReg, partTxt);
				if (phoneticTranscriber.hasError())
					return false;

				phoneticTranscriber.copyOutputPhoneIds(resultPhones);
				return true;
			};
			WordSeqKey seqKey({ wordPart->id() });
			long wordSeqUsage = wordUsage.getWordSequenceUsage(seqKey);
			bool isFreqUsage = minWordPartUsage == -1 || wordSeqUsage >= minWordPartUsage;

			std::vector<PhoneId> phonesTmp;
			if (isFreqUsage && spellFun(wordPart, phonesTmp))
			{
				bool knownPhones = coverTrainPhonesOnlyFun(phonesTmp, trainPhoneIds);
				if (knownPhones)
				{
					PhoneticWord word;
					word.Word = wordPart->partText();
					PronunciationFlavour pron;
					pron.PronCode = wordPart->partText();
					pron.Phones = std::move(phonesTmp);
					word.Pronunciations.push_back(pron);
					wordPartCandidates.push_back(PhWordAndCounter{ std::move(word), wordSeqUsage });
					continue;
				}
			}

			// ignore the word
			noPromotionCounter++;
			//std::wcout <<"no promotion Word->PronCode: " << wordPart->partText() << std::endl; // debug
		}

		std::wcout << "no promotion Word->PronCode: " << noPromotionCounter << std::endl;

		//
		if (maxUnigramsCount > 0)
		{
			int takeUnigrams = maxUnigramsCount - (int)wordPartSureInclude.size();
			if (takeUnigrams < 0)
				takeUnigrams = 0;

			// sort descending by UsageCount
			std::sort(wordPartCandidates.begin(), wordPartCandidates.end(), [&wordUsage](const PhWordAndCounter& a, const PhWordAndCounter& b)
			{
				return a.UsageCounter > b.UsageCounter;
			});

			// take N most used unigrams
			if (wordPartCandidates.size() > takeUnigrams)
				wordPartCandidates.resize(takeUnigrams);
		}

		std::transform(std::begin(wordPartCandidates), std::end(wordPartCandidates), std::back_inserter(wordPartSureInclude), [](PhWordAndCounter& a) -> PhoneticWord&& {
			return std::move(a.PhWord);
		});

		// sort lexicographically
		std::sort(wordPartSureInclude.begin(), wordPartSureInclude.end(), [&wordUsage](const PhoneticWord& a, const PhoneticWord& b)
		{
			return a.Word < b.Word;
		});

		result.insert(result.end(), std::begin(wordPartSureInclude), std::end(wordPartSureInclude));
		return true;
	}

	/// Constructs mapping from pronCode to it's display name in Sphinx .arpa, .dic and .transcript files.
	// Initial word with two pronCodes: (word=також, pronCode1=такош(1), pronCode2=такош(2)) will be mapped into three
	// pronCodes in the Sphinx phonetic dictionary file (.dic): (також, також(11), також(12))
	// The first name is a 'base name', two other pronCodes have the same word before parenthesis and are distinguished by number in parenthesis.
	// This method makes map for the word=також in the form of a list of pairs: (такош(1)-також(11), такош(2)-також(12)).
	// Sphinx library will output the base word=також if any of two pronCodes with parenthesis is recognized.
	void buildPronCodeToSphinxNameMap(
		const std::map<boost::wstring_view, PhoneticWord>& phoneticDictWellFormed, 
		const std::map<boost::wstring_view, PhoneticWord>& phoneticDictBroken,
		bool includeBrokenWords,
		PronCodeToDisplayNameMap& wordToPronCodeSphinxMapping)
	{
		// merge the set of words from wellFormed and broken words
		std::set<boost::wstring_view> wordList;
		for (const std::pair<boost::wstring_view, PhoneticWord>& pair : phoneticDictWellFormed)
			wordList.insert(pair.first);
		if (includeBrokenWords)
		{
			for (const std::pair<boost::wstring_view, PhoneticWord>& pair : phoneticDictBroken)
				wordList.insert(pair.first);
		}

		//
		std::vector<const PronunciationFlavour*> pronCodesPerWord;
		std::wstring displayNameBuf;

		for (boost::wstring_view word : wordList)
		{
			// lookup the broken dictionary
			const PhoneticWord* wellFormedWord = nullptr;
			const PhoneticWord* brokenWord = nullptr;

			auto wellFormedIt = phoneticDictWellFormed.find(word);
			if (wellFormedIt != std::end(phoneticDictWellFormed))
				wellFormedWord = &wellFormedIt->second;

			if (includeBrokenWords)
			{
				auto brokenIt = phoneticDictBroken.find(word);
				if (brokenIt != std::end(phoneticDictBroken))
					brokenWord = &brokenIt->second;
			}

			// merge well formed and broken pronCodes
			pronCodesPerWord.clear();
			if (wellFormedWord != nullptr)
				for (const PronunciationFlavour& p : wellFormedWord->Pronunciations)
					pronCodesPerWord.push_back(&p);

			if (brokenWord != nullptr)
				for (const PronunciationFlavour& p : brokenWord->Pronunciations)
					pronCodesPerWord.push_back(&p);

			bool requireMapping = pronCodesPerWord.size() > 1 ||
				pronCodesPerWord.size() == 1 && word != pronCodesPerWord[0]->PronCode; // word is different from it's unique pronCode
			if (!requireMapping)
				continue;

			// here we do not sort pronCodes, otherwise some pronCodes' display names in phonetic dictionary may become out of lexicographical order

			std::vector<std::pair<boost::wstring_view, boost::wstring_view>> pronCodeToSphinxNameMap;

			// (word=abup, p1=abup, p2=abub, p3=apup) maps to (abup, abup(2), abup3(3))
			int dispName = 0;
			for (const PronunciationFlavour* p : pronCodesPerWord)
			{
				dispName += 1;
				boost::wstring_view pronCode = p->PronCode;

				displayNameBuf.clear();
				displayNameBuf.append(word.data(), word.size());

				// Sphinx distinguishes pronCodes by digit in parenthesis
				if (dispName > 1)
				{
					std::array<wchar_t, 8> numBuf;
					int numSize = swprintf(numBuf.data(), numBuf.size(), L"%d", dispName);

					displayNameBuf.append(1, L'(');
					displayNameBuf.append(numBuf.data(), numSize);
					displayNameBuf.append(1, L')');
				}

				boost::wstring_view sphinxName;
				bool op = registerWord<wchar_t>(displayNameBuf, *wordToPronCodeSphinxMapping.stringArena_, sphinxName);
				PG_Assert2(op, "Can't allocate word in arena");

				pronCodeToSphinxNameMap.push_back(std::make_pair(pronCode, sphinxName));
			}

			wordToPronCodeSphinxMapping.map[word] = std::move(pronCodeToSphinxNameMap);
		}
	}

	bool SphinxTrainDataBuilder::run(ErrMsgList* errMsg)
	{
		// start timer
		typedef std::chrono::system_clock Clock;
		std::chrono::time_point<Clock> now1 = Clock::now();

		// call checks

		dbName_ = "persian";

		QDateTime generationDate = QDateTime::currentDateTime();
		std::string timeStamp;
		appendTimeStampNow(timeStamp);

		outDirPath_ = toBfs(AppHelpers::mapPath(QString("../../../data/TrainSphinx/SphinxModel_%1").arg(QString::fromStdString(timeStamp))));
		outDirPath_ = outDirPath_.normalize();
		if (!boost::filesystem::create_directories(outDirPath_))
		{
			pushErrorMsg(errMsg, str(boost::format("Can't create output directory (%1%)") % outDirPath_.string()));
			return false;
		}
		std::wcout << "outDir=" << outDirPath_.wstring() << std::endl;

		const char* ConfigSpeechModelDir = "speechProjDir";
		const char* ConfigRandSeed = "randSeed";
		const char* ConfigSwapTrainTestData = "swapTrainTestData";
		const char* ConfigIncludeBrownBear = "includeBrownBear";
		const char* ConfigGramDim = "gramDim";
		const char* ConfigOutputCorpus = "textWorld.outputCorpus";
		const char* ConfigRemoveSilenceAnnot = "removeSilenceAnnot";
		const char* ConfigTrainCasesRatio = "trainCasesRatio";
		const char* ConfigUseBrokenPronsInTrainOnly = "useBrokenPronsInTrainOnly";
		const char* ConfigAllowSoftHardConsonant = "allowSoftHardConsonant";
		const char* ConfigAllowVowelStress = "allowVowelStress";
		const char* ConfigMinWordPartUsage = "minWordPartUsage";
		const char* ConfigMaxUnigramsCount = "maxUnigramsCount";
		const char* ConfigAudOutputWav = "aud.outputWav";
		const char* ConfigAudCutSilenceVad = "aud.cutSilVad";
		const char* ConfigAudPadSilStart = "aud.padSilStart";
		const char* ConfigAudPadSilEnd = "aud.padSilEnd";
		const char* ConfigAudMinSilDurMs = "aud.minSilDurMs";
		const char* ConfigAudMaxNoiseLevelDb = "aud.maxNoiseLevelDb";

		speechProjDirPath_ = toBfs(AppHelpers::configParamQString(ConfigSpeechModelDir, "ERROR_path_does_not_exist"));
		speechProjDirPath_ = speechProjDirPath_.normalize();
		if (!boost::filesystem::exists(speechProjDirPath_))
		{
			pushErrorMsg(errMsg, "Specify the current speech model data (lang, acoustic) directory.");
			return false;
		}
		std::wcout << "speechProjDir=" << speechProjDirPath_.wstring() << std::endl;

		int randSeed = AppHelpers::configParamInt(ConfigRandSeed, 1932);
		bool swapTrainTestData = AppHelpers::configParamBool(ConfigSwapTrainTestData, false); // swaps train/test portions of data, so that opposite data can be tested when random generator's seed is fixed
		bool includeBrownBear = AppHelpers::configParamBool(ConfigIncludeBrownBear, false);
#if !(defined(PG_HAS_LIBSNDFILE) && defined(PG_HAS_SAMPLERATE))
		if (outputWav)
		{
			std::wcout << "Info: LibSndFile is not compiled in. Define PG_HAS_LIBSNDFILE C++ preprocessor directive";
			std::wcout << "Info: SampleRate is not compiled in. Define PG_HAS_SAMPLERATE C++ preprocessor directive";
			outputWav = false;
		}
#endif
		bool outputPhoneticDictAndLangModelAndTranscript = true;
		int gramDim = AppHelpers::configParamInt(ConfigGramDim, 2); // 1=unigram, 2=bigram
		bool outputCorpus = AppHelpers::configParamBool(ConfigOutputCorpus, false);
		bool removeSilenceAnnot = AppHelpers::configParamBool(ConfigRemoveSilenceAnnot, true); // remove silence (<sil>, [sp], _s) from audio annotation
		bool outputWav = AppHelpers::configParamBool(ConfigAudOutputWav, true);
		bool cutSilVad = AppHelpers::configParamBool(ConfigAudCutSilenceVad, true); // cut silence automatically detected by VAD
		bool padSilStart = AppHelpers::configParamBool(ConfigAudPadSilStart, true); // pad the audio segment with the silence segment
		bool padSilEnd = AppHelpers::configParamBool(ConfigAudPadSilEnd, true);
		int minSilDurMs = AppHelpers::configParamInt(ConfigAudMinSilDurMs, 300); // minimal duration (milliseconds) of flanked silence
		float maxNoiseLevelDb = (float)AppHelpers::configParamDouble(ConfigAudMaxNoiseLevelDb, 0); // (default 0) files with bigger noise level (dB) are ignored; set value=0 to ignore
		bool allowSoftHardConsonant = AppHelpers::configParamBool(ConfigAllowSoftHardConsonant, true); // true to use soft consonants (TS1) in addition to nomral consonants (TS)
		bool allowVowelStress = AppHelpers::configParamBool(ConfigAllowVowelStress, true); // true to use stressed vowels (A1) in addition to unstressed vowels (A)
		PalatalSupport palatalSupport = PalatalSupport::AsHard;
		bool useBrokenPronsInTrainOnly = true;
		const double trainCasesRatio = AppHelpers::configParamDouble(ConfigTrainCasesRatio, 0.7);
		const float outSampleRate = 16000; // Sphinx requires 16k

		// words with greater usage counter are inlcuded in language model
		// note, the positive threshold may evict the rare words
		// -1=to ignore the parameter
		int minWordPartUsage = 10;
		//int maxWordPartUsage = -1;
		//int maxUnigramsCount = 10000;
		int maxUnigramsCount = -1;

		//
		gen_.seed(randSeed);
		//
		static const size_t WordsCount = 200000;
		stringArena_ = std::make_shared<GrowOnlyPinArena<wchar_t>>(WordsCount * 6); // W*C, W words, C chars per word

		phoneReg_.setPalatalSupport(palatalSupport);
		initPhoneRegistryUk(phoneReg_, allowSoftHardConsonant, allowVowelStress);

		speechData_ = std::make_shared<SpeechData>(speechProjDirPath_);
		speechData_->setPhoneReg(std::shared_ptr<PhoneRegistry>(&phoneReg_, [](auto) {}));
		speechData_->setStringArena(stringArena_);

		if (!speechData_->Load(false, errMsg))
		{
			pushErrorMsg(errMsg, "Can't load speech data");
			return false;
		}

		// merge BrownBear dictionary
		QStringList errMsgList;
		if (includeBrownBear)
		{
			auto brownBearDictPath = speechData_->speechProjDir() / "PhoneticDict/phoneticBrownBear.xml";
			std::vector<PhoneticWord> phoneticDictWordsBrownBear;
			if (!loadPhoneticDictionaryXml(brownBearDictPath, phoneReg_, phoneticDictWordsBrownBear, *stringArena_, errMsg))
			{
				pushErrorMsg(errMsg, "Can't load phonetic dictionary");
				return false;
			}

			speechData_->mergePhoneticDictOnlyNew(phoneticDictWordsBrownBear);

			// check merged data, but avoid checking stress, because other dicts may not specify stress
			if (!speechData_->validate(false, &errMsgList))
			{
				pushErrorMsg(errMsg, toUtf8StdString(errMsgList.join('\n')));
				return false;
			}
		}
		else
		{
			if (!speechData_->validate(true, &errMsgList))
			{
				pushErrorMsg(errMsg, toUtf8StdString(errMsgList.join('\n')));
				return false;
			}
		}

		//
		if (!populatePronCodeToObjAndValidatePhoneticDictsHaveNoDuplicates(errMsg))
			return false;

		auto denyWordsFilePath = speechData_->speechProjDir() / "LM/denyWords.txt";
		std::unordered_set<std::wstring> denyWords;
		if (boost::filesystem::exists(denyWordsFilePath)) // optional list of denied words
		{
			if (!loadWordList(denyWordsFilePath, denyWords, errMsg))
			{
				pushErrorMsg(errMsg, "Can't load 'denyWords.txt' dictionary");
				return false;
			}
		}

		if (!validatePhoneticDictsHaveNoDeniedWords(denyWords, errMsg))
			return false;

		// load annotation
		
		auto speechWavRootDir = speechData_->speechProjDir() / "SpeechAudio";
		auto speechAnnotRootDir = speechData_->speechProjDir() / "SpeechAnnot";
		auto wavDirToAnalyze = speechData_->speechProjDir() / "SpeechAudio";

		if (!loadAudioAnnotation(speechWavRootDir, speechAnnotRootDir, wavDirToAnalyze, removeSilenceAnnot, padSilStart, padSilEnd, maxNoiseLevelDb, errMsg))
			return false;

		std::wcout << "Found annotated segments: " << segments_.size() << std::endl; // debug
		if (segments_.empty())
		{
			pushErrorMsg(errMsg, "There is no annotated audio");
			return false;
		}

		//
		std::vector<details::AssignedPhaseAudioSegment> phaseAssignedSegs;
		std::set<PhoneId> trainPhoneIds;
		if (!partitionTrainTestData(segments_, trainCasesRatio, swapTrainTestData, useBrokenPronsInTrainOnly, phaseAssignedSegs, trainPhoneIds, errMsg))
		{
			pushErrorMsg(errMsg, "Can't fix train/test partitioning");
			return false;
		}

		//
		auto audioOutDirPath =  outFilePath("/wav/");

		std::string dataPartNameTrain = dbName_ + "_train";
		std::string dataPartNameTest = dbName_ + "_test";

		auto outDirWavTrain = audioOutDirPath / dataPartNameTrain;
		auto outDirWavTest = audioOutDirPath / dataPartNameTest;

		//
		if (outputPhoneticDictAndLangModelAndTranscript)
		{
			bool allowPhoneticWordSplit = false;
			phoneticSplitter_.setAllowPhoneticWordSplit(allowPhoneticWordSplit);
			phoneticSplitter_.outputCorpus_ = outputCorpus;

			phoneticSplitter_.corpusFilePath_ = outDirPath_ / "corpusUtters.txt";

			// init sentence parser
			auto sentParser = std::make_shared<SentenceParser>(1024);
			auto abbrExp = std::make_shared<AbbreviationExpanderUkr>();
			abbrExp->stringArena_ = stringArena_;

			auto numDictPath = AppHelpers::mapPath("pgdata/LM_ua/numsCardOrd.xml").toStdWString();
			std::wstring errMsgW;
			if (!abbrExp->load(numDictPath, &errMsgW))
			{
				pushErrorMsg(errMsg, toUtf8StdString(errMsgW));
				pushErrorMsg(errMsg, "Can't load numbers dictionary.");
				return false;
			}
			sentParser->setAbbrevExpander(abbrExp);
			phoneticSplitter_.setSentParser(sentParser);

			//
			int maxFilesToProcess = AppHelpers::configParamInt("textWorld.maxFilesToProcess", -1);
			if (!phoneticSplitterLoad(phoneticSplitter_, maxFilesToProcess, errMsg))
				return false;

			// we call it before propogating pronCodes to words
			rejectDeniedWords(denyWords, phoneticSplitter_.wordUsage());

			//
			auto stressDictFilePath = speechData_->speechProjDir() / "LM/stressDictUk.xml";
			std::unordered_map<std::wstring, int> wordToStressedSyllable;
			if (boost::filesystem::exists(stressDictFilePath)) // optional stress syllable dictionary
			{
				if (!loadStressedSyllableDictionaryXml(stressDictFilePath, wordToStressedSyllable, errMsg))
					return false;
			}

			auto getStressedSyllableIndFun = [&wordToStressedSyllable](boost::wstring_view word, std::vector<int>& stressedSyllableInds) -> bool
			{
				auto it = wordToStressedSyllable.find(std::wstring(word.data(), word.size()));
				if (it == wordToStressedSyllable.end())
					return false;
				stressedSyllableInds.push_back(it->second);
				return true;
			};

			WordPhoneticTranscriber phoneticTranscriber;
			phoneticTranscriber.setStressedSyllableIndFun(getStressedSyllableIndFun); // augment pronunciations with stress

			//

			// propogate pronunciations from well formed words
			// broken words will be pulled before Train phase
			///promoteWordsToPronCodes(phoneticSplitter_, phoneticDictWellFormed_);

			PronCodeToDisplayNameMap pronCodeToSphinxMappingTest(stringArena_.get());
			buildPronCodeToSphinxNameMap(speechData_->phoneticDictWellFormed_, speechData_->phoneticDictBroken_, /*includeBrokenWords*/ false, pronCodeToSphinxMappingTest);
			std::wcout << "pronCode to Sphinx map Test: " << pronCodeToSphinxMappingTest.map.size() << std::endl;

			std::map<boost::wstring_view, boost::wstring_view> pronCodeToDisplayNameTest;
			if (!buildPronCodeToDisplayName(pronCodeToSphinxMappingTest, pronCodeToDisplayNameTest, errMsg))
				return false;

			static auto pronCodeDisplayHelper = [](const std::map<boost::wstring_view, boost::wstring_view>& pronCodeToDisplayName, boost::wstring_view pronCode)->boost::wstring_view
			{
				auto it = pronCodeToDisplayName.find(pronCode);
				if (it != std::end(pronCodeToDisplayName))
					return it->second;
				return boost::wstring_view();
			};
			auto pronCodeDisplayTest = [&pronCodeToDisplayNameTest](boost::wstring_view pronCode)->boost::wstring_view
			{
				return pronCodeDisplayHelper(pronCodeToDisplayNameTest, pronCode);
			};

			// now, well formed words and no broken words are used
			if (!buildPhaseSpecificParts(ResourceUsagePhase::Test, minWordPartUsage, maxUnigramsCount, allowPhoneticWordSplit, trainPhoneIds, gramDim, pronCodeDisplayTest, phoneticTranscriber, &dictWordsCountTest_, &phonesCountTest_, errMsg))
			{
				pushErrorMsg(errMsg, "Can't build Test phase specific parts.");
				return false;
			}

			// propogate also pronunciations from well formed words
			///promoteWordsToPronCodes(phoneticSplitter_, phoneticDictBroken_);

			PronCodeToDisplayNameMap pronCodeToSphinxMappingTrain(stringArena_.get());
			buildPronCodeToSphinxNameMap(speechData_->phoneticDictWellFormed_, speechData_->phoneticDictBroken_, /*includeBrokenWords*/ true, pronCodeToSphinxMappingTrain);
			std::wcout << "pronCode to Sphinx map Train: " << pronCodeToSphinxMappingTrain.map.size() << std::endl;

			std::map<boost::wstring_view, boost::wstring_view> pronCodeToDisplayNameTrain;
			if (!buildPronCodeToDisplayName(pronCodeToSphinxMappingTrain, pronCodeToDisplayNameTrain, errMsg))
				return false;

			auto pronCodeDisplayTrain = [&pronCodeToDisplayNameTrain](boost::wstring_view pronCode)->boost::wstring_view
			{
				return pronCodeDisplayHelper(pronCodeToDisplayNameTrain, pronCode);
			};

			// .dic and .arpa files are different
			// broken words goes in dict and lang model in train phase only
			// alternatively we can always ignore broken words, but it is a pity to not use the markup
			// now, well formed words and broken words are used
			if (!buildPhaseSpecificParts(ResourceUsagePhase::Train, minWordPartUsage, maxUnigramsCount, allowPhoneticWordSplit, trainPhoneIds, gramDim, pronCodeDisplayTrain, phoneticTranscriber, &dictWordsCountTrain_, &phonesCountTrain_, errMsg))
			{
				pushErrorMsg(errMsg, "Can't build Train phase specific parts.");
				return false;
			}

			// filler dictionary
			if (!writePhoneticDictSphinx(speechData_->phoneticDictFillerWords_, phoneReg_, outFilePath(dbName_ + ".filler"), nullptr, errMsg))
			{
				pushErrorMsg(errMsg, "Can't write Filler phonetic dictionary");
				return false;
			}

			// trasnscripts
			auto audioSrcRootDir = speechWavRootDir;

			// find where to put output audio segments
			fixWavSegmentOutputPathes(audioSrcRootDir, audioOutDirPath, ResourceUsagePhase::Train, outDirWavTrain, phaseAssignedSegs);
			fixWavSegmentOutputPathes(audioSrcRootDir, audioOutDirPath, ResourceUsagePhase::Test, outDirWavTest, phaseAssignedSegs);

			if (!writeFileIdAndTranscription(phaseAssignedSegs, ResourceUsagePhase::Train, outFilePath(dataPartNameTrain + ".fileids"), pronCodeDisplayTrain, outFilePath(dataPartNameTrain + ".transcription"), padSilStart, padSilEnd, errMsg))
				return false;
			if (!writeFileIdAndTranscription(phaseAssignedSegs, ResourceUsagePhase::Test, outFilePath(dataPartNameTest + ".fileids"), pronCodeDisplayTest, outFilePath(dataPartNameTest + ".transcription"), padSilStart, padSilEnd, errMsg))
				return false;
		}

		// write wavs

		if (outputWav)
		{
			if (!boost::filesystem::create_directories(outDirWavTrain) || !boost::filesystem::create_directories(outDirWavTest))
			{
				pushErrorMsg(errMsg, "Can't create wav train/test subfolder");
				return false;
			}
			if (!buildWavSegments(phaseAssignedSegs, outSampleRate, padSilStart, padSilEnd, minSilDurMs, cutSilVad, errMsg))
				return false;
		}

		// generate statistics
		generateDataStat(phaseAssignedSegs);

		// stop timer
		std::chrono::time_point<Clock> now2 = Clock::now();
		dataGenerationDurSec_ = std::chrono::duration_cast<std::chrono::seconds>(now2 - now1).count();

		std::map<std::string, QVariant> speechModelConfig;
		speechModelConfig[ConfigSpeechModelDir] = QVariant::fromValue(toQStringBfs(speechProjDirPath_));
		speechModelConfig[ConfigRandSeed] = QVariant::fromValue(randSeed);
		speechModelConfig[ConfigSwapTrainTestData] = QVariant::fromValue(swapTrainTestData);
		speechModelConfig[ConfigIncludeBrownBear] = QVariant::fromValue(includeBrownBear);
		speechModelConfig[ConfigGramDim] = QVariant::fromValue(gramDim);
		speechModelConfig[ConfigTrainCasesRatio] = QVariant::fromValue(trainCasesRatio);
		speechModelConfig[ConfigUseBrokenPronsInTrainOnly] = QVariant::fromValue(useBrokenPronsInTrainOnly);
		speechModelConfig[ConfigAllowSoftHardConsonant] = QVariant::fromValue(allowSoftHardConsonant);
		speechModelConfig[ConfigAllowVowelStress] = QVariant::fromValue(allowVowelStress);
		speechModelConfig[ConfigMinWordPartUsage] = QVariant::fromValue(minWordPartUsage);
		speechModelConfig[ConfigMaxUnigramsCount] = QVariant::fromValue(maxUnigramsCount);
		speechModelConfig[ConfigRemoveSilenceAnnot] = QVariant::fromValue(removeSilenceAnnot);
		speechModelConfig[ConfigAudOutputWav] = QVariant::fromValue(outputWav);
		speechModelConfig[ConfigAudPadSilStart] = QVariant::fromValue(padSilStart);
		speechModelConfig[ConfigAudPadSilEnd] = QVariant::fromValue(padSilEnd);
		speechModelConfig[ConfigAudMinSilDurMs] = QVariant::fromValue(minSilDurMs);
		if (!printDataStat(generationDate, speechModelConfig, outFilePath("dataStats.txt"), errMsg))
			return false;
		return true;
	}

	bool SphinxTrainDataBuilder::validatePhoneticDictsHaveNoDeniedWords(const std::unordered_set<std::wstring>& denyWords, ErrMsgList* errMsg)
	{
		// TODO: this check must be added to model's consistency checker (used in UI)
		// Consistency check: denied words can't be used in Test phonetic dictionary.
		// because of broken words are used in Train (not in Test) phonetic dictionary, the denied words should be searched
		// in well formed dictionary and not in broken words dictionary.
		// Train phonetic dictionary may contain broken words
		std::vector<std::reference_wrapper<PhoneticWord>> usedWords; // only well-formed words, no broken words
		usedWords.reserve(speechData_->phoneticDictWellFormedWords_.size());
		std::copy(std::begin(speechData_->phoneticDictWellFormedWords_), std::end(speechData_->phoneticDictWellFormedWords_), std::back_inserter(usedWords));
		//if (includeBrownBear)
		//	std::copy(std::begin(phoneticDictWordsBrownBear_), std::end(phoneticDictWordsBrownBear_), std::back_inserter(usedWords));

		std::vector<boost::wstring_view> usedDeniedWords;
		for (const PhoneticWord& word : usedWords)
		{
			if (denyWords.find(toStdWString(word.Word)) != std::end(denyWords))
			{
				usedDeniedWords.push_back(word.Word);
			}
		}

		if (!usedDeniedWords.empty())
		{
			std::wostringstream buf;
			buf << "Found used words which are denied: ";
			PticaGovorun::join(std::begin(usedDeniedWords), std::end(usedDeniedWords), boost::wstring_view(L", "), buf);
			pushErrorMsg(errMsg, toUtf8StdString(buf.str()));
			return false;
		}
		return true;
	}

	bool SphinxTrainDataBuilder::populatePronCodeToObjAndValidatePhoneticDictsHaveNoDuplicates(ErrMsgList* errMsg)
	{
		// make composite pronCode->pronObj map
		std::vector<boost::wstring_view> duplicatePronCodes;
		populatePronCodes(speechData_->phoneticDictWellFormedWords_, pronCodeToObjWellFormed_, duplicatePronCodes);
		populatePronCodes(speechData_->phoneticDictBrokenWords_, pronCodeToObjBroken_, duplicatePronCodes);
		populatePronCodes(speechData_->phoneticDictFillerWords_, pronCodeToObjFiller_, duplicatePronCodes);

		if (!duplicatePronCodes.empty())
		{
			QString errQ = "Phonetic dict contains duplicate pronCodes";
			for (boost::wstring_view pronCode : duplicatePronCodes)
				errQ += ", " + toQString(pronCode);
			pushErrorMsg(errMsg, toUtf8StdString(errQ));
			return false;
		}

		// duplicate pronCodes check is not done for BrownBear dict
		// duplicate pronCodes are ignored
//		if (includeBrownBear)
//			populatePronCodes(phoneticDictWordsBrownBear, pronCodeToObjWellFormed_, duplicatePronCodes);
		return true;
	}

	bool SphinxTrainDataBuilder::langModelRecoverUsageOfUnusedWords(const std::vector<PhoneticWord>& vocabWords, UkrainianPhoneticSplitter& phoneticSplitter, bool includeBrokenWords, std::map<int, ptrdiff_t>& wordPartIdToRecoveredUsage, ErrMsgList* errMsg)
	{
		WordsUsageInfo& wordUsage = phoneticSplitter.wordUsage();

		ptrdiff_t unigramTotalUsageCounter = wordsTotalUsage(wordUsage, vocabWords, nullptr);
		std::wcout << "unigramTotalUsageCounter (based on text corpus): " << unigramTotalUsageCounter << std::endl; // debug

		// The fillers which have usage initialized from text corpus (the ordinary fillers do not get into arpa langugae model).
		// The probability of usaged is leveraged when text corpus is emtpy.

		// calculate what probability space is reserved and what need to get minimal usage assigned
		static const int SmallUsage = 1;
		float reservedSpace = 0;
		ptrdiff_t recoveredUsage = 0;
		for (const PhoneticWord& phWord : vocabWords)
		{
			boost::wstring_view word = phWord.Word;
			WordPart* wordPart = wordUsage.wordPartByValue(toStdWString(word), WordPartSide::WholeWord);
			PG_Assert2(wordPart != nullptr, word.data());
			const WordSeqUsage* usage = wordUsage.getOrAddWordSequence(WordSeqKey{ wordPart->id() });
			if (usage->UsedCount == 0)
			{
				if (phWord.Log10ProbHint != boost::none)
				{
					float log10Prob = phWord.Log10ProbHint.value();
					float prob = std::pow(10, log10Prob);
					PG_Assert(prob >= 0 || prob <= 1);
					reservedSpace += prob;
				}
				else
				{
					recoveredUsage += SmallUsage;
				}
			}
		}

		PG_DbgAssert(reservedSpace < 1);
		std::wcout << "reare words to get assigned minimal usage: " << recoveredUsage << std::endl; // debug
		std::wcout << "reserved prob space for [inh], [eee] etc: " << reservedSpace << std::endl; // debug

		ptrdiff_t newTotalUsage = std::trunc<ptrdiff_t>((unigramTotalUsageCounter + recoveredUsage) / (1 - reservedSpace));
		std::wcout << "newTotalUsage: " << newTotalUsage << std::endl; // debug

		// recover usage of wordParts with unknown usage (those which where not covered in text corpus)
		auto recoverUsage = [&wordUsage, &wordPartIdToRecoveredUsage, newTotalUsage](const PhoneticWord& phWord)
		{
			boost::wstring_view word = phWord.Word;
			WordPart* wordPart = wordUsage.wordPartByValue(toStdWString(word), WordPartSide::WholeWord);
			PG_Assert2(wordPart != nullptr, word.data());
			WordSeqUsage* usage = wordUsage.getOrAddWordSequence(WordSeqKey{ wordPart->id() });
			if (usage->UsedCount == 0)
			{
				ptrdiff_t recoverUsage = -1;
				if (phWord.Log10ProbHint != boost::none)
				{
					float log10Prob = phWord.Log10ProbHint.value();
					float prob = std::pow(10, log10Prob);
					PG_Assert(prob >= 0 || prob <= 1);
					recoverUsage = std::trunc<ptrdiff_t>(prob * newTotalUsage);
				}
				else
				{
					// force some minimal usage
					recoverUsage = SmallUsage;
				}
				wordPartIdToRecoveredUsage[wordPart->id()] = recoverUsage;
			}
		};

		for (const PhoneticWord& word : vocabWords)
			recoverUsage(word);

#if PG_DEBUG
		auto totalUsageAfter = wordsTotalUsage(wordUsage, vocabWords, &wordPartIdToRecoveredUsage);
		std::wcout << "unigramTotalUsageCounter (with guessed usage of inh, eee words)= " << totalUsageAfter << std::endl;
#endif
		return true;
	}

	bool SphinxTrainDataBuilder::buildPhaseSpecificParts(ResourceUsagePhase phase, int minWordPartUsage, int maxUnigramsCount, bool allowPhoneticWordSplit, const std::set<PhoneId>& trainPhoneIds, int gramDim,
		std::function<auto (boost::wstring_view)->boost::wstring_view> pronCodeDisplay,
		WordPhoneticTranscriber& phoneticTranscriber,
		int* dictWordsCount, int* phonesCount, ErrMsgList* errMsg)
	{
		// well formed dictionaries are WellFormed, Broken and Filler
		auto expandWellFormedWordFun = [phase, this](boost::wstring_view word) -> const PhoneticWord*
		{
			// include broken words in train phase only
			// ignore broken words in test phase
			bool useBrokenDict = phase == ResourceUsagePhase::Train;
			return findWellKnownWord(word, useBrokenDict);
		};

		std::vector<PhoneticWord> seedPhoneticWords;
		bool includeBrokenWords = phase == ResourceUsagePhase::Train;
		if (!chooseSeedUnigramsNew(phoneticSplitter_, minWordPartUsage, maxUnigramsCount, allowPhoneticWordSplit, phoneReg_, expandWellFormedWordFun, trainPhoneIds, phoneticTranscriber, phase, seedPhoneticWords, errMsg))
			return false;

		std::wcout << "phase=" << (phase == ResourceUsagePhase::Train ? "train" : "test")
			<< " seed unigrams count=" << seedPhoneticWords.size() << std::endl;

		// build seed words (vocabulary) for arpa LM
		std::vector<PhoneticWord> seedWordsLangModel;
		buildLangModelSeedWords(seedPhoneticWords, seedWordsLangModel);;

		//
		boost::string_view fileSuffix = phase == ResourceUsagePhase::Train ? "_train" : "_test";
		std::vector<boost::wstring_view> wordsVoca(seedWordsLangModel.size());
		std::transform(std::begin(seedWordsLangModel), std::end(seedWordsLangModel), std::begin(wordsVoca), [](auto& w) { return w.Word; });
		if (!writeWordList(wordsVoca, outFilePath(str(boost::format("corpusVocab%1%.vocab") % fileSuffix)), errMsg))
			return false;

		std::map<int, ptrdiff_t> wordPartIdToRecoveredUsage;
		if (!langModelRecoverUsageOfUnusedWords(seedWordsLangModel, phoneticSplitter_, includeBrokenWords, wordPartIdToRecoveredUsage, errMsg))
			return false;

		//
		std::wcout << "Generating Arpa LM" << std::endl;
		ArpaLanguageModel langModel(gramDim);
		langModel.generate(seedWordsLangModel, wordPartIdToRecoveredUsage, phoneticSplitter_);

		//
		if (!writeArpaLanguageModel(langModel, outFilePath(str(boost::format("%1%%2%.arpa") % dbName_ % fileSuffix)), errMsg))
			return false;

		// phonetic dictionary
		std::vector<PhoneticWord> outPhoneticDict;
		buildPhoneticDictionaryNew(seedPhoneticWords, outPhoneticDict);

		// test phonetic dictionary should not have broken pronunciations
		if (phase == ResourceUsagePhase::Test)
		{
			std::vector<boost::wstring_view> brokenPronCodes;
			if (!rulePhoneticDicHasNoBrokenPronCodes(seedPhoneticWords, speechData_->phoneticDictBroken_, &brokenPronCodes))
			{
				std::wostringstream buf;
				buf << "Found broken pronunciations in phonetic dictionary: ";
				PticaGovorun::join(std::begin(brokenPronCodes), std::end(brokenPronCodes), boost::wstring_view(L", "), buf);
				
				pushErrorMsg(errMsg, toUtf8StdString(buf.str()));
				return false;
			}
		}

		if (dictWordsCount != nullptr)
			*dictWordsCount = outPhoneticDict.size();

		if (!writePhoneticDictSphinx(outPhoneticDict, phoneReg_, outFilePath(str(boost::format("%1%%2%.dic") % dbName_ % fileSuffix)), pronCodeDisplay, errMsg))
		{
			pushErrorMsg(errMsg, "Can't write phonetic dictionary");
			return false;
		}

		// create the list of used phones

		PhoneAccumulator phoneAcc;
		phoneAcc.addPhonesFromPhoneticDict(outPhoneticDict);
		phoneAcc.addPhonesFromPhoneticDict(speechData_->phoneticDictFillerWords_);

		std::vector<std::string> phoneList;
		phoneAcc.buildPhoneList(phoneReg_, phoneList);

		if (phonesCount != nullptr)
			*phonesCount = phoneList.size();

		// phone list contains all phones used in phonetic dictionary, including SIL
		if (!writePhoneList(phoneList, outFilePath(str(boost::format("%1%%2%.phone") % dbName_ % fileSuffix)), errMsg))
		{
			pushErrorMsg(errMsg, "Can't write phone list file");
			return false;
		}
		return true;
	}

	boost::filesystem::path SphinxTrainDataBuilder::outFilePath(const boost::filesystem::path& relFilePath) const
	{
		return outDirPath_ / relFilePath;
	}

	bool SphinxTrainDataBuilder::hasPhoneticExpansion(boost::wstring_view word, bool useBroken) const
	{
		auto it = pronCodeToObjWellFormed_.find(word);
		if (it != pronCodeToObjWellFormed_.end())
			return true;

		if (useBroken)
		{
			it = pronCodeToObjBroken_.find(word);
			if (it != pronCodeToObjBroken_.end())
				return true;
		}

		it = pronCodeToObjFiller_.find(word);
		if (it != pronCodeToObjFiller_.end())
			return true;

		return false;
	}

	bool SphinxTrainDataBuilder::isBrokenUtterance(boost::wstring_view text) const
	{
		std::vector<boost::wstring_view> textWords;
		GrowOnlyPinArena<wchar_t> arena(1024);
		splitUtteranceIntoPronuncList(text, arena, textWords);

		bool hasBrokenWord = std::any_of(textWords.begin(), textWords.end(), [this](boost::wstring_view word)
		{
			auto it = pronCodeToObjBroken_.find(word);
			return it != pronCodeToObjBroken_.end();
		});
		return hasBrokenWord;
	}

	const PronunciationFlavour* SphinxTrainDataBuilder::expandWellKnownPronCode(boost::wstring_view pronCode, bool useBrokenDict) const
	{
		auto it = pronCodeToObjWellFormed_.find(pronCode);
		if (it != pronCodeToObjWellFormed_.end())
			return &it->second;

		if (useBrokenDict)
		{
			it = pronCodeToObjBroken_.find(pronCode);
			if (it != pronCodeToObjBroken_.end())
				return &it->second;
		}

		it = pronCodeToObjFiller_.find(pronCode);
		if (it != pronCodeToObjFiller_.end())
			return &it->second;

		return nullptr;
	}

	const PhoneticWord* SphinxTrainDataBuilder::findWellKnownWord(boost::wstring_view word, bool useBrokenDict) const
	{
		auto it = speechData_->phoneticDictWellFormed_.find(word);
		if (it != speechData_->phoneticDictWellFormed_.end())
			return &it->second;

		if (useBrokenDict)
		{
			it = speechData_->phoneticDictBroken_.find(word);
			if (it != speechData_->phoneticDictBroken_.end())
				return &it->second;
		}

		it = speechData_->phoneticDictFiller_.find(word);
		if (it != speechData_->phoneticDictFiller_.end())
			return &it->second;

		return nullptr;
	}

	bool SphinxTrainDataBuilder::writePhoneList(const std::vector<std::string>& phoneList, const boost::filesystem::path& phoneListFile, ErrMsgList* errMsg) const
	{
		QFile file(toQStringBfs(phoneListFile));
		if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
		{
			pushErrorMsg(errMsg, str(boost::format("Can't open file for writing (%1%)") % phoneListFile.string()));
			return false;
		}

		QTextStream txt(&file);
		txt.setCodec("UTF-8");
		txt.setGenerateByteOrderMark(false);
		for (const std::string& ph : phoneList)
		{
			txt << QLatin1String(ph.c_str()) << "\n";
		}
		return true;
	}

	// Reads words, one word per line.
	// Lines with double-pound ## (anywhere in the line) are comments.
	// Text from single pound to the end of line is a comment.
	// Empty lines are ignored.
	bool loadWordList(const boost::filesystem::path& filePath, std::unordered_set<std::wstring>& words, ErrMsgList* errMsg)
	{
		QFile file(toQStringBfs(filePath));
		if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
		{
			pushErrorMsg(errMsg, str(boost::format("Can't open file for writing (%1%)") % filePath.string()));
			return false;
		}

		QTextCodec* textCodec = QTextCodec::codecForName("utf-8");
		std::array<char, 1024> lineBuff;
		while (true)
		{
			int readBytes = file.readLine(lineBuff.data(), lineBuff.size());
			if (readBytes == -1) // EOF
				break;
			QString line = textCodec->toUnicode(lineBuff.data(), readBytes);
			if (line.contains(QLatin1String("##"))) // line=comment, ignore entire line
				continue;

			QStringRef lineRef(&line);
			int poundInd = line.indexOf(QLatin1String("#"));
			if (poundInd != -1)
				lineRef = line.leftRef(poundInd);

			lineRef = lineRef.trimmed();
			if (lineRef.isEmpty())
				continue;

			QString word = lineRef.toString();
			words.insert(word.toStdWString());
		}
		return true;
	}

	bool writeWordList(const std::vector<boost::wstring_view>& words, boost::filesystem::path filePath, ErrMsgList* errMsg)
	{
		QFile file(toQStringBfs(filePath));
		if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
		{
			pushErrorMsg(errMsg, str(boost::format("Can't open file (%1%)") % filePath.string()));
			return false;
		}

		QTextStream txt(&file);
		txt.setCodec("UTF-8");
		txt.setGenerateByteOrderMark(false);
		for (const boost::wstring_view word : words)
		{
			txt << toQString(word) << "\n";
		}
		return true;
	}

	void SphinxTrainDataBuilder::loadDeclinationDictionary(std::unordered_map<std::wstring, std::unique_ptr<WordDeclensionGroup>>& declinedWordDict)
	{
		auto dictPathArray = {
			//LR"path(C:\devb\PticaGovorunProj\srcrep\build\x64\Debug\tmp2.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk01-а-вапно-вапнований).htm.20150228220948.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk02-вапнований-гіберелін-гібернація).htm.20150228213543.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk03-гібернація-дипромоній-дипрофен).htm.20150228141620.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk04-дипрофен-запорошеність-запорошення).htm.20150228220515.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk05-запорошення-іонування-іонувати).htm.20150301145100.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk06-іонувати-кластогенний-клатрат).htm.20150302001636.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk07-клатрат-макроцистис-макроцит).htm.20150316142905.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk08-макроцит-м'яшкурити-н).htm.20150325111235.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk09-н-нестравність-нестратифікований).htm.20150301230606.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk10-нестратифікований-однокенотронний-однокислотний)8.htm.20150301230840.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk11-однокислотний-поконання-поконати).htm.20150325000544.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk12-поконати-п'ять-р).htm.20150408175213.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk13-р-ряцькувати-с).htm.20150407185103.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk14-с-строщити-строювати).htm.20150407185337.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk15-строювати-тях-у).htm.20150407185443.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk16-у-чхун-ш).htm.20150301231717.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk17-ш-ящурячий-end).htm.20150301232344.xml)path",
		};

		std::chrono::time_point<Clock> now1 = Clock::now();

		int declinationFileCount = 0;
		for (auto& dictPath : dictPathArray)
		{
			std::wstring errMsg;
			bool loadOp = loadUkrainianWordDeclensionXml(dictPath, declinedWordDict, &errMsg);
			if (!loadOp)
			{
				std::wcerr << errMsg << std::endl;
				return;
			}
			std::wcout <<dictPath <<std::endl;
			declinationFileCount++;
			if (false && declinationFileCount <= 3)
				break;
		}
		std::chrono::time_point<Clock> now2 = Clock::now();
		auto elapsedSec = std::chrono::duration_cast<std::chrono::seconds>(now2 - now1).count();
		std::wcout << L"loaded declination dict in " << elapsedSec << L"s" << std::endl;
	}

	bool SphinxTrainDataBuilder::phoneticSplitterBootstrapOnDeclinedWords(UkrainianPhoneticSplitter& phoneticSplitter, ErrMsgList* errMsg)
	{
		if (!phoneticSplitter.allowPhoneticWordSplit())
			return true;

		std::unordered_map<std::wstring, std::unique_ptr<WordDeclensionGroup>> declinedWordDict;
		loadDeclinationDictionary(declinedWordDict);

		auto processedWordsFilePath = speechData_->speechProjDir() / "declinationDictUk/uk-done.txt";
		std::wcout << "processedWordsFile=" << processedWordsFilePath.wstring() << std::endl;

		std::unordered_set<std::wstring> processedWords;
		if (!loadWordList(processedWordsFilePath, processedWords, errMsg))
		{
			pushErrorMsg(errMsg, "Can't load 'uk-done.txt' dictionary");
			return false;
		}
		std::wcout << "processedWords.size=" << processedWords.size() << std::endl;

		std::wstring targetWord;

		std::chrono::time_point<Clock> now1 = Clock::now();
		phoneticSplitter.bootstrapFromDeclinedWords(declinedWordDict, targetWord, processedWords);
		std::chrono::time_point<Clock> now2 = Clock::now();
		auto elapsedSec = std::chrono::duration_cast<std::chrono::seconds>(now2 - now1).count();
		std::wcout << L"phonetic split of declinartion dict took=" << elapsedSec << L"s" << std::endl;

		int uniqueDeclWordsCount = uniqueDeclinedWordsCount(declinedWordDict);
		std::wcout << L"wordGroupsCount=" << declinedWordDict.size() << L" uniqueDeclWordsCount=" << uniqueDeclWordsCount << std::endl;

		WordsUsageInfo& wordUsage = phoneticSplitter.wordUsage();
		double uniquenessRatio = wordUsage.wordPartsCount() / (double)uniqueDeclWordsCount;
		std::wcout << L" wordParts=" << wordUsage.wordPartsCount() << std::endl;
		std::wcout << L" uniquenessRatio=" << uniquenessRatio << std::endl;
		return true;
	}

	void SphinxTrainDataBuilder::phoneticSplitterRegisterWordsFromPhoneticDictionary(UkrainianPhoneticSplitter& phoneticSplitter)
	{
		PG_Assert2(!phoneticSplitter.allowPhoneticWordSplit(), "Not implemented: only whole words are pulled from phonetic dictionary now");

		WordsUsageInfo& wordUsage = phoneticSplitter.wordUsage();

		auto registerWord = [&wordUsage](boost::wstring_view word)
		{
			bool wasAdded = false;
			WordPart* wordPart = wordUsage.getOrAddWordPart(toStdWString(word), WordPartSide::WholeWord, &wasAdded);
			PG_DbgAssert(wordPart != nullptr);
		};

		// register phonetic words as wordParts
		// the usage statistics of such words is unknown (eg [inh], [eee], ...)
		for (const PhoneticWord& word : speechData_->phoneticDictWellFormedWords_)
			registerWord(word.Word);
		for (const PhoneticWord& word : speechData_->phoneticDictBrokenWords_)
			registerWord(word.Word);
		for (const PhoneticWord& word : speechData_->phoneticDictFillerWords_)
			registerWord(word.Word);
	}

	void SphinxTrainDataBuilder::phoneticSplitterCollectWordUsageInText(UkrainianPhoneticSplitter& phoneticSplitter, int maxFilesToProcess)
	{
		auto txtDirPath = speechProjDirPath_ / "textWorld";
		std::wcout << "txtDir=" << txtDirPath << std::endl;
		long totalPreSplitWords = 0;

		std::chrono::time_point<Clock> now1 = Clock::now();
		phoneticSplitter.gatherWordPartsSequenceUsage(txtDirPath, totalPreSplitWords, maxFilesToProcess);
		std::chrono::time_point<Clock> now2 = Clock::now();
		auto elapsedSec = std::chrono::duration_cast<std::chrono::seconds>(now2 - now1).count();
		std::wcout << L"gatherWordPartsSequenceUsage took=" << elapsedSec << L"s" << std::endl;

		WordsUsageInfo& wordUsage = phoneticSplitter.wordUsage();
		std::wcout << L"number of word parts: " << wordUsage.wordPartsCount() << std::endl;

		std::array<ptrdiff_t, 2> wordSeqSizes;
		wordUsage.wordSeqCountPerSeqSize(wordSeqSizes);
		for (int seqDim = 0; seqDim < wordSeqSizes.size(); ++seqDim)
		{
			auto wordsPerNGram = wordSeqSizes[seqDim];
			std::wcout << L"text corpus has #" << seqDim + 1 << " words: " << wordsPerNGram << std::endl;
		}

		phoneticSplitter.printSuffixUsageStatistics();
	}

	bool SphinxTrainDataBuilder::phoneticSplitterLoad(UkrainianPhoneticSplitter& phoneticSplitter, int maxFilesToProcess, ErrMsgList* errMsg)
	{
		if (!phoneticSplitterBootstrapOnDeclinedWords(phoneticSplitter, errMsg))
			return false;
		phoneticSplitterRegisterWordsFromPhoneticDictionary(phoneticSplitter);
		phoneticSplitterCollectWordUsageInText(phoneticSplitter, maxFilesToProcess);
		return true;
	}

	void SphinxTrainDataBuilder::buildLangModelSeedWords(const std::vector<PhoneticWord>& seedUnigrams,
		std::vector<PhoneticWord>& result)
	{
		if (pgDebug())
		{
			auto sentStartItInput = std::find_if(std::begin(seedUnigrams), std::end(seedUnigrams), [](auto& phWord)
			{
				return phWord.Word == fillerStartSilence() || phWord.Word == fillerEndSilence();
			});
			PG_Assert2(sentStartItInput == std::end(seedUnigrams), "List of seed unigrams should not contain <s> and </s> as they are further added.");
		}

		result.reserve(seedUnigrams.size() + 2); // +2 for <s> and </s>

		// here we do not include fillers (<sil>,[sp]) because they are not used in phonetic dictionary
		// and only <s> and </s> are used in arpa LM (which were syntatically added when processing raw text)
		const auto& fillerDict = speechData_->phoneticDictFillerWords_;
		auto sentStartIt = std::find_if(std::begin(fillerDict), std::end(fillerDict), [](auto& w) { return w.Word == fillerStartSilence(); });
		PG_Assert(sentStartIt != std::end(fillerDict));
		result.push_back(*sentStartIt);
		
		auto sentEndIt = std::find_if(std::begin(fillerDict), std::end(fillerDict), [](auto& w) { return w.Word == fillerEndSilence(); });
		PG_Assert(sentEndIt != std::end(fillerDict));
		result.push_back(*sentEndIt);

		std::copy(std::begin(seedUnigrams), std::end(seedUnigrams), std::back_inserter(result));

		if (pgDebug())
		{
			// arpa has (<s>,</s>) which were syntatically added when parsing raw text
			auto it = std::find_if(std::begin(result), std::end(result), [](auto& w) { return w.Word == fillerStartSilence(); });
			PG_Assert2(it != std::end(result), "arpa has (<s>, </s>)")

				// arpa has no (<sil>, [sp])
				it = std::find_if(std::begin(result), std::end(result), [](auto& w) { return w.Word == fillerSilence(); });
			PG_Assert2(it == std::end(result), "arpa has no (<sil>, [sp])")
		}
	}

	void SphinxTrainDataBuilder::buildPhoneticDictionaryNew(const std::vector<PhoneticWord>& seedUnigrams,
		std::vector<PhoneticWord>& phoneticDictWords)
	{
		if (pgDebug())
		{
			// fillers should not be in seed words
			auto it = std::find_if(std::begin(seedUnigrams), std::end(seedUnigrams), [this](const PhoneticWord& word) -> bool
			{
				const auto& fillerDict = speechData_->phoneticDictFiller_;
				bool isFiller = fillerDict.find(word.Word) != std::end(fillerDict);
				return isFiller;
			});
			PG_Assert2(it == std::end(seedUnigrams), "There should be no fillers in seed unigrams");

		}
		// phonetic dictionary has none of (<s>. </s>, <sil>, [sp])
		// phonetic dictionary has [inh], [eee], [yyy]
		// if they are in WellFormed dictionary, then they have been included in seedUnigrams already
		// if they are in Filler dictionary, then here they should be added manually
		std::copy_if(std::begin(seedUnigrams), std::end(seedUnigrams), std::back_inserter(phoneticDictWords), [this](const PhoneticWord& word) -> bool
		{
			// .dic has no <s> and </s> words, TODO: why???
			//bool sentStart = word.Word.compare(phoneticSplitter_.sentStartWordPart()->partText()) == 0;
			//bool sentEnd   = word.Word.compare(phoneticSplitter_.sentEndWordPart()->partText()) == 0;
			//if (sentStart || sentEnd)
			//	return false;
			// filler words are in the dedicated dictionary, keep them out
			//const auto& fillerDict = speechData_->phoneticDictFiller_;
			//bool isFiller = fillerDict.find(word.Word) != std::end(fillerDict);
			//if (isFiller)
			//	return false;
			return true;
		});

		// post checks
		{
			auto it = std::find_if(std::begin(phoneticDictWords), std::end(phoneticDictWords), [](auto& w) { return w.Word.compare(fillerStartSilence()) == 0; });
			PG_Assert2(it == std::end(phoneticDictWords), "<s> should not be in phonetic dictionary")

			// [inh] is in filler dictionary or in phonetic dictionary with assigned dedicated phone
			//it = std::find_if(std::begin(speechData_->phoneticDictFillerWords_), std::end(speechData_->phoneticDictFillerWords_), [](auto& w) { return w.Word.compare(fillerInhale()) == 0; });
			//if (it != std::end(speechData_->phoneticDictFillerWords_))
			//{
			//	it = std::find_if(std::begin(phoneticDictWords), std::end(phoneticDictWords), [](auto& w) { return w.Word.compare(fillerInhale()) == 0; });
			//	PG_Assert2(it != std::end(phoneticDictWords), "[inh] should be in phonetic dictionary")
			//}
		}
	}
	
	bool SphinxTrainDataBuilder::createPhoneticDictionaryFromUsedWords(gsl::span<const WordPart*> seedWordParts, const UkrainianPhoneticSplitter& phoneticSplitter,
		std::function<auto (boost::wstring_view, PronunciationFlavour&) -> bool> expandPronCodeFun,
		std::vector<PhoneticWord>& phoneticDictWords, ErrMsgList* errMsg)
	{
		std::wstring wordStr;
		for (const WordPart* wordPart : seedWordParts)
		{
			if (wordPart == phoneticSplitter.sentStartWordPart() || wordPart == phoneticSplitter.sentEndWordPart())
				continue;

			toStringWithTilde(*wordPart, wordStr, false);

			boost::wstring_view wordRef = registerWordThrow2(*stringArena_, boost::wstring_view(wordStr));
			
			PronunciationFlavour pron;
			if (!expandPronCodeFun(wordPart->partText(), pron))
			{
				pushErrorMsg(errMsg, str(boost::format("Can't get pronunciation for word %1%") % toUtf8StdString(wordStr)));
				return false;
			}

			PhoneticWord word;
			word.Word = wordRef;
			word.Pronunciations.push_back(pron);
			phoneticDictWords.push_back(word);
		}
		return true;
	}

	bool SphinxTrainDataBuilder::loadAudioAnnotation(const boost::filesystem::path& wavRootDir, const boost::filesystem::path& annotRootDir, const boost::filesystem::path& wavDirToAnalyze,
		bool removeSilenceAnnot, bool padSilStart, bool padSilEnd, float maxNoiseLevelDb, ErrMsgList* errMsg)
	{
		// load audio segments
		auto segPredBeforeFun = [](const AnnotatedSpeechSegment& seg) -> bool
		{
			if (seg.TranscriptText.find(L"<unk>") != std::wstring::npos ||
				seg.TranscriptText.find(L"<air>") != std::wstring::npos ||
				seg.TranscriptText.find(L"[clk]") != std::wstring::npos)
				return false;
			
			if (seg.Language != SpeechLanguage::Ukrainian)
			{
				if (seg.TranscriptText.compare(fillerInhale().data()) == 0) // lang(inhale)=null, but we allow it
					return true;

				return false;
			}
			return true;
		};

		bool loadAudio = true;
		if (!loadSpeechAndAnnotation(QFileInfo(toQStringBfs(wavDirToAnalyze)), wavRootDir.wstring(), annotRootDir.wstring(), MarkerLevelOfDetail::Word, loadAudio, removeSilenceAnnot, padSilStart, padSilEnd, maxNoiseLevelDb, segPredBeforeFun, segments_, errMsg))
		{
			pushErrorMsg(errMsg, "Can't load audio and annotation.");
			return false;
		}
		return true;
	}

	bool SphinxTrainDataBuilder::partitionTrainTestData(const std::vector<AnnotatedSpeechSegment>& segments, double trainCasesRatio, bool swapTrainTestData, bool useBrokenPronsInTrainOnly, std::vector<details::AssignedPhaseAudioSegment>& outSegRefs, std::set<PhoneId>& trainPhoneIds, ErrMsgList* errMsg)
	{
		// partition train-test data
		std::vector<std::reference_wrapper<const AnnotatedSpeechSegment>> rejectedSegments;
		std::uniform_real_distribution<float> rnd(0, 1);
		for (const AnnotatedSpeechSegment& seg : segments)
		{
			bool isAlwaysTest = seg.ContentMarker.ExcludePhase == ResourceUsagePhase::Train;
			bool isAlwaysTrain = seg.ContentMarker.ExcludePhase == ResourceUsagePhase::Test;

			bool broken = isBrokenUtterance(seg.TranscriptText);
			if (useBrokenPronsInTrainOnly)
			{
				isAlwaysTrain |= broken;
			}
			// #includeBrownBear
			//isAlwaysTrain |= seg.ContentMarker.SpeakerBriefId == L"BrownBear1";

			if (isAlwaysTrain && isAlwaysTest)
			{
				// eg when excluded from training segment has broken words
				rejectedSegments.push_back(std::ref(seg));
				continue;
			}

			auto segRef = details::AssignedPhaseAudioSegment();
			segRef.Seg = &seg;

			if (isAlwaysTest)
				segRef.Phase = ResourceUsagePhase::Test;
			else if (isAlwaysTrain)
				segRef.Phase = ResourceUsagePhase::Train;
			else
			{
				PG_Assert(!isAlwaysTrain && !isAlwaysTest);

				float val = rnd(gen_);
				ResourceUsagePhase phase = val < trainCasesRatio ? ResourceUsagePhase::Train : ResourceUsagePhase::Test;

				if (swapTrainTestData)
				{
					if (phase == ResourceUsagePhase::Train)
						phase = ResourceUsagePhase::Test;
					else if (phase == ResourceUsagePhase::Test)
						phase = ResourceUsagePhase::Train;
				}

				segRef.Phase = phase;
			}
			outSegRefs.push_back(segRef);
		}
		if (!rejectedSegments.empty()) // TODO: print rejected segment: annot file path, segment IDs
			std::wcout << "Rejected segments: " << rejectedSegments.size() << std::endl; // info

		return putSentencesWithRarePhonesInTrain(outSegRefs, trainPhoneIds, errMsg);
	}

	// Fix train/test split so that if a (rare) phone exist only in one data set, then this is the Test data set.
	// This is necessary for the rare phones to end up in the train data set. Otherwise Sphinx emits warning and errors:
	// WARNING: "accum.c", line 633: The following senones never occur in the input data...
	// WARNING: "main.c", line 145: No triphones involving DZ1
	bool SphinxTrainDataBuilder::putSentencesWithRarePhonesInTrain(std::vector<details::AssignedPhaseAudioSegment>& segments, std::set<PhoneId>& trainPhoneIds, ErrMsgList* errMsg) const
	{
		PhoneAccumulator trainAcc;
		PhoneAccumulator testAcc;
		std::vector<boost::wstring_view> pronCodes;
		GrowOnlyPinArena<wchar_t> arena(1024);
		for (const auto& segRef : segments)
		{
			PhoneAccumulator& acc = segRef.Phase == ResourceUsagePhase::Train ? trainAcc : testAcc;
			
			pronCodes.clear();
			arena.clear();
			splitUtteranceIntoPronuncList(segRef.Seg->TranscriptText, arena, pronCodes);
			
			for (auto pronCode : pronCodes)
			{
				const PronunciationFlavour* pron = expandWellKnownPronCode(pronCode, true);
				if (pron == nullptr)
				{
					if (errMsg != nullptr) errMsg->utf8Msg = toUtf8StdString(str(boost::wformat(L"Can't map pronCode %1% into phonelist") % pronCode));
					return false;
				}
				acc.addPhones(pron->Phones);
			}
		}

		//
		trainAcc.copyPhoneList(putInsert(trainPhoneIds));
		
		std::vector<PhoneId> trainPhones;
		std::vector<PhoneId> testPhones;
		trainAcc.copyPhoneList(putBack(trainPhones));
		testAcc.copyPhoneList(putBack(testPhones));

		// phones which exist only in test portion but not in train
		std::vector<PhoneId> testOnlyPhoneIds;
		std::sort(trainPhones.begin(), trainPhones.end());
		std::sort(testPhones.begin(), testPhones.end());
		std::set_difference(testPhones.begin(), testPhones.end(), trainPhoneIds.begin(), trainPhoneIds.end(), std::back_inserter(testOnlyPhoneIds));

		//
		auto sentHasRarePhone = [&pronCodes, this](boost::wstring_view text, const std::vector<PhoneId>& rarePhoneIds) -> bool
		{
			pronCodes.clear();
			GrowOnlyPinArena<wchar_t> arena(1024);
			splitUtteranceIntoPronuncList(text, arena, pronCodes);

			return std::any_of(pronCodes.begin(), pronCodes.end(), [this, &rarePhoneIds](boost::wstring_view pronCode) -> bool
			{
				const PronunciationFlavour* pron = expandWellKnownPronCode(pronCode, true);
				PG_Assert2(pron != nullptr, "PronCode can't be expanded into phones, but has been successfully expanded before");
				
				return std::any_of(pron->Phones.begin(), pron->Phones.end(), [&rarePhoneIds](PhoneId phId) -> bool
				{
					return std::find(rarePhoneIds.begin(), rarePhoneIds.end(), phId) != rarePhoneIds.end();
				});
			});
		};

		// put sentences with testOnly (rare) phones into train portion
		int testToTrainMove = 0;
		for (PhoneId phoneId : testOnlyPhoneIds)
		{
			for (auto& segRef : segments)
			{
				if (segRef.Phase != ResourceUsagePhase::Test)
					continue;
				if (sentHasRarePhone(segRef.Seg->TranscriptText, testOnlyPhoneIds))
				{
					segRef.Phase = ResourceUsagePhase::Train;
					testToTrainMove++;
				}
			}
		}
		if (testToTrainMove > 0)
			std::wcout << L"Moved " << testToTrainMove << L" sentences with rare phones to Train portion" << std::endl; // info

		return true;
	}


	QString generateSegmentFileNameNoExt(const QString& baseFileName, const AnnotatedSpeechSegment& seg)
	{
		QLatin1Char filler('0');
		int width = 5;
		return QString("%1_%2-%3").arg(baseFileName).arg(seg.StartMarker.Id, width, 10, filler).arg(seg.EndMarker.Id, width, 10, filler);
	}

	// audioSrcRootDir   =C:\PG\data\SpeechAudio\
	// wavSrcFilePathAbs =C:\PG\data\SpeechAudio\ncru1-slovo\22229.WAV
	// wavBaseOutDir     =C:\PG\SphinxTrain\out123\wav\
	// wavDirForRelPathes=C:\PG\SphinxTrain\out123\wav\persian_train\
	// Output:
	// SegFileNameNoExt  =                                                       22229_00273-00032
	// WavOutRelFilePathNoExt=                         persian_train\ncru1-slovo\22229_00273-00032
	// WavOutFilePathNExt=C:\PG\SphinxTrain\out123\wav\persian_train\ncru1-slovo\22229_00273-00032
	details::AudioFileRelativePathComponents audioFilePathComponents(
		const QString& audioSrcRootDirPath,
		const QString& wavSrcFilePathAbs, const AnnotatedSpeechSegment& seg,
		const QString& wavBaseOutDirPath,
		const QString& wavDirPathForRelPathes)
	{
		QString wavFilePathRel = QDir(audioSrcRootDirPath).relativeFilePath(wavSrcFilePathAbs);

		// abs path to out wav
		QString wavOutAbsFilePath = QDir(wavDirPathForRelPathes).absoluteFilePath(wavFilePathRel);

		// rel path to out wav
		auto wavBaseOutDir = QDir(wavBaseOutDirPath);
		QString wavOutRelFilePath = wavBaseOutDir.relativeFilePath(wavOutAbsFilePath);

		// Sphinx FileId file name must have no extension, forward slashes
		QString segFileNameNoExt = generateSegmentFileNameNoExt(QFileInfo(wavOutRelFilePath).completeBaseName(), seg);
		QString wavOutRelFilePathNoExt = QFileInfo(wavOutRelFilePath).dir().filePath(segFileNameNoExt);
		QString wavOutFilePath = wavBaseOutDir.filePath(wavOutRelFilePathNoExt);

		details::AudioFileRelativePathComponents result;
		result.SegFileNameNoExt = segFileNameNoExt;
		result.WavOutRelFilePathNoExt = wavOutRelFilePathNoExt;
		result.AudioSegFilePathNoExt = wavOutFilePath;
		return result;
	}

	void SphinxTrainDataBuilder::fixWavSegmentOutputPathes(
		const boost::filesystem::path& audioSrcRootDirPath,
		const boost::filesystem::path& wavBaseOutDirPath,
		ResourceUsagePhase targetPhase,
		const boost::filesystem::path& wavDirPathForRelPathes,
		std::vector<details::AssignedPhaseAudioSegment>& outSegRefs)
	{
		for (details::AssignedPhaseAudioSegment& segRef : outSegRefs)
		{
			const AnnotatedSpeechSegment& seg = *segRef.Seg;
			if (segRef.Phase != targetPhase)
				continue;
			QString wavFilePath = QString::fromStdWString(seg.AudioFilePath);
			segRef.OutAudioSegPathParts = audioFilePathComponents(toQStringBfs(audioSrcRootDirPath), wavFilePath, seg, toQStringBfs(wavBaseOutDirPath), toQStringBfs(wavDirPathForRelPathes));
		}
	}

	bool SphinxTrainDataBuilder::writeFileIdAndTranscription(const std::vector<details::AssignedPhaseAudioSegment>& segsRefs, ResourceUsagePhase targetPhase,
		const const boost::filesystem::path& fileIdsFilePath,
		std::function<auto (boost::wstring_view)->boost::wstring_view> pronCodeDisplay,
		const const boost::filesystem::path& transcriptionFilePath,
		bool padSilStart, bool padSilEnd, ErrMsgList* errMsg)
	{
		QFile fileFileIds(toQStringBfs(fileIdsFilePath));
		if (!fileFileIds.open(QIODevice::WriteOnly | QIODevice::Text))
			return false;

		QFile fileTranscription(toQStringBfs(transcriptionFilePath));
		if (!fileTranscription.open(QIODevice::WriteOnly | QIODevice::Text))
			return false;
		
		QTextStream txtFileIds(&fileFileIds);
		txtFileIds.setCodec("UTF-8");
		txtFileIds.setGenerateByteOrderMark(false);

		QTextStream txtTranscription(&fileTranscription);
		txtTranscription.setCodec("UTF-8");
		txtTranscription.setGenerateByteOrderMark(false);

		QString startSilQ = toQString(fillerStartSilence());
		QString endSilQ = toQString(fillerEndSilence());

		std::vector<boost::wstring_view> pronCodes;
		GrowOnlyPinArena<wchar_t> arena(1024);
		for (const details::AssignedPhaseAudioSegment& segRef : segsRefs)
		{
			if (segRef.Phase != targetPhase)
				continue;

			// fileIds
			txtFileIds << segRef.OutAudioSegPathParts.WavOutRelFilePathNoExt << "\n";

			// determine if segment requires padding with silence
			bool startsSil = segRef.Seg->TranscriptionStartsWithSilence;
			bool endsSil = segRef.Seg->TranscriptionEndsWithSilence;

			PG_Assert(!startsSil);
			PG_Assert(!endsSil);

			// transcription
			if (padSilStart && targetPhase == ResourceUsagePhase::Train && !startsSil)
				txtTranscription << startSilQ << " ";
			
			// output TranscriptText mangling a pronCode if necessary (eg clothes(2)->clothes2)
			pronCodes.clear();
			arena.clear();
			splitUtteranceIntoPronuncList(segRef.Seg->TranscriptText, arena, pronCodes);
			for (size_t i = 0; i < pronCodes.size(); ++i)
			{
				boost::wstring_view pronCode = pronCodes[i];
				//sphinxPronCode.clear();
				//manglePronCodeSphinx(pronCode, sphinxPronCode);

				boost::wstring_view sphinxPronCode = pronCode;
				
				auto dispName = pronCodeDisplay(pronCode);
				if (dispName != boost::wstring_view())
					sphinxPronCode = dispName;

				txtTranscription << toQString(sphinxPronCode) << " ";
			}
			
			if (padSilEnd  && targetPhase == ResourceUsagePhase::Train && !endsSil)
				txtTranscription << endSilQ << " ";
			txtTranscription << "(" << segRef.OutAudioSegPathParts.SegFileNameNoExt << ")" << "\n";
		}
		return true;
	}

	bool SphinxTrainDataBuilder::buildWavSegments(const std::vector<details::AssignedPhaseAudioSegment>& segsRefs, float targetSampleRate, bool padSilStart, bool padSilEnd, float minSilDurMs, bool cutSilVad, ErrMsgList* errMsg)
	{
		// group segmets by source wav file, so wav files are read sequentially

		typedef std::vector<const details::AssignedPhaseAudioSegment*> VecPerFile;
		std::map<std::wstring, VecPerFile> srcFilePathToSegs;

		for (const details::AssignedPhaseAudioSegment& segRef : segsRefs)
		{
			auto it = srcFilePathToSegs.find(segRef.Seg->AudioFilePath);
			if (it != srcFilePathToSegs.end())
				it->second.push_back(&segRef);
			else
				srcFilePathToSegs.insert({ segRef.Seg->AudioFilePath, VecPerFile{ &segRef } });
		}

		// audio frames are lazy loaded
		float srcAudioSampleRate = -1;
		std::vector<short> srcAudioSamples;
		std::vector<short> segFramesResamp;
		std::vector<short> segFramesPad;
		std::vector<short> curFileAllSil;

		for (const auto& pair : srcFilePathToSegs)
		{
			const std::wstring& wavFilePath = pair.first;
			const VecPerFile& segs = pair.second;
			
			// load wav file
			std::string srcAudioPath = QString::fromStdWString(wavFilePath).toStdString();
			std::wcout << L"wav=" << wavFilePath << std::endl;

			srcAudioSamples.clear();
			if (!readAllSamplesFormatAware(srcAudioPath, srcAudioSamples, &srcAudioSampleRate, errMsg))
			{
				pushErrorMsg(errMsg, "Can't read audio file.");
				return false;
			}

			// prepare silence segments to pad speech segments not flanked with silence
			curFileAllSil.clear();
			{
				PG_Assert(!segs.empty())
				auto audioMarkupFilePathAbs = boost::filesystem::path(segs.front()->Seg->AnnotFilePath);
				SpeechAnnotation speechAnnot;
				if (!loadAudioMarkupXml(audioMarkupFilePathAbs, speechAnnot, errMsg))
					return false;

				QString silQ = toQString(fillerSilence());
				for (size_t i = 0; i<speechAnnot.markersSize(); ++i)
				{
					const TimePointMarker& marker = speechAnnot.marker(i);
					const TimePointMarker& next = speechAnnot.marker(i+1);
					if (marker.TranscripText == silQ)
					{
						gsl::span<const short> silSamples = srcAudioSamples;
						int len = next.SampleInd - marker.SampleInd;
						silSamples = silSamples.subspan(marker.SampleInd + static_cast<ptrdiff_t>(len * 0.3),
							static_cast<ptrdiff_t>(len * 0.4));
						std::copy(silSamples.begin(), silSamples.end(), std::back_inserter(curFileAllSil));
					}
				}
			}


			std::vector<boost::wstring_view> segPronCodes;
			std::vector<wchar_t> pronCodeBuff;

			// generate each audio segments
			for (const details::AssignedPhaseAudioSegment* segRef : segs)
			{
				const AnnotatedSpeechSegment& seg =  *segRef->Seg;

				// since each part may go to train or test output, ensure that output dir exist
				auto segOutDir = QFileInfo(segRef->OutAudioSegPathParts.AudioSegFilePathNoExt).dir();
				if (!segOutDir.exists())
					segOutDir.mkpath(".");

				//
				//long segLen = seg.EndMarker.SampleInd - seg.StartMarker.SampleInd;
				//PG_DbgAssert(segLen >= 0);
				//gsl::span<const short> segFramesNoPad = srcAudioSamples;
				//segFramesNoPad = segFramesNoPad.subspan(seg.StartMarker.SampleInd, segLen);;
				gsl::span<const short> segFramesNoPad = seg.Samples;

				// remove silence segments using VAD
				std::vector<short> samplesCutSil;
				if (!cutSilVad)
				{
					if (wavFilePath.find(L"BrownBear") != std::wstring::npos)
						cutSilVad = true;
				}
				if (cutSilVad)
				{
					static const float G729SampleRate = 8000;
					std::vector<short> samples8k;
					if (!resampleFrames(segFramesNoPad, srcAudioSampleRate, G729SampleRate, samples8k, errMsg))
						return false;

					std::vector<SegmentSpeechActivity> activity;
					if (!detectVoiceActivityG729(samples8k, G729SampleRate, activity, errMsg))
						return false;

					float sampleRatio = G729SampleRate / srcAudioSampleRate;

					for (const SegmentSpeechActivity& act : activity)
					{
						if (act.IsSpeech)
						{
							ptrdiff_t startSampleInd = act.StartSampleInd / sampleRatio;
							ptrdiff_t endSampleInd   = act.EndSampleInd / sampleRatio;

							auto speech = segFramesNoPad.subspan(startSampleInd, endSampleInd - startSampleInd);
							std::copy(speech.begin(), speech.end(), std::back_inserter(samplesCutSil));
						}
					}
					segFramesNoPad = samplesCutSil;
				}

				// determine whether to pad the utterance with silence
				bool startsSil = seg.AudioStartsWithSilence;
				bool endsSil = seg.AudioEndsWithSilence;

				// pad frames with silence
				segFramesPad.clear();

				long minSilLenFrames = static_cast<long>(minSilDurMs / 1000 * srcAudioSampleRate);
				
				auto chooseSil = [&](int len) -> gsl::span<const short>
				{
					PG_DbgAssert(len >= 0);
					PG_DbgAssert2(len <= curFileAllSil.size(), "Can't find silence for padding. Requested silence duration is longer than annotated silence in all file.");
					
					std::uniform_int_distribution<int> uni(0, curFileAllSil.size() - len);
					auto start = uni(gen_);
					gsl::span<const short> result = curFileAllSil;
					result = result.subspan(start, len);
					return result;
				};
				if (padSilStart) // start silence
				{
					auto existentSilSize = !startsSil ? 0 : std::min(minSilLenFrames, seg.StartSilenceFramesCount);
					
					auto extraSilLen = minSilLenFrames - existentSilSize;
					auto padSil = chooseSil(extraSilLen);

					std::copy(padSil.begin(), padSil.end(), std::back_inserter(segFramesPad));
				}

				std::copy(segFramesNoPad.begin(), segFramesNoPad.end(), std::back_inserter(segFramesPad));

				if (padSilEnd) // end silence
				{
					auto existentSilSize = !endsSil ? 0 : std::min(minSilLenFrames, seg.EndSilenceFramesCount);

					auto extraSilLen = minSilLenFrames - existentSilSize;
					auto padSil = chooseSil(extraSilLen);

					std::copy(padSil.begin(), padSil.end(), std::back_inserter(segFramesPad));
				}

				gsl::span<const short> segFramesOut = segFramesPad;

				// resample
				bool requireResampling = srcAudioSampleRate != targetSampleRate;
				if (requireResampling)
				{
					if (!resampleFrames(segFramesPad, srcAudioSampleRate, targetSampleRate, segFramesResamp, errMsg))
					{
						pushErrorMsg(errMsg, "Can't resample frames.");
						return false;
					}
					segFramesOut = segFramesResamp;
				}

				// statistics
				double durSec = segFramesPad.size() / (double)targetSampleRate; // speech + padded silence
				double& durCounter = segRef->Phase == ResourceUsagePhase::Train ? audioDurationSecTrain_ : audioDurationSecTest_;
				durCounter += durSec;

				double noPadDurSec = segFramesNoPad.size() / (double)targetSampleRate; // only speech (no padded silence)
				double& noPadDurCounter = segRef->Phase == ResourceUsagePhase::Train ? audioDurationNoPaddingSecTrain_ : audioDurationNoPaddingSecTest_;
				noPadDurCounter += noPadDurSec;

				speakerIdToAudioDurSec_[seg.ContentMarker.SpeakerBriefId] += durSec;

				// write output wav segment

				std::string wavSegOutPath = (segRef->OutAudioSegPathParts.AudioSegFilePathNoExt + ".wav").toStdString();
#if PG_HAS_LIBSNDFILE
				if (!writeAllSamplesWav(segFramesOut, targetSampleRate, wavSegOutPath, errMsg))
				{
					pushErrorMsg(errMsg, "Can't write output wav segment.");
					return false;
				}
#endif
			}
		}
		return true;
	}

	void SphinxTrainDataBuilder::generateDataStat(const std::vector<details::AssignedPhaseAudioSegment>& phaseAssignedSegs)
	{
		std::vector<boost::wstring_view> words;
		GrowOnlyPinArena<wchar_t> arena(1024);
		for (const auto& segRef : phaseAssignedSegs)
		{
			int& utterCounter = segRef.Phase == ResourceUsagePhase::Train ? utterCountTrain_ : utterCountTest_;
			utterCounter += 1;

			words.clear();
			arena.clear();
			splitUtteranceIntoPronuncList(segRef.Seg->TranscriptText, arena, words);

			int& wordCounter = segRef.Phase == ResourceUsagePhase::Train ? wordCountTrain_ : wordCountTest_;
			wordCounter += (int)words.size();
		}
	}

	bool SphinxTrainDataBuilder::printDataStat(QDateTime genDate, const std::map<std::string, QVariant> speechModelConfig, const boost::filesystem::path& statFilePath, ErrMsgList* errMsg)
	{
		QFile outFile(toQStringBfs(statFilePath));
		if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text))
		{
			pushErrorMsg(errMsg, str(boost::format("Can't open file for writing (%1)") % statFilePath.string()));
			return false;
		}

		QTextStream outStream(&outFile);
		outStream.setCodec("UTF-8");
		outStream.setGenerateByteOrderMark(false);

		const int TimePrec = 4;

		QString curDateStr = genDate.toString("dd.MM.yyyy HH:mm::ss");
		outStream << QString("generationDate=%1").arg(curDateStr) << "\n";
		outStream << QString("generationDurH=%1").arg(dataGenerationDurSec_ / 3600.0, 0, 'f', TimePrec) << "\n";

		// config parameters
		for (const auto& pair : speechModelConfig)
		{
			outStream << QString::fromStdString(pair.first) << "=" << pair.second.toString() << "\n";
		}

		outStream << "\n";
		outStream << QString("audioDurH(Train, Test)=(%1, %2)").arg(audioDurationSecTrain_ / 3600, 0, 'f', TimePrec).arg(audioDurationSecTest_ / 3600, 0, 'f', TimePrec) << "\n";
		outStream << QString("audioDurNoPaddingH=(%1, %2)").arg(audioDurationNoPaddingSecTrain_ / 3600, 0, 'f', TimePrec).arg(audioDurationNoPaddingSecTest_ / 3600, 0, 'f', TimePrec) << "\n";
		outStream << QString("utterCount=(%1, %2)").arg(utterCountTrain_).arg(utterCountTest_) << "\n";
		outStream << QString("wordCount=(%1, %2)").arg(wordCountTrain_).arg(wordCountTest_) << "\n";
		outStream << QString("distSize=(%1, %2)").arg(dictWordsCountTrain_).arg(dictWordsCountTest_) << "\n";
		outStream << QString("phonesCount=(%1, %2)").arg(phonesCountTrain_).arg(phonesCountTest_) << "\n";

		// audio duration per speaker, order hi-lo
		std::vector<std::wstring> speakerIds;
		std::transform(std::begin(speakerIdToAudioDurSec_), std::end(speakerIdToAudioDurSec_), std::back_inserter(speakerIds),
			[](const std::pair<std::wstring, double>& pair) { return pair.first; });
		std::sort(std::begin(speakerIds), std::end(speakerIds),
			[&](const std::wstring& a, const std::wstring& b) { return speakerIdToAudioDurSec_[a] > speakerIdToAudioDurSec_[b]; });

		outStream << "\n#speakerId hours (hi to lo)" << "\n";
		for (const std::wstring& speakerId : speakerIds)
		{
			double durSec = speakerIdToAudioDurSec_[speakerId];
			outStream << QString("%1 %2").arg(toQString(speakerId)).arg(durSec / 3600, 0, 'f', TimePrec) << "\n";
		}
		return true;
	}

	bool loadSphinxAudio(boost::wstring_view audioDir, const std::vector<std::wstring>& audioRelPathesNoExt, boost::wstring_view audioFileSuffix, std::vector<AudioData>& audioDataList, std::wstring* errMsg)
	{
		QString ext = toQString(audioFileSuffix);
		auto audioDirQ = QDir(toQString(audioDir));

		for (const std::wstring& audioPathNoExt : audioRelPathesNoExt)
		{
			QString absPath = audioDirQ.filePath(QString("%1.%2").arg(toQString(audioPathNoExt)).arg(ext));

			AudioData audioData;
			ErrMsgList errMsgL;
			if (!readAllSamplesFormatAware(absPath.toStdWString(), audioData.Samples, &audioData.SampleRate, &errMsgL))
			{
				*errMsg = combineErrorMessages(errMsgL).toStdWString();
				return false;
			}
			audioDataList.push_back(std::move(audioData));
		}
		return true;
	}

	void manglePronCodeSphinx(boost::wstring_view pronCode, std::wstring& result)
	{
		boost::wstring_view pronCodeName = pronCode;
		boost::wstring_view pronCodeStressSuffix;
		
		// if it fails, then copy to output as-is
		parsePronCodeNameAndStress(pronCode, &pronCodeName, &pronCodeStressSuffix);

		result.append(pronCodeName.data(), pronCodeName.size());
		
		// append stress number immediately
		result.append(pronCodeStressSuffix.data(), pronCodeStressSuffix.size());
	}

	bool writePhoneticDictSphinx(const std::vector<PhoneticWord>& phoneticDictWords, const PhoneRegistry& phoneReg, const boost::filesystem::path& filePath, std::function<auto (boost::wstring_view)->boost::wstring_view> pronCodeDisplay, ErrMsgList* errMsg)
	{
		QFile file(toQStringBfs(filePath));
		if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
		{
			pushErrorMsg(errMsg, str(boost::format("Can't open file for writing (%1%)") % filePath.string()));
			return false;
		}

		QTextStream txt(&file);
		txt.setCodec("UTF-8");
		txt.setGenerateByteOrderMark(false);

		std::string phoneListStr;

		for (const PhoneticWord& word : phoneticDictWords)
		{
			for (const PronunciationFlavour& pron : word.Pronunciations)
			{
				phoneListStr.clear();
				bool pronToStrOp = phoneListToStr(phoneReg, pron.Phones, phoneListStr);
				if (!pronToStrOp)
				{
					pushErrorMsg(errMsg, str(boost::format("Can't convert phone list to string for word %1%") % toUtf8StdString(word.Word)));
					return false;
				}

				boost::wstring_view dispName = pron.PronCode;

				if (pronCodeDisplay != nullptr)
				{
					boost::wstring_view mappedName = pronCodeDisplay(pron.PronCode);
					if (mappedName != boost::wstring_view())
						dispName = mappedName;
				}

				txt << toQString(dispName);

				txt << "\t" << QLatin1String(phoneListStr.c_str()) << "\n";
			}
		}
		return true;
	}

	bool readSphinxFileFileId(boost::wstring_view fileIdPath, std::vector<std::wstring>& fileIds)
	{
		QFile file(toQString(fileIdPath));
		if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
			return false;

		QTextStream stream(&file);
		while (!stream.atEnd())
		{
			QString fileId = stream.readLine();
			fileIds.push_back(fileId.toStdWString());
		}
		return true;
	}

	bool readSphinxFileTranscription(boost::wstring_view transcriptionFilePath, std::vector<SphinxTranscriptionLine>& transcriptions)
	{
		QFile file(toQString(transcriptionFilePath));
		if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
			return false;

		QTextStream stream(&file);
		stream.setCodec("UTF-8");
		while (!stream.atEnd())
		{
			QString line = stream.readLine();
			int openBracketInd = line.lastIndexOf(QChar('('));
			if (openBracketInd == -1)
				return false;

			// move to the *last* group separated by underscore
			int closeBracketInd = line.indexOf(QChar(')'), openBracketInd);
			if (closeBracketInd == -1)
				return false;

			// -1 to account for space after transcription and before the open bracket
			QString transcr = line.left(openBracketInd - 1);

			QString fileId = line.mid(openBracketInd + 1, closeBracketInd - openBracketInd - 1);

			SphinxTranscriptionLine part;
			part.Transcription = transcr.toStdWString();
			part.FileNameWithoutExtension = fileId.toStdWString();
			transcriptions.push_back(part);
		}
		return true;
	}

	bool checkFileIdTranscrConsistency(const std::vector<std::wstring>& dataFilePathNoExt, const std::vector<SphinxTranscriptionLine>& dataTranscr)
	{
		if (dataFilePathNoExt.size() != dataTranscr.size())
			return false;

		for (size_t i = 0; i < dataFilePathNoExt.size(); ++i)
		{
			const std::wstring& filePathNoExt = dataFilePathNoExt[i];
			const SphinxTranscriptionLine& transcrInfo = dataTranscr[i];

			// TODO: substring is not enough
			// here we actually need to extract exact file name without ext from filePath and compare with fileId from transcription
			if (filePathNoExt.find(transcrInfo.FileNameWithoutExtension) == std::wstring::npos)
				return false;
		}
		return true;
	};

	// TODO: move to SphinxDataValidation module
	bool rulePhoneticDicHasNoBrokenPronCodes(const std::vector<PhoneticWord>& phoneticDictPronCodes, const std::map<boost::wstring_view, PhoneticWord>& phoneticDictBroken, std::vector<boost::wstring_view>* brokenPronCodes)
	{
		bool noBrokenPron = true;
		for (const PhoneticWord& word : phoneticDictPronCodes)
		{
			auto brokenWordIt = phoneticDictBroken.find(word.Word);
			if (brokenWordIt == std::end(phoneticDictBroken))
				continue;

			const PhoneticWord& brokenWord = brokenWordIt->second;

			//
			for (const PronunciationFlavour& pron : word.Pronunciations)
			{
				auto brokenPronIt = std::find_if(std::begin(brokenWord.Pronunciations), std::end(brokenWord.Pronunciations), [&pron](const PronunciationFlavour& x)
				{
					return x.PronCode == pron.PronCode;
				});

				if (brokenPronIt != std::end(brokenWord.Pronunciations))
				{
					noBrokenPron = false;
					if (brokenPronCodes != nullptr) brokenPronCodes->push_back(pron.PronCode);
				}
			}
		}
		return noBrokenPron;
	}

#if PG_HAS_SPHINX
	cmd_ln_t* SphinxConfig::pg_init_cmd_ln_t(boost::string_view acousticModelDir, boost::string_view langModelFile, boost::string_view phoneticModelFile, bool verbose, bool debug, bool backTrace, boost::string_view logFile)
	{
		// cmd_ln_init() redirects to parse_options() which do not use variable number of arguments
		// but the later is internal
		// now, using cmd_ln_init(), only one last argument may be optional
		// note, number of variable arguments must be even (arg name, arg value)
		cmd_ln_t* config = cmd_ln_init(nullptr, ps_args(), true,
			"-hmm", acousticModelDir.data(),
			"-lm", langModelFile.data(), // 
			"-dict", phoneticModelFile.data(),
			"-verbose", verbose ? "yes" : "no",
			"-debug", debug ? "1" : "0", // value>0 crashes pocketsphinx-5prelease on debug output
			"-backtrace", backTrace ? "yes" : "no",

			"-lw", SphinxConfig::DEC_CFG_LANGUAGEWEIGHT(),
			"-fwdflatlw", SphinxConfig::DEC_CFG_LANGUAGEWEIGHT(),
			"-bestpathlw", SphinxConfig::DEC_CFG_LANGUAGEWEIGHT(),

			"-beam", SphinxConfig::DEC_CFG_BEAMWIDTH(),
			"-wbeam", SphinxConfig::DEC_CFG_WORDBEAM(),
			"-fwdflatbeam", SphinxConfig::DEC_CFG_BEAMWIDTH(),
			"-fwdflatwbeam", SphinxConfig::DEC_CFG_WORDBEAM(),
			"-pbeam", SphinxConfig::DEC_CFG_BEAMWIDTH(),
			"-lpbeam", SphinxConfig::DEC_CFG_BEAMWIDTH(),
			"-lponlybeam", SphinxConfig::DEC_CFG_BEAMWIDTH(),

			// note, logFile can be optional
			// null is treated as arguments' terminator
			logFile.data() != nullptr ? "-logfn" : nullptr, logFile.data(),
			nullptr // args list terminator
			);
		return config;
	}
#endif
}
