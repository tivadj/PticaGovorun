#pragma once
#include <map>
#include <QObject>
#include <QStringList>
#include "PhoneticService.h"

namespace PticaGovorun
{
	class PhoneticDictionaryViewModel : public QObject
	{
		Q_OBJECT
	public:
	signals :
		void matchedWordsChanged();
		void phoneticTrnascriptChanged();

	public:
		PhoneticDictionaryViewModel();

		void setCurrentWord(QString browseDictStr, const QString& currentWord);

		const QStringList& matchedWords() const;

		void showWordPhoneticTranscription(QString browseDictStr, const std::wstring& word);

		const QString wordPhoneticTranscript() const;

		void setEditedWordSourceDictionary(QString editedWordSourceDictionary);
		void updateWordPronunciation(QString dictStr, QString newWord, QString newWordProns);

		// Saves all phonetic dictionaries.
		void saveDict();

		void validateSegmentTranscription(const QString& text, QStringList& resultMessages);
	private:
		void ensureDictionaryLoaded();
	private:
		std::map<std::wstring, std::vector<Pronunc>> wordToPhoneListDict_;
		std::map<std::wstring, PhoneticWord> persianWordToPronsDict_;
		std::map<std::wstring, PhoneticWord> brokenWordToPronsDict_;
		std::unique_ptr<PhoneRegistry> phoneReg_;
		QStringList matchedWords_;
		QString wordPhoneticTrnascript_;
		QString editedWordSourceDictionary_; // the dictId from where the current word is taken
	};

}