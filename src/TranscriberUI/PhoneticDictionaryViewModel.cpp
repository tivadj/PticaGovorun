#include <sstream>
#include <chrono>
#include <QTextCodec>
#include <QDebug>
#include "PhoneticDictionaryViewModel.h"
#include <ClnUtils.h>
#include "SpeechProcessing.h"
#include "SpeechAnnotation.h"

namespace PticaGovorun
{
	const wchar_t* shrekkyDictPath = LR"path(C:\devb\PticaGovorunProj\data\shrekky\shrekkyDic.voca)path";
	const wchar_t* persianDictPath = LR"path(C:\devb\PticaGovorunProj\srcrep\data\phoneticDictUkKnown.xml)path";
	const wchar_t* brokenDictPath = LR"path(C:\devb\PticaGovorunProj\srcrep\data\phoneticDictUkBroken.xml)path";
	static const size_t WordsCount = 200000;

	PhoneticDictionaryViewModel::PhoneticDictionaryViewModel()
		: stringArena_(WordsCount * 6) // W*C, W words, C chars per word
	{
	}

	void PhoneticDictionaryViewModel::ensureDictionaryLoaded()
	{
		if (phoneReg_ == nullptr)
		{
			phoneReg_ = std::make_unique<PhoneRegistry>();
			bool allowSoftHardConsonant = true;
			bool allowVowelStress = true;
			phoneReg_->setPalatalSupport(PalatalSupport::AsPalatal);
			initPhoneRegistryUk(*phoneReg_, allowSoftHardConsonant, allowVowelStress);
		}

		typedef std::chrono::system_clock Clock;

		std::vector<PhoneticWord> phoneticDictWords;
		std::vector<std::string> brokenLines;
		if (phoneticDictShrekky_.empty())
		{
			QTextCodec* pTextCodec = QTextCodec::codecForName("windows-1251");
			bool loadOp;
			const char* errMsg;

			std::chrono::time_point<Clock> now1 = Clock::now();
			
			phoneticDictWords.reserve(WordsCount);
			std::tie(loadOp, errMsg) = loadPhoneticDictionaryPronIdPerLine(shrekkyDictPath, *phoneReg_, *pTextCodec, phoneticDictWords, brokenLines, stringArena_);
			
			std::chrono::time_point<Clock> now2 = Clock::now();
			auto elapsedSec = std::chrono::duration_cast<std::chrono::seconds>(now2 - now1).count();
			qDebug() << "Loaded shrekky dict in " << elapsedSec << " s";

			if (!loadOp)
				qDebug(errMsg);
			else
			{
				reshapeAsDict(phoneticDictWords, phoneticDictShrekky_);
			}
		}

		if (phoneticDictKnown_.empty())
		{
			bool loadOp;
			const char* errMsg;
			phoneticDictWords.clear();
			phoneticDictWords.reserve(WordsCount);
			std::tie(loadOp, errMsg) = loadPhoneticDictionaryXml(persianDictPath, *phoneReg_, phoneticDictWords, stringArena_);
			if (!loadOp)
				qDebug(errMsg);
			else
				reshapeAsDict(phoneticDictWords, phoneticDictKnown_);
		}
		if (phoneticDictBroken_.empty())
		{
			bool loadOp;
			const char* errMsg;
			phoneticDictWords.clear();
			phoneticDictWords.reserve(WordsCount);
			std::tie(loadOp, errMsg) = loadPhoneticDictionaryXml(brokenDictPath, *phoneReg_, phoneticDictWords, stringArena_);
			if (!loadOp)
				qDebug(errMsg);
			else
				reshapeAsDict(phoneticDictWords, phoneticDictBroken_);
		}
		if (!brokenLines.empty())
		{
			qDebug() << "Found broken lines in shrekky dict";
			for (const auto& line : brokenLines)
				qDebug() << line.c_str();
		}
	}

