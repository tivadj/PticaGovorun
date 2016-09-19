#pragma once
#include <string>
#include <vector>
#include <tuple>
#include <unordered_set>
#include <map>
#include <QTextCodec>
#include <QTextStream>
#include <boost/optional.hpp>
#include <boost/utility/string_view.hpp>
#include <boost/filesystem/path.hpp>
#include "ComponentsInfrastructure.h"
#include "PticaGovorunCore.h"
#include "LangStat.h"
#include "TextProcessing.h"

namespace PticaGovorun
{
	// Soft or hard attribute (uk: тверда/м'яка). The third enum value is 'middle-tongued'.
	enum class SoftHardConsonant
	{
		Hard,
		Palatal, // =HalfSoft
		Soft,
	};

	void toString(SoftHardConsonant value, std::string& result);

	// eg TS for soft consonant TS1
	struct PG_EXPORTS BasicPhone
	{
		int Id;
		std::string Name; // eg TS, CH
		CharGroup DerivedFromChar; // the vowel/consonant it is derived from
	};

	// A  = unvoiced vowel A
	// A1 = voiced vowel A
	// T  = hard T
	// T1 = soft T
	// B2 = palatized (kind of half-soft) B
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

	// Palatal are slightly soft consonants.
	enum class PalatalSupport
	{
		// Palatalized consonant is treated as is (eg P2 -> P2)
		AsPalatal,

		// Palatalized consonant is replaced by hard consonant (eg P2 -> P)
		AsHard,

		// Palatalized consonant is replaced by soft consonant (eg P2 -> P1)
		AsSoft,
	};

	// The class to enlist the phones of different configurations. We may be interested in vowel phones with stress marked
	// or consonant phones with marked softness. So the basic vowel phone A turns into A1 under stress.
	// The basic phone T turns into T1 when it is softened.
	class PG_EXPORTS PhoneRegistry
	{
		int nextPhoneId_ = 1; // the first phone gets Id=1
		std::vector<Phone> phoneReg_;
		std::vector<BasicPhone> basicPhones_;
		std::unordered_map<std::string, size_t> basicPhonesStrToId_;
		bool allowSoftConsonant_ = false;
		bool allowVowelStress_ = false;
		PalatalSupport palatalSupport_ = PalatalSupport::AsHard;
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
		PalatalSupport palatalSupport() const;
		void setPalatalSupport(PalatalSupport value);

		boost::optional<SoftHardConsonant> defaultSoftHardConsonant() const;
		boost::optional<bool> defaultIsVowelStressed() const;
	private:
		friend PG_EXPORTS void initPhoneRegistryUk(PhoneRegistry& phoneReg, bool allowSoftConsonant, bool allowVowelStress);
	};

	PG_EXPORTS void initPhoneRegistryUk(PhoneRegistry& phoneReg, bool allowSoftHardConsonant, bool allowVowelStress);

	// Returns true if the basic phone becomes half-softened (palatized) in certain letter combinations (before letter I).
	// Returns false if phone becomes soft in certain letter combinations.
	bool usuallyHardBasicPhone(const PhoneRegistry& phoneReg, Phone::BasicPhoneIdT basicPhoneId);

	// Checks whether the character is unvoiced (uk:глухий).
	PG_EXPORTS inline bool isUnvoicedCharUk(wchar_t ch);

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
		boost::wstring_view PronCode;

