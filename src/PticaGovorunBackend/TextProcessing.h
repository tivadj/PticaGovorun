#pragma once
#include <vector>
#include <unordered_map>
#include <memory>
#include <boost/optional.hpp>
#include "PticaGovorunCore.h"
#include "ClnUtils.h"

namespace PticaGovorun
{
	// Extrcts sentences from text. Skips commas and other punctuation marks.
	// Sentence is a sequence of words.
	class PG_EXPORTS TextParser
	{
	public:
		// Sets the buffer to read sentence (words) from.
		void setInputText(wv::slice<wchar_t> text);

		// Finds words' boundaries and return pointers to it.
		// The words are mutated to be lower
		// Returns false if there is no sentences left in the buffer.
		bool parseSentence(std::vector<wv::slice<wchar_t>>& words);

	private:
		void finishWord(std::vector<wv::slice<wchar_t>>& words);

		void onWordCharacter(wchar_t chw);

		void onWordBreak(wchar_t chw, std::vector<wv::slice<wchar_t>>& words);

		// Whether the character cursor points inside some word.
		bool isWordStarted() const;

	private:
		wv::slice<wchar_t> mutText_;
		int charInd_;
		int wordStartInd_;
		
		// -1 if chars are copied without position shift
		// When there is a character in the middle of a word which must be removed from output
		// this index tracks the output char position
		int outCharInd_; 

		// True, if current character is an apostrophe and we expect the next character to be something different.
		bool gotApostrophe_;

		// True, if current character is a hyphen and we expect the next character to be something different.
		bool gotHyphen_;
	};

	// Creates copy of a string, specified by two pointers.
	PG_EXPORTS std::wstring toString(wv::slice<wchar_t> wordSlice);

	PG_EXPORTS inline bool isDigitChar(wchar_t ch);

	PG_EXPORTS inline bool isEnglishChar(wchar_t ch);

	// Returns true if the character is from latin charset and not from cyrillic.
	PG_EXPORTS inline bool isExclusiveEnglishChar(wchar_t ch);

	PG_EXPORTS inline bool isRussianChar(wchar_t ch);

	// Returns true if the char belongs to russian alphabet for sure. Returns false if the chars may belong to other cyrilic alphabets.
	PG_EXPORTS inline bool isExclusiveRussianChar(wchar_t ch);

	PG_EXPORTS inline bool isUkrainianConsonant(wchar_t ch);
	PG_EXPORTS inline bool isUkrainianVowel(wchar_t ch);

	enum class ActionTense
	{
		Future,
		Present,
		Past
	};

	enum class WordPerson
	{
		Impersonal, // безособове,
		I,
		YouFamiliar, // ти
		He,
		She,
		It,
		We,
		YouRespectful, // ви
		They
	};

	enum class WordClass
	{
		Noun,         // іменник
		Pronoun,      // займенник
		Preposition,  // прийменник
		Verb,         // глагол
		Adverb,       // прислівник
		VerbalAdverb, // дієприслівник
		Adjective,    // прикметник
		Participle,   // дієприкметник
		Numeral,      // числівник
		Conjunction,  // сполучник
		Interjection, // вигук
		Particle,     // частка
		Irremovable,  // незмінюване слово
	};

	enum class WordCase
	{
		Nominative,  // називний
		Genitive,    // родовий
		Dative,      // давальний
		Acusative,   // знахідний
		Instrumental,// орудний
		Locative,    // місцевий
		Vocative,    // кличний
	};

	enum class WordNumberCategory
	{
		Singular,
		Plural
	};

	enum class WordGender
	{
		Masculine,
		Feminine,
		Neuter // neutral, середній
	};

	enum class WordDegree
	{
		Positive,    // звичайний
		Comparative, // вищий
		Superlative  // найвищий
	};

	enum class WordActiveOrPassive
	{
		Active,  // звичайний
		Passive, // вищий
	};

	/// <summary>
	/// Perfective referes to action which is completed or result is accomplished.
	/// </summary>
	// впливати (недоконаний) - вплинути (доконаний)
	enum class WordPerfectiveAspect
	{
		Perfective,  // доконаний (більшовизувати??)
		Imperfective,  // недоконаний (білити)
	};

	enum class WordTransitive
	{
		Transitive,  // перехідне
		Intransitive,  // неперехідне
	};

	class WordDeclensionGroup;

	struct WordDeclensionForm
	{
		std::wstring Name;
		boost::optional<ActionTense> Tense;
		boost::optional<WordPerson> Person;
		boost::optional<WordNumberCategory> NumberCategory;
		boost::optional<WordClass> WordClass;
		/// <summary>
		/// The beginning for of the verb.
		/// (інфінітив)
		/// </summary>
		boost::optional<bool> IsInfinitive;
		boost::optional<bool> Mandative; // the form of order
		boost::optional<WordCase> Case;
		boost::optional<WordGender> Gender;
		boost::optional<WordDegree> Degree;
		boost::optional<WordActiveOrPassive> ActiveOrPassive;
		WordDeclensionGroup* OwningWordGroup;
	};

	class WordDeclensionGroup
	{
	public:
		std::wstring Name;
		std::vector<WordDeclensionForm> Forms;
		
		boost::optional<WordClass> WordClass;

		/// <summary>
		/// If something that is represented by this word is alive or not.
		/// </summary>
		boost::optional<bool> IsBeing;

		/// <summary>
		/// Gender common to all word forms.
		/// </summary>
		boost::optional<WordGender> Gender;

		boost::optional<WordNumberCategory> NumberCategory;

		boost::optional<WordPerfectiveAspect> PerfectiveAspect;

		boost::optional<WordTransitive> Transitive;

		boost::optional<WordPerson> Person;
	};

	PG_EXPORTS inline std::tuple<bool, const char*> loadUkrainianWordDeclensionXml(const std::wstring& declensionDictPath, std::unordered_map<std::wstring, std::unique_ptr<WordDeclensionGroup>>& declinedWords);

	void split();
}
