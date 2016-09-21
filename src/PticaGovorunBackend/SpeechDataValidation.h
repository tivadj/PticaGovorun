#pragma once
#include <vector>
#include <map>
#include <memory>
#include <boost/filesystem/path.hpp>
#include "PticaGovorunCore.h"
#include "PhoneticService.h"
#include "SpeechAnnotation.h"

namespace PticaGovorun
{
	/// Represents data, required to train speech recognizer: phonetic dictionary, transcribed speech,
	/// language model (probabilities of words in text).
	class PG_EXPORTS SpeechData
	{
		const boost::filesystem::path speechProjDir_;

		std::shared_ptr<PhoneRegistry> phoneReg_;
		std::shared_ptr<GrowOnlyPinArena<wchar_t>> stringArena_;

	public:
		std::vector<PhoneticWord> phoneticDictWellFormedWords_;
		std::vector<PhoneticWord> phoneticDictBrokenWords_;
		std::vector<PhoneticWord> phoneticDictFillerWords_;
		std::map<boost::wstring_view, PhoneticWord> phoneticDictWellFormed_;
		std::map<boost::wstring_view, PhoneticWord> phoneticDictBroken_;
		std::map<boost::wstring_view, PhoneticWord> phoneticDictFiller_;
		std::map<boost::wstring_view, PhoneticWord> phoneticDictShrekky_;
	public:
		explicit SpeechData(const boost::filesystem::path& speechProjDir);
		void setPhoneReg(std::shared_ptr<PhoneRegistry> phoneReg);
		void setStringArena(std::shared_ptr<GrowOnlyPinArena<wchar_t>> stringArena);
		SpeechData(const SpeechData&) = delete;
		~SpeechData();


		void ensureDataLoaded();
		void ensureShrekkyDictLoaded();
		void ensurePhoneticDictionaryLoaded();

		// Saves all phonetic dictionaries.
		void saveDict();

		//

		boost::filesystem::path speechProjDir() const;

		// annotated speech
		boost::filesystem::path speechAnnotDirPath() const;

		// phonetic dictionaries
		boost::filesystem::path persianDictPath();
		boost::filesystem::path brokenDictPath();
		boost::filesystem::path fillerDictPath();
		boost::filesystem::path shrekkyDictPath();

		std::tuple<bool, const char*> convertTextToPhoneListString(boost::wstring_view text, std::string& speechPhonesString);
		bool findPronAsWordPhoneticExpansions(boost::wstring_view pronAsWord, std::vector<PronunciationFlavour>& prons);
	private:
		bool findPronAsWordPhoneticExpansions(const std::map<boost::wstring_view, PhoneticWord>& phoneticDict, boost::wstring_view pronCode, std::vector<PronunciationFlavour>& prons);
	public:
		const PhoneticWord* findPhoneticWord(const QString& browseDictStr, const std::wstring& word) const;
		bool createUpdateDeletePhoneticWord(const QString& dictId, const QString& word, const QString& pronLinesAsStr, QString* errMsg);

		void suggesedWordsUserInput(const QString& browseDictStr, const QString& currentWord, QStringList& result);

	public:
		bool validate(QStringList* errMsgs = nullptr);

		// validate speech annotation
		void validateUtteranceHasPhoneticExpansion(const QString& text, QStringList& checkMsgs);
		void validateOneSpeechAnnot(const SpeechAnnotation& annot, QStringList* errMsgs);

	private:
		bool validatePhoneticDictionary(QStringList* errMsgs = nullptr);

		/// For a word with multiple pronunciations checks if none or all pronCodes have marked stress.
		/// eg. valid=(word=setup, setup(1) setup(2)) or wrong=(word=setup, setup setup(1))
		bool validatePhoneticDictNoneOrAllPronunciationsSpecifyStress(const std::vector<PhoneticWord>& phoneticDictPronCodes, std::vector<const PhoneticWord*>* invalidWords);

		bool validatePhoneticDictAllPronsAreUsed(QStringList* errMsgs);

		// Add each pronId usage from speech annotation to the dict.
		// Used to calculate which pronIds in phonetic dict are not used.
		void phoneticDictCountPronUsage(const SpeechAnnotation& speechAnnot, GrowOnlyPinArena<wchar_t>& arena, std::map<boost::wstring_view, int>& pronIdToUsedCount);

		// speech annotation

		void validateOneSpeechAnnotHasPhoneticExpansion(const SpeechAnnotation& speechAnnot, QStringList& checkMsgs);

		bool validateAllSpeechAnnotations(QStringList* errMsgs);

		static void iterateAllSpeechAnnotations(boost::wstring_view annotDir, bool includeBadMarkup,
			std::function<void(const AnnotSpeechFileNode&)> onAnnotFile,
			std::function<void(const SpeechAnnotation&)> onAnnot,
			std::function<void(const char*)> onAnnotError);
	};
}
