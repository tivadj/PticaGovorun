#pragma once
#include <string>
#include <vector>
#include <tuple>
#include <unordered_set>
#include <map>
#include <QTextCodec>
#include <QTextStream>
#include <boost/optional.hpp>
#include <boost/utility/string_ref.hpp>
#include "ComponentsInfrastructure.h"
#include "PticaGovorunCore.h"
#include "LangStat.h"
#include "TextProcessing.h"

namespace PticaGovorun
{
	// Soft or hard attribute (uk: ������/�'���). The third enum value is 'middle-tongued'.
	enum class SoftHardConsonant
	{
		Soft,
		Hard
	};

	// eg TS for soft consonant TS1
	struct PG_EXPORTS BasicPhone
	{
		int Id;
		std::string Name; // eg TS, CH
		CharGroup DerivedFromChar; // the vowel/consonant it is derived from
	};

	struct PG_EXPORTS Phone
	{
		typedef IdWithDebugStr<int, char, 4> PhoneIdT;
		int Id;
		typedef IdWithDebugStr<int, char, 3> BasicPhoneIdT;
		BasicPhoneIdT BasicPhoneId;
		boost::optional<SoftHardConsonant> SoftHard; // valid for consonants only
		boost::optional<bool> IsStressed; // valid for for vowels only
	};

	typedef Phone::PhoneIdT PhoneId;

	// The class to enlist the phones of different configurations. We may be interested in vowel phones with stress marked
	// or consonant phones with marked softness. So the basic vowel phone A turns into A1 under stress.
	// The basic phone T turns into T1 when it is softened.
	class PG_EXPORTS PhoneRegistry
	{
		int nextPhoneId_ = 1; // the first phone gets Id=1
		std::vector<Phone> phoneReg_;
		std::vector<BasicPhone> basicPhones_;
		std::unordered_map<std::string, size_t> basicPhonesStrToId_;
		bool allowSoftHardConsonant_ = false;
		bool allowVowelStress_ = false;
	public:
		typedef Phone::BasicPhoneIdT BasicPhoneIdT;
	private:
		inline BasicPhoneIdT extendBasicPhoneId(int basicPhoneStrId) const;
		BasicPhoneIdT getOrCreateBasicPhone(const std::string& basicPhoneStr, CharGroup charGroup);
	public:
		BasicPhoneIdT basicPhoneId(const std::string& basicPhoneStr, bool* success) const;
		const BasicPhone* basicPhone(BasicPhoneIdT basicPhoneId) const;

		PhoneId newVowelPhone(const std::string& basicPhoneStr, bool isStressed);
		PhoneId newConsonantPhone(const std::string& basicPhoneStr, boost::optional<SoftHardConsonant> softHard);

		// Augment PhoneId:int with string representation of the phone. Useful for debugging.
		inline PhoneId extendPhoneId(int validPhoneId) const;
		inline const Phone* phoneById(int phoneId) const;
		inline const Phone* phoneById(PhoneId phoneId) const;
		boost::optional<PhoneId> phoneIdSingle(const std::string& basicPhoneStr, boost::optional<SoftHardConsonant> softHard, boost::optional<bool> isStressed) const;
		boost::optional<PhoneId> phoneIdSingle(BasicPhoneIdT basicPhoneStrId, boost::optional<SoftHardConsonant> softHard, boost::optional<bool> isStressed) const;

		void findPhonesByBasicPhoneStr(const std::string& basicPhoneStr, std::vector<PhoneId>& phoneIds) const;

		// assumes that phones are not removed from registry and ordered consequently
		int phonesCount() const;
		
		// assume that phones are not removed from phone registry and are ordered consequently
		inline void assumeSequentialPhoneIdsWithoutGaps() const {};

		bool allowSoftHardConsonant() const;
		bool allowVowelStress() const;

		boost::optional<SoftHardConsonant> defaultSoftHardConsonant() const;
		boost::optional<bool> defaultIsVowelStressed() const;
	private:
		friend PG_EXPORTS void initPhoneRegistryUk(PhoneRegistry& phoneReg, bool allowSoftHardConsonant, bool allowVowelStress);
	};

