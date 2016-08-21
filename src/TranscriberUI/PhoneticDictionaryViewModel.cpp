#include <sstream>
#include <chrono>
#include <QTextCodec>
#include <QDebug>
#include "PhoneticDictionaryViewModel.h"
#include <ClnUtils.h>
#include "SpeechProcessing.h"
#include "SpeechAnnotation.h"
#include "SpeechDataValidation.h"
#include "assertImpl.h"

namespace PticaGovorun
{
	PhoneticDictionaryViewModel::PhoneticDictionaryViewModel(std::shared_ptr<SpeechData> speechData, std::shared_ptr<PhoneRegistry> phoneReg)
		: speechData_(speechData),
		phoneReg_(phoneReg)
	{
		PG_Assert(speechData_ != nullptr);
		PG_Assert(phoneReg_ != nullptr);
	}

	void PhoneticDictionaryViewModel::updateSuggesedWordsList(const QString& browseDictStr, const QString& currentWord)
	{
		matchedWords_.clear();
		speechData_->suggesedWordsUserInput(browseDictStr, currentWord, matchedWords_);
		emit matchedWordsChanged();
	}

	const QStringList& PhoneticDictionaryViewModel::matchedWords() const
	{
		return matchedWords_;
	}

	void PhoneticDictionaryViewModel::showWordPhoneticTranscription(const QString& browseDictStr, const std::wstring& word)
	{
		const PhoneticWord* phonWord = speechData_->findPhoneticWord(browseDictStr, word);
		if (phonWord != nullptr)
		{
			QString buf;
			for (const PronunciationFlavour& pron : phonWord->Pronunciations)
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
		// prohibit accidental overwriting of word from one dictionary with a value from other dictionary
		auto wordDerivedFromDict = editedWordSourceDictionary_;
		if ((wordDerivedFromDict == "persian" || wordDerivedFromDict == "broken") && wordDerivedFromDict != dictId)
		{
			qDebug() << "The word already exist in the dictionary. Edit the existing word instead.";
			return;
		}

		QString errMsg;
		if (!speechData_->createUpdateDeletePhoneticWord(dictId, word, pronLinesAsStr, &errMsg))
		{
			qDebug() << errMsg;
			return;
		}

		qDebug() << "Added word=" << word << " pron=" << pronLinesAsStr;
	}

	QString PhoneticDictionaryViewModel::getWordAutoPhoneticExpansion(const QString& word) const
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
}