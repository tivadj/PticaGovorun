#include "stdafx.h"
#include <chrono>
#include "SpeechDataValidation.h"
#include "PhoneticService.h"
#include "AppHelpers.h"
#include "SpeechAnnotation.h"
#include "SpeechProcessing.h"

namespace PticaGovorun
{
	namespace
	{
		static const size_t WordsCount = 200000;
	}

	SpeechData::SpeechData(const boost::filesystem::path& speechProjDir_)
		: speechProjDir_(speechProjDir_)
	{
	}

	SpeechData::~SpeechData()
	{
	}

	void SpeechData::setPhoneReg(std::shared_ptr<PhoneRegistry> phoneReg)
	{
		phoneReg_ = phoneReg;
	}

	void SpeechData::setStringArena(std::shared_ptr<GrowOnlyPinArena<wchar_t>> stringArena)
	{
		stringArena_ = stringArena;
	}

	void SpeechData::ensurePhoneticDictionaryLoaded()
	{
		if (phoneticDictWellFormed_.empty())
		{
			bool loadOp;
			const char* errMsg;
			std::tie(loadOp, errMsg) = loadPhoneticDictionaryXml(persianDictPath().wstring(), *phoneReg_, phoneticDictWellFormedWords_, *stringArena_);
			if (!loadOp)
				qDebug(errMsg);
			else
				reshapeAsDict(phoneticDictWellFormedWords_, phoneticDictWellFormed_);
		}
		if (phoneticDictBroken_.empty())
		{
			bool loadOp;
			const char* errMsg;
			std::tie(loadOp, errMsg) = loadPhoneticDictionaryXml(brokenDictPath().wstring(), *phoneReg_, phoneticDictBrokenWords_, *stringArena_);
			if (!loadOp)
				qDebug(errMsg);
			else
				reshapeAsDict(phoneticDictBrokenWords_, phoneticDictBroken_);
		}
		if (phoneticDictFiller_.empty())
		{
			bool loadOp;
			const char* errMsg;
			std::tie(loadOp, errMsg) = loadPhoneticDictionaryXml(fillerDictPath().wstring(), *phoneReg_, phoneticDictFillerWords_, *stringArena_);
			if (!loadOp)
				qDebug(errMsg);
			else
				reshapeAsDict(phoneticDictFillerWords_, phoneticDictFiller_);
		}
	}