		// The actual phones of this pronunciation.
		std::vector<PhoneId> Phones;
	};

	// Represents all possible pronunciations of a word.
	// The word 'clothes' may be pronounced as clothes(1)='K L OW1 DH Z' or clothes(2)='K L OW1 Z'
	struct PhoneticWord
	{
		boost::wstring_view Word;
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
		ptrdiff_t seqOneWordCounter_ = 0;
		ptrdiff_t seqTwoWordsCounter_ = 0;

		/// If phonetic split is enabled, the splitter tries to divide the word in parts. Otherwise the whole word is used.
		bool allowPhoneticWordSplit_ = false;
		const WordPart* sentStartWordPart_;
		const WordPart* sentEndWordPart_;
		const WordPart* wordPartSeparator_ = nullptr;

		std::shared_ptr<SentenceParser> sentParser_;

		QTextStream* log_ = nullptr;
	public:
		bool outputCorpus_ = false;
		boost::filesystem::path corpusFilePath_;
		bool outputCorpusNormaliz_ = false; // prints each initial and normalized sentences
		boost::filesystem::path corpusNormalizFilePath_;
	public:
		UkrainianPhoneticSplitter();

		void bootstrapFromDeclinedWords(const std::unordered_map<std::wstring, std::unique_ptr<WordDeclensionGroup>>& declinedWords, const std::wstring& targetWord,
			const std::unordered_set<std::wstring>& processedWords);

		void gatherWordPartsSequenceUsage(const wchar_t* textFilesDir, long& totalPreSplitWords, int maxFileToProcess);

		const WordsUsageInfo& wordUsage() const;
		WordsUsageInfo& wordUsage();
		void printSuffixUsageStatistics() const;

		// Gets the number of sequences with 'wordSeqLength' words per sequence.
		long wordSeqCount(int wordsPerSeq) const;
		const WordPart* sentStartWordPart() const;
		const WordPart* sentEndWordPart() const;

		void setAllowPhoneticWordSplit(bool value);
		bool allowPhoneticWordSplit() const;

		void setSentParser(std::shared_ptr<SentenceParser> sentParser);
	private:
		void doWordPhoneticSplit(const wv::slice<wchar_t>& wordSlice, std::vector<const WordPart*>& wordParts);

		void analyzeSentence(const std::vector<wv::slice<wchar_t>>& words, std::vector<RawTextLexeme>& lexemes) const;

		bool checkGoodSentenceUkr(const std::vector<RawTextLexeme>& lexemes) const;

		// split words into slices
		void selectWordParts(const std::vector<RawTextLexeme>& lexemes, std::vector<const WordPart*>& wordParts, long& preSplitWords);

		void calcNGramStatisticsOnWordPartsBatch(std::vector<const WordPart*>& wordParts);

		// calculate statistics on word parts list
		void calcLangStatistics(const std::vector<const WordPart*>& wordParts);
	};

	// equality by value
	PG_EXPORTS bool operator == (const Pronunc& a, const Pronunc& b);
	PG_EXPORTS bool operator < (const Pronunc& a, const Pronunc& b);

	PG_EXPORTS bool getStressedVowelCharIndAtMostOne(boost::wstring_view word, int& stressedCharInd);
	PG_EXPORTS int syllableIndToVowelCharIndUk(boost::wstring_view word, int syllableInd);
	PG_EXPORTS int vowelCharIndToSyllableIndUk(boost::wstring_view word, int vowelCharInd);

	// Loads stressed vowel definitions from file.
	PG_EXPORTS std::tuple<bool, const char*> loadStressedSyllableDictionaryXml(boost::wstring_view dictFilePath, std::unordered_map<std::wstring, int>& wordToStressedSyllableInd);

#ifdef PG_HAS_JULIUS
	// Loads dictionary of word -> (phone list) from text file.
	// File usually has 'voca' extension.
	// File has Windows-1251 encodeding.
	// Each word may have multiple pronunciations (1-* relation); for now we neglect it and store data into map (1-1 relation).
	PG_EXPORTS std::tuple<bool, const char*> loadPronunciationVocabulary(const std::wstring& vocabFilePathAbs, std::map<std::wstring, std::vector<std::string>>& wordToPhoneList, const QTextCodec& textCodec); // TODO: remove
