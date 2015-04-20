#include <sstream>
#include <QTextCodec>
#include <QDebug>
#include "PhoneticDictionaryViewModel.h"
#include <ClnUtils.h>
#include "SpeechProcessing.h"

namespace PticaGovorun
{
	const wchar_t* shrekkyDictPath = LR"path(C:\devb\PticaGovorunProj\data\shrekky\shrekkyDic.voca)path";
	const wchar_t* persianDictPath = LR"path(C:\devb\PticaGovorunProj\data\TrainSphinx\SpeechModels\phoneticKnown.yml)path";
	const wchar_t* brokenDictPath = LR"path(C:\devb\PticaGovorunProj\data\TrainSphinx\SpeechModels\phoneticBroken.yml)path";

	PhoneticDictionaryViewModel::PhoneticDictionaryViewModel()
	{
	}

	void PhoneticDictionaryViewModel::ensureDictionaryLoaded()
	{
		if (wordToPhoneListDict_.empty())
		{
			QTextCodec* pTextCodec = QTextCodec::codecForName("windows-1251");
			bool loadOp;
			const char* errMsg;
			std::tie(loadOp, errMsg) = loadPronunciationVocabulary2(shrekkyDictPath, wordToPhoneListDict_, *pTextCodec);
			if (!loadOp)
				qDebug(errMsg);
		}

		std::vector<PhoneticWord> phoneticDict;
		if (persianWordToPronsDict_.empty())
		{
			bool loadOp;
			const char* errMsg;
			phoneticDict.clear();
			std::tie(loadOp, errMsg) = loadPhoneticDictionaryYaml(persianDictPath, phoneticDict);
			if (!loadOp)
				qDebug(errMsg);
			else
			{
				for (const PhoneticWord& item : phoneticDict)
					persianWordToPronsDict_[item.Word] = item;
			}
		}
		if (brokenWordToPronsDict_.empty())
		{
			bool loadOp;
			const char* errMsg;
			phoneticDict.clear();
			brokenWordToPronsDict_.clear();
			std::tie(loadOp, errMsg) = loadPhoneticDictionaryYaml(brokenDictPath, phoneticDict);
			if (!loadOp)
				qDebug(errMsg);
			else
			{
				for (const PhoneticWord& item : phoneticDict)
					brokenWordToPronsDict_[item.Word] = item;
			}
		}
	}