	void PhoneticDictionaryViewModel::updateSuggesedWordsList(const QString& browseDictStr, const QString& currentWord)
	{
		ensureDictionaryLoaded();

		matchedWords_.clear();

		std::map<boost::wstring_ref, PhoneticWord>* dict = nullptr;
		if (browseDictStr.compare("shrekky", Qt::CaseInsensitive) == 0)
			dict = &phoneticDictShrekky_;
		else if (browseDictStr.compare("broken", Qt::CaseInsensitive) == 0)
			dict = &phoneticDictBroken_;
		else if (browseDictStr.compare("persian", Qt::CaseInsensitive) == 0)
			dict = &phoneticDictKnown_;
		if (dict == nullptr)
			return;

		for (const auto& pair : *dict)
		{
			const PhoneticWord& wordAndProns = pair.second;

			QString wordDict = QString::fromWCharArray(wordAndProns.Word.data(), wordAndProns.Word.size());

			// try to match word itself
			bool match = wordDict.startsWith(currentWord, Qt::CaseInsensitive);

			// try to match its pronunciations
			if (!match)
			{
				for (const PronunciationFlavour& prons : wordAndProns.Pronunciations)
				{
					QString pronAsWord = QString::fromWCharArray(prons.PronCode.data(), prons.PronCode.size());

					if (pronAsWord.startsWith(currentWord, Qt::CaseInsensitive))
					{
						match = true;
						break; // process the next word
					}
				}
			}

			if (match)
			{
				// the word itself, not its pronunciation (which looks like word) is shown
				matchedWords_.append(wordDict);

				if (matchedWords_.count() > 100)
					break;
			}
		}
		emit matchedWordsChanged();
	}

	const QStringList& PhoneticDictionaryViewModel::matchedWords() const
	{
		return matchedWords_;
	}

	void PhoneticDictionaryViewModel::showWordPhoneticTranscription(const QString& browseDictStr, const std::wstring& word)
	{
		std::map<boost::wstring_ref, PhoneticWord>* dict = nullptr;
		if (browseDictStr.compare("shrekky", Qt::CaseInsensitive) == 0)
			dict = &phoneticDictShrekky_;
		else if (browseDictStr.compare("broken", Qt::CaseInsensitive) == 0)
			dict = &phoneticDictBroken_;
		else if (browseDictStr.compare("persian", Qt::CaseInsensitive) == 0)
			dict = &phoneticDictKnown_;
		if (dict == nullptr)
			return;

		auto it = dict->find(word);
		if (it != std::end(*dict))
		{
			const PhoneticWord& prons = it->second;
			QString buf;
			for (const PronunciationFlavour& pron : prons.Pronunciations)
			{
				buf.append(QString::fromWCharArray(pron.PronCode.data(), pron.PronCode.size()));
				buf.append("\t");

				std::string phonesBuf;
				phoneListToStr(*phoneReg_, pron.Phones, phonesBuf);

				buf.append(phonesBuf.c_str());

				buf.append("\n");
			}
			wordPhoneticTrnascript_ = buf;
		}

		emit phoneticTrnascriptChanged();
	}

	const QString PhoneticDictionaryViewModel::wordPhoneticTranscript() const
	{
		return wordPhoneticTrnascript_;
	}

	void PhoneticDictionaryViewModel::setEditedWordSourceDictionary(const QString& editedWordSourceDictionary)
	{
		editedWordSourceDictionary_ = editedWordSourceDictionary;
	}

	void PhoneticDictionaryViewModel::updateWordPronunciation(const QString& dictId, const QString& word, const QString& pronLinesAsStr)
	{
		std::map<boost::wstring_ref, PhoneticWord>* targetDict = nullptr;
		if (dictId.compare("persian", Qt::CaseInsensitive) == 0)
			targetDict = &phoneticDictKnown_;
		else if (dictId.compare("broken", Qt::CaseInsensitive) == 0)
			targetDict = &phoneticDictBroken_;
		if (targetDict == nullptr)
			return;

		PhoneticWord wordItem;

		std::vector<wchar_t> wordBuff;
		boost::wstring_ref wordRef = toWStringRef(word, wordBuff);
		boost::wstring_ref arenaWordRef;

		auto itWord = targetDict->find(wordRef);
		if (itWord != std::end(*targetDict))
		{
			// prohibit overwriting of existent word
			if (dictId.compare(editedWordSourceDictionary_, Qt::CaseInsensitive) != 0)
			{
				qDebug() << "The word already exist in the dictionary. Edit the existing word instead.";
				return;
			}
			arenaWordRef = itWord->first;
			wordItem = itWord->second;
		}
		else
		{
			if (!registerWord(wordRef, stringArena_, arenaWordRef))
			{
				qDebug() << "Can't allocate new word.";
				return;
			}
			wordItem.Word = arenaWordRef;
		}

		//
		bool parseOp;
		const char* errMsg;
		std::vector<PronunciationFlavour> prons;
		// TODO: memory leak; string arena always grow
		std::tie(parseOp, errMsg) = parsePronuncLinesNew(*phoneReg_, pronLinesAsStr.toStdWString(), prons, stringArena_);
		if (!parseOp)
		{
			qDebug() << errMsg;
			return;
		}
		if (prons.empty())
		{
			// treat empty pronunciations as a request to delete a word
			(*targetDict).erase(wordRef);
		}
		else
		{
			wordItem.Pronunciations = std::move(prons);
			PG_DbgAssert(arenaWordRef != boost::wstring_ref());
			(*targetDict)[arenaWordRef] = wordItem;
		}

		qDebug() << "Added word=" << word << " pron=" << pronLinesAsStr;
	}