	PG_EXPORTS void initPhoneRegistryUk(PhoneRegistry& phoneReg, bool allowSoftHardConsonant, bool allowVowelStress);

	// TODO: remove
	struct Pronunc
	{
		std::string StrDebug; // debug string
		std::vector<std::string> Phones;

		void setPhones(const std::vector<std::string>& phones);
		void pushBackPhone(const std::string& phone);
	};

	// One possible pronunciation of a word.
	struct PronunciationFlavour
	{
		// The id of pronunciation. It must be unique among all pronunciations of corresponding word.
		// Sphinx uses 'clothes(1)' or 'clothes(2)' as unique pronunciation names for the word 'clothes'.
		// Seems like it is not unique. If some word is pronounced differently then temporary we can assign
		// different pronAsWord for it even though the same sequence of phones is already assigned to some pronAsWord.
		std::wstring PronAsWord;

		// The actual phones of this pronunciation.
		std::vector<PhoneId> Phones;
	};

	// Represents all possible pronunciations of a word.
	// The word 'clothes' may be pronounced as clothes(1)='K L OW1 DH Z' or clothes(2)='K L OW1 Z'
	struct PhoneticWord
	{
		std::wstring Word;
		std::vector<PronunciationFlavour> Pronunciations;
	};

	class PG_EXPORTS UkrainianPhoneticSplitter
	{
		template <typename T, size_t FixedSize>
		struct ShortArray
		{
			std::array<T, FixedSize> Array;
			size_t ActualSize;
		};
		WordsUsageInfo wordUsage_;
		std::unordered_map<std::wstring, ShortArray<int,2>> wordStrToPartIds_; // some words, split by default
		long seqOneWordCounter_ = 0;
		long seqTwoWordsCounter_ = 0;
		bool allowPhoneticWordSplit_ = false;
		const WordPart* sentStartWordPart_;
		const WordPart* sentEndWordPart_;
		const WordPart* wordPartSeparator_ = nullptr;
	public:
		UkrainianPhoneticSplitter();

		void bootstrap(const std::unordered_map<std::wstring, std::unique_ptr<WordDeclensionGroup>>& declinedWords, const std::wstring& targetWord,
			const std::unordered_set<std::wstring>& processedWords);

		void gatherWordPartsSequenceUsage(const wchar_t* textFilesDir, long& totalPreSplitWords, int maxFileToProcess = -1, bool outputCorpus = false);

		const WordsUsageInfo& wordUsage() const;
		WordsUsageInfo& wordUsage();
		void printSuffixUsageStatistics() const;

		// Gets the number of sequences with 'wordSeqLength' words per sequence.
		long wordSeqCount(int wordsPerSeq) const;
		const WordPart* sentStartWordPart() const;
		const WordPart* sentEndWordPart() const;

		void setAllowPhoneticWordSplit(bool value);
	private:
		void doWordPhoneticSplit(const wv::slice<wchar_t>& wordSlice, std::vector<const WordPart*>& wordParts);

		// split words into slices
		void selectWordParts(const std::vector<wv::slice<wchar_t>>& words, std::vector<const WordPart*>& wordParts, long& preSplitWords);

		void calcNGramStatisticsOnWordPartsBatch(std::vector<const WordPart*>& wordParts, bool outputCorpus, QTextStream& corpusStream);

		// calculate statistics on word parts list
		void calcLangStatistics(const std::vector<const WordPart*>& wordParts);
	};

	template <typename StreatT>
	void printWordPart(const WordPart* wordPart, StreatT& stream)
	{
		if (wordPart->partSide() == WordPartSide::RightPart || wordPart->partSide() == WordPartSide::MiddlePart)
			stream << "~";
		stream << QString::fromStdWString(wordPart->partText());
		if (wordPart->partSide() == WordPartSide::LeftPart || wordPart->partSide() == WordPartSide::MiddlePart)
			stream << "~";
	}