	void PhoneticDictionaryViewModel::setCurrentWord(QString browseDictStr, const QString& currentWord)
	{
		ensureDictionaryLoaded();

		matchedWords_.clear();
		QString wordDict;

		if (browseDictStr.compare("shrekky", Qt::CaseInsensitive) == 0)
		{
			for (const auto& pair : wordToPhoneListDict_)
			{
				const std::wstring& word = pair.first;
				wordDict.setUtf16((ushort*)word.data(), word.size());
				if (wordDict.startsWith(currentWord, Qt::CaseInsensitive))
				{
					matchedWords_.append(wordDict);
					if (matchedWords_.count() > 100)
						break;
				}
			}
		}
		else
		{
			std::map<std::wstring, PhoneticWord>* dict = nullptr;
			if (browseDictStr.compare("broken", Qt::CaseInsensitive) == 0)
				dict = &brokenWordToPronsDict_;
			else if (browseDictStr.compare("persian", Qt::CaseInsensitive) == 0)
				dict = &persianWordToPronsDict_;
			if (dict == nullptr)
				return;

			QString pronAsWord;
			for (const auto& pair : *dict)
			{
				const PhoneticWord& wordAndProns = pair.second;

				wordDict.setUtf16((ushort*)wordAndProns.Word.data(), wordAndProns.Word.size());

				// try to match word itself
				bool match = wordDict.startsWith(currentWord, Qt::CaseInsensitive);

				// try to match its pronunciations
				if (!match)
				{
					for (const PronunciationFlavour& prons : wordAndProns.Pronunciations)
					{
						pronAsWord.setUtf16((ushort*)prons.PronAsWord.data(), prons.PronAsWord.size());

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

		}
		emit matchedWordsChanged();
	}

	const QStringList& PhoneticDictionaryViewModel::matchedWords() const
	{
		return matchedWords_;
	}

	void PhoneticDictionaryViewModel::showWordPhoneticTranscription(QString browseDictStr, const std::wstring& word)
	{
		if (browseDictStr.compare("shrekky", Qt::CaseInsensitive) == 0)
		{
			auto it = wordToPhoneListDict_.find(word);
			if (it != std::end(wordToPhoneListDict_))
			{
				const std::vector<Pronunc>& prons = it->second;
				QString buf;
				for (const Pronunc& pron : prons)
				{
					buf.append(QString::fromStdString(pron.StrDebug).toUpper());
					buf.append("\n");
				}
				wordPhoneticTrnascript_ = buf;
			}
		}
		else
		{
			std::map<std::wstring, PhoneticWord>* dict = nullptr;
			if (browseDictStr.compare("broken", Qt::CaseInsensitive) == 0)
				dict = &brokenWordToPronsDict_;
			else if (browseDictStr.compare("persian", Qt::CaseInsensitive) == 0)
				dict = &persianWordToPronsDict_;
			if (dict == nullptr)
				return;

			auto it = dict->find(word);
			if (it != std::end(*dict))
			{
				const PhoneticWord& prons = it->second;
				QString buf;
				for (const PronunciationFlavour& pron : prons.Pronunciations)
				{
					buf.append(QString::fromStdWString(pron.PronAsWord));
					buf.append("\t");

					std::string phonesBuf;
					phoneListToStr(pron.PhoneIds, phonesBuf);

					buf.append(phonesBuf.c_str());

					buf.append("\n");
				}
				wordPhoneticTrnascript_ = buf;
			}

		}

		emit phoneticTrnascriptChanged();
	}

	const QString PhoneticDictionaryViewModel::wordPhoneticTranscript() const
	{
		return wordPhoneticTrnascript_;
	}

	void PhoneticDictionaryViewModel::setEditedWordSourceDictionary(QString editedWordSourceDictionary)
	{
		editedWordSourceDictionary_ = editedWordSourceDictionary;
	}

	void PhoneticDictionaryViewModel::updateWordPronunciation(QString dictId, QString word, QString pronLinesAsStr)
	{
		std::map<std::wstring, PhoneticWord>* targetDict = nullptr;
		if (dictId.compare("persian", Qt::CaseInsensitive) == 0)
			targetDict = &persianWordToPronsDict_;
		else if (dictId.compare("broken", Qt::CaseInsensitive) == 0)
			targetDict = &brokenWordToPronsDict_;
		if (targetDict == nullptr)
			return;

		PhoneticWord wordItem;

		std::wstring wordW = word.toStdWString();
		auto itWord = targetDict->find(wordW);
		if (itWord != std::end(*targetDict))
		{
			// prohibit overwriting of existent word
			if (dictId.compare(editedWordSourceDictionary_,Qt::CaseInsensitive) != 0)
			{
				qDebug() << "The word already exist in the dictionary. Edit the existing word instead.";
				return;
			}
			wordItem = itWord->second;
		}
		else
		{
			wordItem.Word = wordW;
		}

		//
		bool parseOp;
		const char* errMsg;
		std::vector<PronunciationFlavour> prons;
		std::tie(parseOp, errMsg) = parsePronuncLines(pronLinesAsStr.toStdWString(), prons);
		if (!parseOp)
		{
			qDebug() << errMsg;
			return;
		}
		if (prons.empty())
		{
			// treat empty pronunciations as a request to delete a word
			(*targetDict).erase(wordW);
		}
		else
		{
			wordItem.Pronunciations = prons;

			(*targetDict)[wordW] = wordItem;
		}

		qDebug() << "Added word=" << word << " pron=" << pronLinesAsStr;
	}

	void PhoneticDictionaryViewModel::saveDict()
	{
		auto selectSecondFun = [](const std::pair<std::wstring, PhoneticWord>& pair)
		{
			return pair.second;
		};

		std::vector<PhoneticWord> words(persianWordToPronsDict_.size());
		std::transform(std::begin(persianWordToPronsDict_), std::end(persianWordToPronsDict_), std::begin(words), selectSecondFun);
		savePhoneticDictionaryYaml(words, persianDictPath);

		//
		words.resize(brokenWordToPronsDict_.size());
		std::transform(std::begin(brokenWordToPronsDict_), std::end(brokenWordToPronsDict_), std::begin(words), selectSecondFun);
		savePhoneticDictionaryYaml(words, brokenDictPath);
	}

	void PhoneticDictionaryViewModel::validateSegmentTranscription(const QString& text, QStringList& resultMessages)
	{
		ensureDictionaryLoaded();

		std::wstring textW = text.toStdWString();

		std::vector<wv::slice<const wchar_t>> wordsAsSlices;
		splitUtteranceIntoWords(textW, wordsAsSlices);

		// check if the word is in the phonetic dictionary
		// considers similar occurences of phoneAsWord and the word itself
		auto isPronAsWordInDict = [](const std::map<std::wstring, PhoneticWord> phoneticDict, const std::wstring& pronAsWordPattern,
			std::wstring& outExactWord, std::wstring& outSimilarPronAsWord) -> bool
		{
			bool exactPronAsWordExist = false;

			// pattern for similar pronAsWord
			QString patternWithOpenBracketQ = QString::fromStdWString(pronAsWordPattern);
			patternWithOpenBracketQ.append('(');

			QString curPronAsWordQ;
			for (const auto& pair : phoneticDict)
			{
				const std::wstring& word = pair.first;
				if (outExactWord.empty() && word == pronAsWordPattern)
					outExactWord = word;

				for (const PronunciationFlavour& pron : pair.second.Pronunciations)
				{
					if (pron.PronAsWord == pronAsWordPattern)
						return true; // interrupt the search

					if (outSimilarPronAsWord.empty())
					{
						curPronAsWordQ.setUtf16((ushort*)pron.PronAsWord.data(), pron.PronAsWord.size());
						if (curPronAsWordQ.startsWith(patternWithOpenBracketQ))
						{
							outSimilarPronAsWord = curPronAsWordQ.toStdWString();
						}
					}
				}
			}
			return exactPronAsWordExist;
		};

		// iterate through words
		std::wstring pronAsWordQuery;
		std::wstringstream msgBuf;
		std::wstring outSimilarPronAsWordDictKnown;
		std::wstring outExactWordDictKnown;
		std::wstring outSimilarPronAsWordDictBroken;
		std::wstring outExactWordDictBroken;
		for (wv::slice<const wchar_t>& pronAsWordSlice : wordsAsSlices)
		{
			pronAsWordQuery.assign(pronAsWordSlice.begin(), pronAsWordSlice.end());

			outSimilarPronAsWordDictKnown.clear();
			outExactWordDictKnown.clear();
			outSimilarPronAsWordDictBroken.clear();
			outExactWordDictBroken.clear();
			bool isKnown = isPronAsWordInDict(persianWordToPronsDict_, pronAsWordQuery, outExactWordDictKnown, outSimilarPronAsWordDictKnown);
			bool isBroken = isPronAsWordInDict(brokenWordToPronsDict_, pronAsWordQuery, outExactWordDictBroken, outSimilarPronAsWordDictBroken);

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
				resultMessages.append(QString::fromStdWString(msgBuf.str()));
			}
			else if (isKnown && isBroken)
			{
				msgBuf.str(L"");
				msgBuf << L"PronId '" << pronAsWordQuery << L"' has occurence in multiple dictionaries";
				resultMessages.append(QString::fromStdWString(msgBuf.str()));
			}
			else
			{
				// ok, word is exactly in on dictionary of correct or broken words
			}
		}
	}
}