	void PhoneticDictionaryViewModel::saveDict()
	{
		auto selectSecondFun = [](const std::pair<boost::wstring_ref, PhoneticWord>& pair)
		{
			return pair.second;
		};

		std::vector<PhoneticWord> words(phoneticDictKnown_.size());
		std::transform(std::begin(phoneticDictKnown_), std::end(phoneticDictKnown_), std::begin(words), selectSecondFun);
		savePhoneticDictionaryXml(words, persianDictPath, *phoneReg_);

		//
		words.resize(phoneticDictBroken_.size());
		std::transform(std::begin(phoneticDictBroken_), std::end(phoneticDictBroken_), std::begin(words), selectSecondFun);
		savePhoneticDictionaryXml(words, brokenDictPath, *phoneReg_);
	}

	void PhoneticDictionaryViewModel::validateWordsHavePhoneticTranscription(const QString& text, QStringList& checkMsgs)
	{
		ensureDictionaryLoaded();

		std::wstring textW = text.toStdWString();

		std::vector<wv::slice<const wchar_t>> wordsAsSlices;
		splitUtteranceIntoWords(textW, wordsAsSlices);

		// check if the word is in the phonetic dictionary
		// considers similar occurences of phoneAsWord and the word itself
		// returns true if exact word match was found
		auto isPronAsWordInDict = [](const std::map<boost::wstring_ref, PhoneticWord>& phoneticDict, boost::wstring_ref pronAsWordPattern,
			boost::wstring_ref& outExactWord, boost::wstring_ref& outSimilarPronAsWord) -> bool
		{
			// pattern for similar pronAsWord
			std::wstring patternWithOpenBracketQ = toStdWString(pronAsWordPattern);
			patternWithOpenBracketQ.append(L"(");

			for (const auto& pair : phoneticDict)
			{
				boost::wstring_ref word = pair.first;
				if (outExactWord.empty() && word == pronAsWordPattern)
					outExactWord = word;

				for (const PronunciationFlavour& pron : pair.second.Pronunciations)
				{
					if (pron.PronCode == pronAsWordPattern)
						return true; // interrupt the search

					if (outSimilarPronAsWord.empty())
					{
						if (pron.PronCode.starts_with(patternWithOpenBracketQ))
						{
							outSimilarPronAsWord = pron.PronCode;
						}
					}
				}
			}
			return false;
		};

		// iterate through words
		std::wstringstream msgBuf;
		boost::wstring_ref outSimilarPronAsWordDictKnown;
		boost::wstring_ref outExactWordDictKnown;
		boost::wstring_ref outSimilarPronAsWordDictBroken;
		boost::wstring_ref outExactWordDictBroken;
		for (wv::slice<const wchar_t>& pronAsWordSlice : wordsAsSlices)
		{
			boost::wstring_ref pronAsWordQuery(pronAsWordSlice.data(), pronAsWordSlice.size());

			outSimilarPronAsWordDictKnown.clear();
			outExactWordDictKnown.clear();
			outSimilarPronAsWordDictBroken.clear();
			outExactWordDictBroken.clear();
			bool isKnown = isPronAsWordInDict(phoneticDictKnown_, pronAsWordQuery, outExactWordDictKnown, outSimilarPronAsWordDictKnown);
			bool isBroken = isPronAsWordInDict(phoneticDictBroken_, pronAsWordQuery, outExactWordDictBroken, outSimilarPronAsWordDictBroken);

			if (!isKnown && !isBroken)
			{
				msgBuf.str(L"");

				bool hasSimilar = !outExactWordDictKnown.empty() || !outSimilarPronAsWordDictKnown.empty() ||
					!outExactWordDictBroken.empty() || !outSimilarPronAsWordDictBroken.empty();
				if (!hasSimilar)
					msgBuf << L"PronId '" << pronAsWordQuery << L"' is not in the phonetic dictionary";
				else
				{
					// display different message for error with suggestions to visually distinguish
					// from error without suggestions
					msgBuf << L"PronId '" << pronAsWordQuery << L"' can't be found. Try ";

					// display similar occurences
					if (!outSimilarPronAsWordDictKnown.empty())
						msgBuf << " " << outSimilarPronAsWordDictKnown;
					if (!outExactWordDictKnown.empty())
						msgBuf << "word " << outExactWordDictKnown;
					if (!outSimilarPronAsWordDictBroken.empty())
						msgBuf << " " << outSimilarPronAsWordDictBroken;
					if (!outExactWordDictBroken.empty())
						msgBuf << "word " << outExactWordDictBroken;
				}
				checkMsgs.append(QString::fromStdWString(msgBuf.str()));
			}
			else if (isKnown && isBroken)
			{
				msgBuf.str(L"");
				msgBuf << L"PronId '" << pronAsWordQuery << L"' has occurence in multiple dictionaries";
				checkMsgs.append(QString::fromStdWString(msgBuf.str()));
			}
			else
			{
				// ok, word is exactly in on dictionary of correct or broken words
			}
		}
	}

