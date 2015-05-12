#pragma once
#include <map>
#include <QObject>
#include <QStringList>
#include "PhoneticService.h"
#include "ComponentsInfrastructure.h"
#include "SpeechAnnotation.h"

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

		void ensureDictionaryLoaded();

		void updateSuggesedWordsList(const QString& browseDictStr, const QString& currentWord);

		const QStringList& matchedWords() const;

		void showWordPhoneticTranscription(const QString& browseDictStr, const std::wstring& word);

		const QString wordPhoneticTranscript() const;

		void setEditedWordSourceDictionary(const QString& editedWordSourceDictionary);
		void updateWordPronunciation(const QString& dictStr, const QString& newWord, const QString& newWordProns);

		// Saves all phonetic dictionaries.
		void saveDict();

		void validateWordsHavePhoneticTranscription(const QString& text, QStringList& checkMsgs);
		void validateSpeechAnnotationsHavePhoneticTranscription(const SpeechAnnotation& speechAnnot, QStringList& checkMsgs);
		void validateAllPronunciationsSpecifyStress(QStringList& checkMsgs);
		
		// Add each pronId usage from speech annotation to the dict.
		// Used to calculate which pronIds in phonetic dict are not used.
		void countPronIdUsage(const SpeechAnnotation& speechAnnot, std::map<boost::wstring_ref, int>& pronIdToUsedCount);

		std::tuple<bool, const char*> convertTextToPhoneListString(boost::wstring_ref text, std::string& speechPhonesString);
		bool findPronAsWordPhoneticExpansions(boost::wstring_ref pronAsWord, std::vector<PronunciationFlavour>& prons);

		QString getWordAutoTranscription(const QString& word) const;
	private:
		bool findPronAsWordPhoneticExpansions(const std::map<boost::wstring_ref, PhoneticWord>& phoneticDict, boost::wstring_ref pronCode, std::vector<PronunciationFlavour>& prons);
	private:
		GrowOnlyPinArena<wchar_t> stringArena_;
		std::map<boost::wstring_ref, PhoneticWord> phoneticDictKnown_;
		std::map<boost::wstring_ref, PhoneticWord> phoneticDictBroken_;
		std::map<boost::wstring_ref, PhoneticWord> phoneticDictShrekky_;
		std::unique_ptr<PhoneRegistry> phoneReg_;
		QStringList matchedWords_;
		QString wordPhoneticTrnascript_;
		QString editedWordSourceDictionary_; // the dictId from where the current word is taken
	};

	void validate(PhoneticDictionaryViewModel& phoneticDictViewModel, const SpeechAnnotation& speechAnnot, QStringList& validResult);
}