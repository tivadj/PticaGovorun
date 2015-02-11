#pragma once
#include <string>
#include <vector>
#include <tuple>
#include <map>
#include <QTextCodec>
#include "PticaGovorunCore.h"
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

	PG_EXPORTS std::tuple<bool, const char*> parsePronuncLines(const std::wstring& prons, std::vector<PronunciationFlavour>& result);

	PG_EXPORTS bool phoneToStr(UkrainianPhoneId phone, std::string& result);
	PG_EXPORTS bool pronuncToStr(const std::vector<UkrainianPhoneId>& pron, Pronunc& result);
	PG_EXPORTS std::tuple<bool,const char*> spellWord(const std::wstring& word, std::vector<UkrainianPhoneId>& phones);


	// Saves phonetic dictionary to file in YAML format.
	PG_EXPORTS void savePhoneticDictionaryYaml(const std::vector<PhoneticWord>& phoneticDict, const std::wstring& filePath);
	
	PG_EXPORTS std::tuple<bool, const char*> loadPhoneticDictionaryYaml(const std::wstring& filePath, std::vector<PhoneticWord>& phoneticDict);
}