	void PhoneticDictionaryViewModel::validateSpeechAnnotationsHavePhoneticTranscription(const SpeechAnnotation& speechAnnot, QStringList& checkMsgs)
	{
		std::vector<std::pair<const PticaGovorun::TimePointMarker*, const PticaGovorun::TimePointMarker*>> segments;
		collectAnnotatedSegments(speechAnnot.markers(), segments);

		for (const std::pair<const PticaGovorun::TimePointMarker*, const PticaGovorun::TimePointMarker*>& seg : segments)
		{
			if (seg.first->Language == SpeechLanguage::Ukrainian)
			{
				int sizeBefore = checkMsgs.size();
				validateWordsHavePhoneticTranscription(seg.first->TranscripText, checkMsgs);

				int sizeAfter = checkMsgs.size();
				int newCount = sizeAfter - sizeBefore;

				// update last few messages with info about marker
				QString markerInfo = QString(" marker[id=%1]").arg(seg.first->Id);
				for (int i = 0; i < newCount; ++i)
					checkMsgs[checkMsgs.size() - 1 - i].append(markerInfo);
			}
		}
	}

	void PhoneticDictionaryViewModel::validateAllPronunciationsSpecifyStress(QStringList& checkMsgs)
	{
		ensureDictionaryLoaded();

		auto checkStress = [this,&checkMsgs](const std::map<boost::wstring_ref, PhoneticWord>& phonDict) ->void
		{
			for (const auto& pair : phonDict)
			{
				for (const PronunciationFlavour& pronId : pair.second.Pronunciations)
				{
					if (isWordStressAssigned(*phoneReg_, pronId.Phones))
						continue;
					checkMsgs << QString("PronId='%1' does not specify stressed syllable").arg(toQString(pronId.PronCode));
				}
			}
		};
		checkStress(phoneticDictKnown_);
		checkStress(phoneticDictBroken_);
	}

	void PhoneticDictionaryViewModel::countPronIdUsage(const SpeechAnnotation& speechAnnot, std::map<boost::wstring_ref, int>& pronIdToUsedCount)
	{
		ensureDictionaryLoaded();

		// set up all words in phonetic dictionary for counting
		if (pronIdToUsedCount.empty())
		{
			auto pushPronIds = [&pronIdToUsedCount](const decltype(phoneticDictKnown_)& dict) -> void
			{
				for (const auto& pair : dict)
				{
					for (const auto& pron : pair.second.Pronunciations)
						pronIdToUsedCount[pron.PronCode] = 0;
				}
			};
			pushPronIds(phoneticDictKnown_);
			pushPronIds(phoneticDictBroken_);
		}

		//
		std::vector<std::pair<const TimePointMarker*, const TimePointMarker*>> segments;
		collectAnnotatedSegments(speechAnnot.markers(), segments);

		std::vector<wchar_t> textBuff;
		for (const std::pair<const TimePointMarker*, const TimePointMarker*>& seg : segments)
		{
			if (seg.first->Language == SpeechLanguage::Ukrainian)
			{
				const QString& text = seg.first->TranscripText;
				boost::wstring_ref textRef = toWStringRef(text, textBuff);

				std::vector<boost::wstring_ref> wordsAsSlices;
				splitUtteranceIntoWords(textRef, wordsAsSlices);

				for (boost::wstring_ref pronAsWordRef : wordsAsSlices)
				{
					// do not implicitly add the word to counting dict!
					auto it = pronIdToUsedCount.find(pronAsWordRef);
					if (it != pronIdToUsedCount.end())
						pronIdToUsedCount[pronAsWordRef]++;
				}
			}
		}
	}