	void SpeechData::ensureShrekkyDictLoaded()
	{
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
			std::tie(loadOp, errMsg) = loadPhoneticDictionaryPronIdPerLine(shrekkyDictPath().wstring(), *phoneReg_, *pTextCodec, phoneticDictWords, brokenLines, *stringArena_);

			std::chrono::time_point<Clock> now2 = Clock::now();
			auto elapsedSec = std::chrono::duration_cast<std::chrono::seconds>(now2 - now1).count();
			std::wcout << "Loaded shrekky dict in " << elapsedSec << " s";

			if (!loadOp)
				std::wcout << errMsg;
			else
			{
				reshapeAsDict(phoneticDictWords, phoneticDictShrekky_);
			}
		}
	}

	void SpeechData::saveDict()
	{
		auto selectSecondFun = [](const std::pair<boost::wstring_ref, PhoneticWord>& pair)
		{
			return pair.second;
		};

		std::vector<PhoneticWord> words(phoneticDictWellFormed_.size());
		std::transform(std::begin(phoneticDictWellFormed_), std::end(phoneticDictWellFormed_), std::begin(words), selectSecondFun);
		savePhoneticDictionaryXml(words, persianDictPath().wstring(), *phoneReg_);

		//
		words.resize(phoneticDictBroken_.size());
		std::transform(std::begin(phoneticDictBroken_), std::end(phoneticDictBroken_), std::begin(words), selectSecondFun);
		savePhoneticDictionaryXml(words, brokenDictPath().wstring(), *phoneReg_);
	}

	void SpeechData::ensureDataLoaded()
	{
		ensurePhoneticDictionaryLoaded();
	}

	boost::filesystem::path SpeechData::speechProjDir() const
	{
		return speechProjDir_;
	}

	boost::filesystem::path SpeechData::speechAnnotDirPath() const
	{
		return speechProjDir_ / "SpeechAnnot";
	}

	boost::filesystem::path SpeechData::persianDictPath()
	{
		return speechProjDir_ / "PhoneticDict/phoneticDictUkKnown.xml";
	}

	boost::filesystem::path SpeechData::brokenDictPath()
	{
		return speechProjDir_ / "PhoneticDict/phoneticDictUkBroken.xml";
	}

	boost::filesystem::path SpeechData::fillerDictPath()
	{
		return speechProjDir_ / "PhoneticDict/phoneticDictFiller.xml";
	}

	boost::filesystem::path SpeechData::shrekkyDictPath()
	{
		return AppHelpers::mapPath("pgdata/Julius/shrekky/shrekkyDic.voca").toStdWString();
	}

	std::tuple<bool, const char*> SpeechData::convertTextToPhoneListString(boost::wstring_ref text, std::string& speechPhonesString)
	{
		ensurePhoneticDictionaryLoaded();

		std::ostringstream buff;
		std::vector<PronunciationFlavour> prons;

		std::wstring textW = toStdWString(text);
		std::vector<boost::wstring_ref> pronCodes;
		splitUtteranceIntoWords(textW, pronCodes);

		// iterate through words
		for (size_t i = 0; i < pronCodes.size(); ++i)
		{
			boost::wstring_ref pronIdRef = pronCodes[i];

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

			if (i < pronCodes.size() - 1)
			{
				buff << "~"; // separator
			}
		}

		speechPhonesString = buff.str();

		return std::make_tuple(true, nullptr);
	}

	bool SpeechData::findPronAsWordPhoneticExpansions(boost::wstring_ref pronAsWord, std::vector<PronunciationFlavour>& prons)
	{
		bool found1 = findPronAsWordPhoneticExpansions(phoneticDictWellFormed_, pronAsWord, prons);
		bool found2 = findPronAsWordPhoneticExpansions(phoneticDictBroken_, pronAsWord, prons);
		bool found3 = findPronAsWordPhoneticExpansions(phoneticDictFiller_, pronAsWord, prons);
		return found1 || found2 || found3;
	}

	bool SpeechData::findPronAsWordPhoneticExpansions(const std::map<boost::wstring_ref, PhoneticWord>& phoneticDict, boost::wstring_ref pronCode, std::vector<PronunciationFlavour>& prons)
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

	const PhoneticWord* SpeechData::findPhoneticWord(const QString& browseDictStr, const std::wstring& word) const
	{
		const std::map<boost::wstring_ref, PhoneticWord>* dict = nullptr;
		if (browseDictStr.compare("shrekky", Qt::CaseInsensitive) == 0)
			dict = &phoneticDictShrekky_;
		else if (browseDictStr.compare("broken", Qt::CaseInsensitive) == 0)
			dict = &phoneticDictBroken_;
		else if (browseDictStr.compare("persian", Qt::CaseInsensitive) == 0)
			dict = &phoneticDictWellFormed_;
		if (dict == nullptr)
			return nullptr;

		auto it = dict->find(word);
		if (it != std::end(*dict))
		{
			const PhoneticWord& pron = it->second;
			return &pron;
		}
		return nullptr;
	}

	bool SpeechData::createUpdateDeletePhoneticWord(const QString& dictId, const QString& word, const QString& pronLinesAsStr, QString* errMsg)
	{
		std::map<boost::wstring_ref, PhoneticWord>* targetDict = nullptr;
		if (dictId.compare("persian", Qt::CaseInsensitive) == 0)
			targetDict = &phoneticDictWellFormed_;
		else if (dictId.compare("broken", Qt::CaseInsensitive) == 0)
			targetDict = &phoneticDictBroken_;
		if (targetDict == nullptr)
			return false;

		PhoneticWord wordItem;

		std::vector<wchar_t> wordBuff;
		boost::wstring_ref wordRef = toWStringRef(word, wordBuff);
		boost::wstring_ref arenaWordRef;

		auto itWord = targetDict->find(wordRef);
		if (itWord != std::end(*targetDict))
		{
			arenaWordRef = itWord->first;
			wordItem = itWord->second;
		}
		else
		{
			if (!registerWord(wordRef, *stringArena_, arenaWordRef))
			{
				*errMsg = "Can't allocate new word.";
				return false;
			}
			wordItem.Word = arenaWordRef;
		}

		//
		bool parseOp;
		const char* errMsgC;
		std::vector<PronunciationFlavour> prons;
		// TODO: memory leak; string arena always grow
		std::tie(parseOp, errMsgC) = parsePronuncLinesNew(*phoneReg_, pronLinesAsStr.toStdWString(), prons, *stringArena_);
		if (!parseOp)
		{
			*errMsg = QString::fromLatin1(errMsgC);
			return false;
		}
		if (prons.empty())
		{
			// treat empty pronunciations as a request to delete a word
			(*targetDict).erase(wordRef);
		}
		else
		{
			// create or update
			wordItem.Pronunciations = std::move(prons);
			PG_DbgAssert(arenaWordRef != boost::wstring_ref());
			(*targetDict)[arenaWordRef] = wordItem;
		}
		return true;
	}

	void SpeechData::suggesedWordsUserInput(const QString& browseDictStr, const QString& currentWord, QStringList& result)
	{
		ensurePhoneticDictionaryLoaded();
		ensureShrekkyDictLoaded();

		std::map<boost::wstring_ref, PhoneticWord>* dict = nullptr;
		if (browseDictStr.compare("shrekky", Qt::CaseInsensitive) == 0)
			dict = &phoneticDictShrekky_;
		else if (browseDictStr.compare("broken", Qt::CaseInsensitive) == 0)
			dict = &phoneticDictBroken_;
		else if (browseDictStr.compare("persian", Qt::CaseInsensitive) == 0)
			dict = &phoneticDictWellFormed_;
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
				result.append(wordDict);

				if (result.count() > 100)
					break;
			}
		}
	}

	bool SpeechData::validate(QStringList* errMsgs)
	{
		ensurePhoneticDictionaryLoaded();

		bool phDict = validatePhoneticDictionary(errMsgs);
		bool annot = validateAllSpeechAnnotations(errMsgs);

		bool valid = phDict && annot;
#if PG_DEBUG // postcondition
		if (errMsgs != nullptr)
		{
			if (valid)
			{
				PG_DbgAssert(errMsgs->isEmpty());
			}
			else
				PG_DbgAssert(!errMsgs->isEmpty());
		}
#endif
		return valid;
	}

	void SpeechData::iterateAllSpeechAnnotations(boost::wstring_ref annotDir, bool includeBadMarkup,
		std::function<void(const AnnotSpeechFileNode&)> onAnnotFile,
		std::function<void(const SpeechAnnotation&)> onAnnot,
		std::function<void(const char*)> onAnnotError)
	{
		QString curDirQ = toQString(annotDir);

		AnnotSpeechDirNode annotStructure;
		populateAnnotationFileStructure(curDirQ, annotStructure);

		std::vector<AnnotSpeechFileNode> annotInfos;
		flat(annotStructure, annotInfos);

		std::map<boost::wstring_ref, int> pronIdToUsedCount;
		for (const AnnotSpeechFileNode& fileItem : annotInfos)
		{
			if (onAnnotFile != nullptr) onAnnotFile(fileItem);

			// do not consider badly marked markup
			// TODO: fix finance.ua-pynzenykvm markup
			bool isBadMarkup = fileItem.SpeechAnnotationAbsPath.contains("finance.ua-pynzenykvm");
			if (isBadMarkup && !includeBadMarkup)
				continue;

			SpeechAnnotation annot;
			bool loadOp;
			const char* errMsg;
			std::tie(loadOp, errMsg) = loadAudioMarkupFromXml(fileItem.SpeechAnnotationAbsPath.toStdWString(), annot);
			if (!loadOp)
			{
				if (onAnnotError != nullptr)  onAnnotError(errMsg);
				continue;
			}

			if (onAnnot != nullptr) onAnnot(annot);
		}
	}

	// phonetic dictionary

	bool SpeechData::validatePhoneticDictionary(QStringList* errMsgs)
	{
		// stress

		std::vector<const PhoneticWord*> invalidWords;
		bool d1 = validatePhoneticDictNoneOrAllPronunciationsSpecifyStress(phoneticDictWellFormedWords_, &invalidWords);
		bool d2 = validatePhoneticDictNoneOrAllPronunciationsSpecifyStress(phoneticDictBrokenWords_, &invalidWords);
		if (!invalidWords.empty() && errMsgs != nullptr)
		{
			QString msg = "Not all pronCodes specify stress for words: ";
			for (const PhoneticWord* pWord : invalidWords) msg += ", " + toQString(pWord->Word);

			errMsgs->append(msg);
		}

		bool d3 = validatePhoneticDictAllPronsAreUsed(errMsgs);
		return d1 && d2 && d3;
	}

	bool SpeechData::validatePhoneticDictNoneOrAllPronunciationsSpecifyStress(const std::vector<PhoneticWord>& phoneticDictPronCodes, std::vector<const PhoneticWord*>* invalidWords)
	{
		bool result = true;
		for (const PhoneticWord& word : phoneticDictPronCodes)
		{
			if (word.Pronunciations.size() < 2)
				continue;

			int numPronWithExclam = 0;
			for (const PronunciationFlavour& pron : word.Pronunciations)
			{
				if (isWordStressAssigned(*phoneReg_, pron.Phones))
					numPronWithExclam++;
			}
			bool ok = numPronWithExclam == 0 || numPronWithExclam == word.Pronunciations.size();
			if (!ok)
			{
				result = false;
				if (invalidWords != nullptr)
					invalidWords->push_back(&word);
			}
		}
		return result;
	}

	bool SpeechData::validatePhoneticDictAllPronsAreUsed(QStringList* errMsgs)
	{
		std::map<boost::wstring_ref, int> pronIdToUsedCount;

		// set up all words in phonetic dictionary for counting
		auto pushPronIds = [&pronIdToUsedCount](const decltype(phoneticDictWellFormed_)& dict) -> void
		{
			for (const auto& pair : dict)
			{
				for (const auto& pron : pair.second.Pronunciations)
					pronIdToUsedCount[pron.PronCode] = 0;
			}
		};
		pushPronIds(phoneticDictWellFormed_);
		pushPronIds(phoneticDictBroken_);

		auto countFun = [this, &pronIdToUsedCount](const SpeechAnnotation& annot)
		{
			// count usage of pronIds in phonetic dictionary
			phoneticDictCountPronUsage(annot, pronIdToUsedCount);
		};
		auto oneAnnotErrFun = [this, errMsgs](const char* errMsg)
		{
			errMsgs->push_back(QString::fromLatin1(errMsg));
		};

		auto annotDir = speechAnnotDirPath().wstring();
		iterateAllSpeechAnnotations(annotDir, true, nullptr, countFun, oneAnnotErrFun);

		int numErrs = 0;
		for (const auto& pair : pronIdToUsedCount)
		{
			if (pair.second == 0)
			{
				numErrs += 1;
				if (errMsgs != nullptr)
					errMsgs->append(QString("PronId=%1 is not used in speech annotation\n").arg(toQString(pair.first)));
			}
		}
		return numErrs == 0;
	}

	void SpeechData::phoneticDictCountPronUsage(const SpeechAnnotation& speechAnnot, std::map<boost::wstring_ref, int>& pronIdToUsedCount)
	{
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

	// speech annotation

	bool SpeechData::validateAllSpeechAnnotations(QStringList* errMsgs)
	{
		int errNum = 0;
		QString curSpeechFileAbsPath;
		auto oneAnnotFileFun = [this, &curSpeechFileAbsPath](const AnnotSpeechFileNode& fileNode)
		{
			curSpeechFileAbsPath = fileNode.SpeechAnnotationAbsPath;
		};
		auto oneAnnotErrFun = [this, errMsgs](const char* errMsg)
		{
			errMsgs->push_back(QString::fromLatin1(errMsg));
		};
		auto oneAnnotFun = [this, errMsgs, &curSpeechFileAbsPath, &errNum](const SpeechAnnotation& annot)
		{
			// accumulate error messages in different temporary list
			// if it gets messages then we will prepend it with anot file's path
			QStringList fileItemMsgs;
			validateOneSpeechAnnot(annot, &fileItemMsgs);

			if (!fileItemMsgs.empty())
			{
				errNum += fileItemMsgs.size();
				errMsgs->append(QString("File=%1\n").arg(curSpeechFileAbsPath));
				(*errMsgs) << fileItemMsgs << "\n";
			}
		};

		auto annotDir = speechAnnotDirPath().wstring();
		iterateAllSpeechAnnotations(annotDir, false, oneAnnotFileFun, oneAnnotFun, oneAnnotErrFun);
		return errNum == 0;
	}

	void SpeechData::validateOneSpeechAnnot(const SpeechAnnotation& annot, QStringList* errMsgs)
	{
		ensurePhoneticDictionaryLoaded();

		// validate markers structure

		annot.validateMarkers(*errMsgs);

		// validate that all pronId of entire transcript text are in the phonetic dictionary

		validateOneSpeechAnnotHasPhoneticExpansion(annot, *errMsgs);
	}

	void SpeechData::validateOneSpeechAnnotHasPhoneticExpansion(const SpeechAnnotation& speechAnnot, QStringList& checkMsgs)
	{
		std::vector<std::pair<const PticaGovorun::TimePointMarker*, const PticaGovorun::TimePointMarker*>> segments;
		collectAnnotatedSegments(speechAnnot.markers(), segments);

		for (const std::pair<const PticaGovorun::TimePointMarker*, const PticaGovorun::TimePointMarker*>& seg : segments)
		{
			if (seg.first->Language == SpeechLanguage::Ukrainian)
			{
				int sizeBefore = checkMsgs.size();
				validateUtteranceHasPhoneticExpansion(seg.first->TranscripText, checkMsgs);

				int sizeAfter = checkMsgs.size();
				int newCount = sizeAfter - sizeBefore;

				// update last few messages with info about marker
				QString markerInfo = QString(" marker[id=%1]").arg(seg.first->Id);
				for (int i = 0; i < newCount; ++i)
					checkMsgs[checkMsgs.size() - 1 - i].append(markerInfo);
			}
		}
	}

	void SpeechData::validateUtteranceHasPhoneticExpansion(const QString& text, QStringList& checkMsgs)
	{
		ensurePhoneticDictionaryLoaded();

		std::wstring textW = text.toStdWString();

		std::vector<boost::wstring_ref> pronCodes;
		splitUtteranceIntoWords(textW, pronCodes);

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
		for (boost::wstring_ref pronCode : pronCodes)
		{
			outSimilarPronAsWordDictKnown.clear();
			outExactWordDictKnown.clear();
			outSimilarPronAsWordDictBroken.clear();
			outExactWordDictBroken.clear();
			bool isKnown = isPronAsWordInDict(phoneticDictWellFormed_, pronCode, outExactWordDictKnown, outSimilarPronAsWordDictKnown);
			bool isBroken = isPronAsWordInDict(phoneticDictBroken_, pronCode, outExactWordDictBroken, outSimilarPronAsWordDictBroken);
			bool isFiller = isPronAsWordInDict(phoneticDictFiller_, pronCode, outExactWordDictBroken, outSimilarPronAsWordDictBroken);

			if (!isKnown && !isBroken && !isFiller)
			{
				msgBuf.str(L"");

				bool hasSimilar = !outExactWordDictKnown.empty() || !outSimilarPronAsWordDictKnown.empty() ||
					!outExactWordDictBroken.empty() || !outSimilarPronAsWordDictBroken.empty();
				if (!hasSimilar)
					msgBuf << L"PronId '" << pronCode << L"' is not in the phonetic dictionary";
				else
				{
					// display different message for error with suggestions to visually distinguish
					// from error without suggestions
					msgBuf << L"PronId '" << pronCode << L"' can't be found. Try ";

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
				msgBuf << L"PronId '" << pronCode << L"' has occurence in multiple dictionaries";
				checkMsgs.append(QString::fromStdWString(msgBuf.str()));
			}
			else
			{
				// ok, word is exactly in on dictionary of correct or broken words
			}
		}
	}
}