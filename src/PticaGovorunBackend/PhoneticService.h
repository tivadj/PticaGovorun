#pragma once
#include <string>
#include <vector>
#include <tuple>
#include <unordered_set>
#include <map>
#include <QTextCodec>
#include <QTextStream>
#include <boost/optional.hpp>
#include "PticaGovorunCore.h"
#include "LangStat.h"
#include "TextProcessing.h"

namespace PticaGovorun
{
	enum class UkrainianPhoneId
	{
		Nil,
		P_A,
		P_B,
		P_CH,
		P_D,
		P_DZ,
		P_DZH,
		P_E,
		P_F,
		P_G,
		P_H,
		P_I,
		P_J,
		P_K,
		P_KH,
		P_L,
		P_M,
		P_N,
		P_O,
		P_P,
		P_R,
		P_S,
		P_SH,
		P_T,
		P_TS,
		P_U,
		P_V,
		P_Y,
		P_Z,
		P_ZH
	};

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
		//std::vector<int> Phones;
		std::vector<std::string> PhoneStrs;
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
		const WordPart* sentStartWordPart_;
		const WordPart* sentEndWordPart_;
		const WordPart* wordPartSeparator_ = nullptr;
	public:
		UkrainianPhoneticSplitter();

		void bootstrap(const std::unordered_map<std::wstring, std::unique_ptr<WordDeclensionGroup>>& declinedWords, const std::wstring& targetWord,
			const std::unordered_set<std::wstring>& processedWords, int& totalWordsCount);

		void buildLangModel(const wchar_t* textFilesDir, long& totalPreSplitWords, int maxFileToProcess = -1, bool outputCorpus = false);

		const WordsUsageInfo& wordUsage() const;
		WordsUsageInfo& wordUsage();
		void printSuffixUsageStatistics() const;

		// Gets the number of sequences with 'wordSeqLength' words per sequence.
		long wordSeqCount(int wordsPerSeq) const;
		const WordPart* sentStartWordPart() const;
		const WordPart* sentEndWordPart() const;
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
	PG_EXPORTS std::tuple<bool, const char*> loadPronunciationVocabulary(const std::wstring& vocabFilePathAbs, std::map<std::wstring, std::vector<std::string>>& wordToPhoneList, const QTextCodec& textCodec);
	PG_EXPORTS std::tuple<bool, const char*> loadPronunciationVocabulary2(const std::wstring& vocabFilePathAbs, std::map<std::wstring, std::vector<Pronunc>>& wordToPhoneList, const QTextCodec& textCodec);
	PG_EXPORTS void normalizePronunciationVocabulary(std::map<std::wstring, std::vector<Pronunc>>& wordToPhoneList);

	// Parses space-separated list of phones.
	PG_EXPORTS void parsePhoneListStrs(const std::string& phonesStr, std::vector<std::string>& result);
	PG_EXPORTS bool parsePhoneListStrs(const std::string& phonesStr, std::vector<UkrainianPhoneId>& result);

	PG_EXPORTS std::tuple<bool, const char*> parsePronuncLines(const std::wstring& prons, std::vector<PronunciationFlavour>& result);

	PG_EXPORTS bool phoneToStr(UkrainianPhoneId phone, std::string& result);
	PG_EXPORTS UkrainianPhoneId phoneStrToId(const std::string& phoneStr, bool* parseSuccess = nullptr);
	PG_EXPORTS bool pronuncToStr(const std::vector<UkrainianPhoneId>& pron, Pronunc& result);

	// Performs word transcription (word is represented as a sequence of phonemes).
	PG_EXPORTS std::tuple<bool,const char*> spellWord(const std::wstring& word, std::vector<UkrainianPhoneId>& phones);



	// Saves phonetic dictionary to file in YAML format.
	PG_EXPORTS void savePhoneticDictionaryYaml(const std::vector<PhoneticWord>& phoneticDict, const std::wstring& filePath);
	
	PG_EXPORTS std::tuple<bool, const char*> loadPhoneticDictionaryYaml(const std::wstring& filePath, std::vector<PhoneticWord>& phoneticDict);

	//
	PG_EXPORTS int phoneticSplitOfWord(wv::slice<wchar_t> word, boost::optional<WordClass> wordClass, int* pMatchedSuffixInd = nullptr);

	// Checks whether the character is unvoiced (uk:глухий).
	PG_EXPORTS inline bool isUnvoicedCharUk(wchar_t ch);
}