	std::tuple<bool, const char*> PhoneticDictionaryViewModel::convertTextToPhoneListString(boost::wstring_ref text, std::string& speechPhonesString)
	{
		ensureDictionaryLoaded();

		std::ostringstream buff;
		std::vector<PronunciationFlavour> prons;

		std::wstring textW = toStdWString(text);
		std::vector<wv::slice<const wchar_t>> pronIdsAsSlices;
		splitUtteranceIntoWords(textW, pronIdsAsSlices);

		// iterate through words
		for (size_t i = 0; i < pronIdsAsSlices.size(); ++i)
		{
			wv::slice<const wchar_t>& pronIdSlice = pronIdsAsSlices[i];
			boost::wstring_ref pronIdRef(pronIdSlice.begin(), pronIdSlice.size());

			// convert word to phones
			prons.clear();
			if (!findPronAsWordPhoneticExpansions(pronIdRef, prons))
				buff << "???";
			else if (prons.size() > 1) // more than one possible 
			{
				buff << "(...)";
			}
			else
			{
				const std::vector<PhoneId>& wordPhones = prons.front().Phones;
				std::string phoneListStr;
				if (!phoneListToStr(*phoneReg_, wordPhones, phoneListStr))
					return std::make_tuple(false, "Can't convert phone list to string");
				buff << phoneListStr;
			}

			if (i < pronIdsAsSlices.size() - 1)
			{
				buff << "~"; // separator
			}
		}

		speechPhonesString = buff.str();

		return std::make_tuple(true, nullptr);
	}

	bool PhoneticDictionaryViewModel::findPronAsWordPhoneticExpansions(boost::wstring_ref pronAsWord, std::vector<PronunciationFlavour>& prons)
	{
		bool found1 = findPronAsWordPhoneticExpansions(phoneticDictKnown_, pronAsWord, prons);
		bool found2 = findPronAsWordPhoneticExpansions(phoneticDictBroken_, pronAsWord, prons);
		return found1 || found2;
	}

	QString PhoneticDictionaryViewModel::getWordAutoTranscription(const QString& word) const
	{
		std::vector<PhoneId> phones;
		bool spellOp;
		const char* errMsg;
		std::tie(spellOp, errMsg) = spellWordUk(*phoneReg_, word.toStdWString(), phones, nullptr);
		if (!spellOp)
			return QString::fromLatin1(errMsg);
		std::string wordTranscr;
		if (!phoneListToStr(*phoneReg_, phones, wordTranscr))
			return QString::fromStdWString(L"Can't convert phone list to string");
		return QString::fromStdString(wordTranscr);
	}

	bool PhoneticDictionaryViewModel::findPronAsWordPhoneticExpansions(const std::map<boost::wstring_ref, PhoneticWord>& phoneticDict, boost::wstring_ref pronCode, std::vector<PronunciationFlavour>& prons)
	{
		bool found = false;
		for (const auto& pair : phoneticDict)
		{
			const auto& curProns = pair.second;
			//bool pronMatched = false;
			for (const PronunciationFlavour& pron : curProns.Pronunciations)
			{
				if (pron.PronCode == pronCode)
				{
					// this handles exact match and pronunciation word as prefix
					prons.push_back(pron);
					//pronMatched = true;
					found = true;
				}
			}

			//if (!pronMatched && pair.first == pronCode)
			//{
			//	std::copy(curProns.Pronunciations.begin(), curProns.Pronunciations.end(), std::back_inserter(prons));
			//	found = true;
			//	continue;
			//}
		}
		return found;
	}
}