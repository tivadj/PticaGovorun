#pragma once
#include <QObject>
#include <QStringList>
#include "PhoneticService.h"
#include "ComponentsInfrastructure.h"
#include "SpeechAnnotation.h"
#include "SpeechDataValidation.h"

namespace PticaGovorun
{
	class PhoneticDictionaryViewModel : public QObject
	{
		Q_OBJECT
		std::shared_ptr<SpeechData> speechData_;
	public:
	signals :
		void matchedWordsChanged();
		void phoneticTrnascriptChanged();

	public:
		PhoneticDictionaryViewModel(std::shared_ptr<SpeechData> speechData, std::shared_ptr<PhoneRegistry> phoneReg);

		void updateSuggesedWordsList(const QString& browseDictStr, const QString& currentWord);

		const QStringList& matchedWords() const;

		void showWordPhoneticTranscription(const QString& browseDictStr, const std::wstring& word);

		const QString wordPhoneticTranscript() const;

		void setEditedWordSourceDictionary(const QString& editedWordSourceDictionary);
		void updateWordPronunciation(const QString& dictStr, const QString& newWord, const QString& newWordProns);

		QString getWordAutoPhoneticExpansion(const QString& word) const;

		private:
		std::shared_ptr<PhoneRegistry> phoneReg_;
		QStringList matchedWords_;
		QString wordPhoneticTrnascript_;
		QString editedWordSourceDictionary_; // the dictId from where the current word is taken
	};
}