#endif
	PG_EXPORTS void parsePronId(boost::wstring_view pronId, boost::wstring_view& pronName);
	PG_EXPORTS bool isWordStressAssigned(const PhoneRegistry& phoneReg, const std::vector<PhoneId>& phoneIds);

	/// Checks that pronCode in 'clothes(1)' format and not 'clothes' (without round parantheses).
	bool isPronCodeDefinesStress(boost::wstring_view pronCode);

	/// Parses pronCode.
	/// For 'clothes(2)' pronCodeName='clothes', pronCodeStressSuffix='2'
	bool parsePronCodeNameAndStress(boost::wstring_view pronCode, boost::wstring_view* pronCodeName, boost::wstring_view* pronCodeStressSuffix);

	// 'brokenLines' has lines of the dictionary which can't be read.
	PG_EXPORTS std::tuple<bool, const char*> loadPhoneticDictionaryPronIdPerLine(const std::basic_string<wchar_t>& vocabFilePathAbs, const PhoneRegistry& phoneReg,
		const QTextCodec& textCodec, std::vector<PhoneticWord>& words, std::vector<std::string>& brokenLines,
		GrowOnlyPinArena<wchar_t>& stringArena);

	PG_EXPORTS std::tuple<bool, const char*> loadPhoneticDictionaryXml(boost::wstring_view filePath, const PhoneRegistry& phoneReg, std::vector<PhoneticWord>& phoneticDict, GrowOnlyPinArena<wchar_t>& stringArena);
	PG_EXPORTS std::tuple<bool, const char*> savePhoneticDictionaryXml(const std::vector<PhoneticWord>& phoneticDict, boost::wstring_view filePath, const PhoneRegistry& phoneReg);


	PG_EXPORTS void normalizePronunciationVocabulary(std::map<std::wstring, std::vector<Pronunc>>& wordToPhoneList, bool toUpper = true, bool trimNumbers = true);
	PG_EXPORTS void trimPhoneStrExtraInfos(const std::string& phoneStr, std::string& phoneStrTrimmed, bool toUpper, bool trimNumbers);

	PG_EXPORTS bool phoneToStr(const PhoneRegistry& phoneReg, int phoneId, std::string& result);
	PG_EXPORTS bool phoneToStr(const PhoneRegistry& phoneReg, PhoneId phoneId, std::string& result);
	PG_EXPORTS bool phoneListToStr(const PhoneRegistry& phoneReg, wv::slice<PhoneId> pron, std::string& result);

	// Parses space-separated list of phones.
	PG_EXPORTS boost::optional<PhoneId> parsePhoneStr(const PhoneRegistry& phoneReg, boost::string_view phoneStr);
	PG_EXPORTS bool parsePhoneList(const PhoneRegistry& phoneReg, boost::string_view phoneListStr, std::vector<PhoneId>& result);
	PG_EXPORTS std::tuple<bool, const char*> parsePronuncLinesNew(const PhoneRegistry& phoneReg, const std::wstring& prons, std::vector<PronunciationFlavour>& result, GrowOnlyPinArena<wchar_t>& stringArena);

	// Removes phone modifiers.
	PG_EXPORTS void updatePhoneModifiers(const PhoneRegistry& phoneReg, bool keepConsonantSoftness, bool keepVowelStress, std::vector<PhoneId>& phonesList);

	// Half made phone.
	// The incomplete phone's data, so that complete PhoneId can't be queried from phone registry.
	typedef decltype(static_cast<Phone*>(nullptr)->BasicPhoneId) BasicPhoneIdT;
	struct PhoneBillet
	{
		BasicPhoneIdT BasicPhoneId;
		boost::optional<CharGroup> DerivedFromChar = boost::none;
		boost::optional<SoftHardConsonant> SoftHard = boost::none; // valid for consonants only
		boost::optional<bool> IsStressed = boost::none; // valid for for vowels only
	};

	// Transforms text into sequence of phones.
	class PG_EXPORTS WordPhoneticTranscriber
	{
	public:
		typedef std::function<auto (boost::wstring_view, std::vector<int>&) -> bool> StressedSyllableIndFunT;
	private:
		const PhoneRegistry* phoneReg_;
		const std::wstring* word_;
		std::vector<char> isLetterStressed_; // -1 not initialized, 0=false, 1=true
		size_t letterInd_;
		std::wstring errString_;
		std::vector<PhoneBillet> billetPhones_;
		std::vector<PhoneId> outputPhones_;
		std::map<int, int> phoneIndToLetterInd_;
		StressedSyllableIndFunT stressedSyllableIndFun_ = nullptr;
	public:
		void transcribe(const PhoneRegistry& phoneReg, const std::wstring& word);
		void copyOutputPhoneIds(std::vector<PhoneId>& phoneIds) const;
		
		void setStressedSyllableIndFun(StressedSyllableIndFunT value);

		bool hasError() const;
		const std::wstring& errorString() const;
	private:
		// The current processed character of the word.
		inline wchar_t curLetter() const;
		inline boost::optional<bool> isCurVowelStressed() const;
		inline wchar_t offsetLetter(int offset) const;
		inline bool isFirstLetter() const;
		inline bool isLastLetter() const;
	public:
		int getVowelLetterInd(int vowelPhoneInd) const;
	private:
		PhoneBillet newConsonantPhone(const std::string& basicPhoneStr, boost::optional<SoftHardConsonant> SoftHard) const;
		PhoneBillet newVowelPhone(const std::string& basicPhoneStr, boost::optional<bool> isStressed) const;

		// Maps letter to a default phone candidate. More complicated cases are handled via rules.
		bool makePhoneFromCurLetterOneToOne(PhoneBillet& ph) const;

		void addPhone(const PhoneBillet& phone);
		void phoneBilletToStr(const PhoneBillet& phone, std::wstring& result) const;

		//
		void tryInitStressedVowels();
		bool ruleIgnore(); // do not require neighbourhood info
		bool ruleJi(); // do not require neighbourhood info
		bool ruleShCh(); // do not require neighbourhood info
		bool ruleDzDzh(); // progressive
		bool ruleZhDzh(); // progressive
		bool ruleNtsk(); // progressive
		bool ruleSShEtc(); // progressive
		bool ruleTsEtc(); // progressive
		bool ruleSoftSign(); // regressive
		bool ruleApostrophe(); // regressive
		bool ruleHardConsonantBeforeE(); // regressive
		bool ruleSoftConsonantBeforeI(); // regressive
		bool ruleDoubleJaJeJu(); // regressive
		bool ruleSoftConsonantBeforeJaJeJu(); // regressive
		bool ruleDampVoicedConsonantBeforeUnvoiced(); // progressive
		bool ruleDefaultSimpleOneToOneMap(); // do not require neighbourhood info

		// Checks whether the given phone is of the kind, which mutually softens each other.
		bool isMutuallySoftConsonant(Phone::BasicPhoneIdT basicPhoneId) const;
		// Rule: checks that if there are two consequent phones of specific type, and the second is soft, then the first becomes soft too.
		void postRulePairOfConsonantsSoftenEachOther();

		void postRuleAmplifyUnvoicedConsonantBeforeVoiced();

		void buildOutputPhones();
	};

	// Performs word transcription (word is represented as a sequence of phonemes).
	PG_EXPORTS std::tuple<bool, const char*> spellWordUk(const PhoneRegistry& phoneReg, const std::wstring& word, std::vector<PhoneId>& phones,
		WordPhoneticTranscriber::StressedSyllableIndFunT stressedSyllableIndFun = nullptr);

	template <typename MapT>
	void reshapeAsDict(const std::vector<PhoneticWord>& phoneticDictWordsList, MapT& phoneticDict)
	{
		for (const PhoneticWord& item : phoneticDictWordsList)
		{
			boost::wstring_view w = item.Word; // IMPORTANT: the word must not move in memory
			phoneticDict[w] = item;
		}
	}

	PG_EXPORTS void populatePronCodes(const std::vector<PhoneticWord>& phoneticDict, std::map<boost::wstring_view, PronunciationFlavour>& pronCodeToObj, std::vector<boost::wstring_view>& duplicatePronCodes);

	// Integrate new pronunciations from extra dictionary into base dictionary. Pronunciations with existent code are ignored.
	PG_EXPORTS void mergePhoneticDictOnlyNew(std::map<boost::wstring_view, PhoneticWord>& basePhoneticDict, const std::vector<PhoneticWord>& extraPhoneticDict);

	//
	PG_EXPORTS int phoneticSplitOfWord(wv::slice<wchar_t> word, boost::optional<WordClass> wordClass, int* pMatchedSuffixInd = nullptr);

	// <sil> pseudo word.
	PG_EXPORTS boost::wstring_view fillerSilence();

	// <s> pseudo word.
	PG_EXPORTS boost::wstring_view fillerStartSilence();

	// </s> pseudo word.
	PG_EXPORTS boost::wstring_view fillerEndSilence();

	// [sp] pseudo word.
	PG_EXPORTS boost::wstring_view fillerShortPause();

	// [inh] pseudo word.
	PG_EXPORTS boost::wstring_view fillerInhale();

	// [eee] (latin 'e') pseudo word.
	PG_EXPORTS boost::wstring_view fillerEee();

	// [yyy] (latin 'y') pseudo word.
	PG_EXPORTS boost::wstring_view fillerYyy();

	/// [clk] pseudo word.
	PG_EXPORTS boost::wstring_view fillerClick();

	/// [glt] pseudo word.
	PG_EXPORTS boost::wstring_view fillerGlottal();
}