	// equality by value
	PG_EXPORTS bool operator == (const Pronunc& a, const Pronunc& b);
	PG_EXPORTS bool operator < (const Pronunc& a, const Pronunc& b);

	// Loads dictionary of word -> (phone list) from text file.
	// File usually has 'voca' extension.
	// File has Windows-1251 encodeding.
	// Each word may have multiple pronunciations (1-* relation); for now we neglect it and store data into map (1-1 relation).
	PG_EXPORTS std::tuple<bool, const char*> loadPronunciationVocabulary(const std::wstring& vocabFilePathAbs, std::map<std::wstring, std::vector<std::string>>& wordToPhoneList, const QTextCodec& textCodec); // TODO: remove
	PG_EXPORTS std::tuple<bool, const char*> loadPronunciationVocabulary2(const std::wstring& vocabFilePathAbs, std::map<std::wstring, std::vector<Pronunc>>& wordToPhoneList, const QTextCodec& textCodec); // TODO: remove

	// 'brokenLines' has lines of the dictionary which can't be read.
	std::tuple<bool, const char*> loadPhoneticDictionaryPronIdPerLine(const std::wstring& vocabFilePathAbs, const PhoneRegistry& phoneReg, const QTextCodec& textCodec, std::vector<PhoneticWord>& words, std::vector<std::string>& brokenLines);

	PG_EXPORTS void normalizePronunciationVocabulary(std::map<std::wstring, std::vector<Pronunc>>& wordToPhoneList, bool toUpper = true, bool trimNumbers = true);
	PG_EXPORTS void trimPhoneStrExtraInfos(const std::string& phoneStr, std::string& phoneStrTrimmed, bool toUpper, bool trimNumbers);

	PG_EXPORTS bool phoneToStr(const PhoneRegistry& phoneReg, int phoneId, std::string& result);
	PG_EXPORTS bool phoneToStr(const PhoneRegistry& phoneReg, PhoneId phoneId, std::string& result);
	PG_EXPORTS bool phoneListToStr(const PhoneRegistry& phoneReg, wv::slice<PhoneId> pron, std::string& result);

	// Parses space-separated list of phones.
	PG_EXPORTS boost::optional<PhoneId> parsePhoneStr(const PhoneRegistry& phoneReg, boost::string_ref phoneStr);
	PG_EXPORTS bool parsePhoneList(const PhoneRegistry& phoneReg, boost::string_ref phoneListStr, std::vector<PhoneId>& result);
	PG_EXPORTS std::tuple<bool, const char*> parsePronuncLines(const PhoneRegistry& phoneReg, const std::wstring& prons, std::vector<PronunciationFlavour>& result);

	// Performs word transcription (word is represented as a sequence of phonemes).
	PG_EXPORTS std::tuple<bool, const char*> spellWordUk(const PhoneRegistry& phoneReg, const std::wstring& word, std::vector<PhoneId>& phones);
	
	// Removes phone modifiers.
	PG_EXPORTS void updatePhoneModifiers(const PhoneRegistry& phoneReg, bool keepConsonantSoftness, bool keepVowelStress, std::vector<PhoneId>& phonesList);

	// Saves phonetic dictionary to file in YAML format.
	PG_EXPORTS void savePhoneticDictionaryYaml(const std::vector<PhoneticWord>& phoneticDict, const std::wstring& filePath, const PhoneRegistry& phoneReg);
	
	PG_EXPORTS std::tuple<bool, const char*> loadPhoneticDictionaryYaml(const std::wstring& filePath, const PhoneRegistry& phoneReg, std::vector<PhoneticWord>& phoneticDict);

	//
	PG_EXPORTS int phoneticSplitOfWord(wv::slice<wchar_t> word, boost::optional<WordClass> wordClass, int* pMatchedSuffixInd = nullptr);

	// Checks whether the character is unvoiced (uk:������).
	PG_EXPORTS inline bool isUnvoicedCharUk(wchar_t ch);
}
