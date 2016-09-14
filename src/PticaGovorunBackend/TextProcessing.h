#pragma once
#include <vector>
#include <unordered_map>
#include <memory>
#include <boost/optional.hpp>
#include <boost/utility/string_view.hpp>
#include <boost/filesystem.hpp>
#include "PticaGovorunCore.h"
#include "ClnUtils.h"
#include "ComponentsInfrastructure.h"
#include <gsl/span>

namespace PticaGovorun
{
	struct LetterUk
	{
		static constexpr const wchar_t apostrophe() { return L'\''; }
	};

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

	//
	enum class TextRunType
	{
		Alpha,       // alphabetical letters
		Digit,      // digits
		Punctuation, // any sort of single char puntation ,-;:
		PunctuationStopSentence, // single char punctuation which can be at the end of a sentence .!?…
		Whitespace   // space, tag, CR=\r, LF=\n
	};

	inline bool isAlphaOrDigit(TextRunType x)
	{
		return x == TextRunType::Alpha || x == TextRunType::Digit;
	}

	/// A sequene of chars with similar style. Usually words (a sequence of chars), numbers (a sequence of digits)
	/// or puntuations signs.
	struct RawTextRun
	{
		boost::wstring_view Str;
		TextRunType Type;
	};

	inline boost::wstring_view str(const RawTextRun& x) { return x.Str; }


	// Extrcts sentences from text. Skips commas and other punctuation marks.
	// Sentence is a sequence of words.
	class PG_EXPORTS TextParserNew
	{
	public:
		// Sets the buffer to read sentence (words) from.
		void setInputText(boost::wstring_view text);

		void setTextRunDest(std::vector<RawTextRun>* textRunDest);

		// Finds words' boundaries and return pointers to it.
		// The words are mutated to be lower
		// Returns false if there is no sentences left in the buffer.
		bool parseTokensTillDot(std::vector<RawTextRun>& tokens);
	private:
		void startTextRun(TextRunType curTextRun);
		void finishTextRunIfStarted();
		void finishTextRun();

		void continueRun(TextRunType curTextRun);

		void createSingleCharRun(TextRunType curTextRun);

		// Whether the character cursor points inside some word.
		inline bool isWordStarted() const;

	private:
		boost::wstring_view text_;
		int curCharInd_ = -1;
		int textRunStartInd_ = -1;
		boost::optional<TextRunType> textRunType_ = boost::none;
		std::vector<RawTextRun>* textRunDest_ = nullptr;

		// -1 if chars are copied without position shift
		// When there is a character in the middle of a word which must be removed from output
		// this index tracks the output char position
		//int outCharInd_;

		// True, if current character is an apostrophe and we expect the next character to be something different.
		//bool gotApostrophe_;

		// True, if current character is a hyphen and we expect the next character to be something different.
		//bool gotHyphen_;
	};

	// Creates copy of a string, specified by two pointers.
	PG_EXPORTS std::wstring toString(wv::slice<wchar_t> wordSlice);
	inline std::wstring toString(boost::wstring_view wordSlice) { return wordSlice.to_string(); }

	/// Converts to lower case.
	inline wchar_t pgToLower(wchar_t x);
	void pgToLower(boost::wstring_view x, std::wstring* result);
	
	/// токсичний -> toksychnyj
	PG_EXPORTS void transliterateUaToEn(boost::wstring_view strUa, std::wstring* result);

	PG_EXPORTS bool isApostrophe(wchar_t ch);
	PG_EXPORTS inline bool isDigitChar(wchar_t ch);
	/// Whether the letter is one of Roman Numerals (I,V,X,L,C,D,M).
	PG_EXPORTS inline bool isRomanNumeral(wchar_t ch);

	PG_EXPORTS inline bool isEnglishChar(wchar_t ch);
	PG_EXPORTS inline bool isEnglishVowel(wchar_t ch, bool includeY);

	// Returns true if the character is from latin charset and not from cyrillic.
	PG_EXPORTS inline bool isExclusiveEnglishChar(wchar_t ch);

	PG_EXPORTS inline bool isRussianChar(wchar_t ch);

	// Returns true if the char belongs to russian alphabet for sure. Returns false if the chars may belong to other cyrilic alphabets.
	PG_EXPORTS inline bool isExclusiveRussianChar(wchar_t ch);

	PG_EXPORTS inline bool isUkrainianConsonant(wchar_t ch);
	PG_EXPORTS inline bool isUkrainianVowel(wchar_t ch);
	PG_EXPORTS int vowelsCountUk(boost::wstring_view word);
	
	// Vowel or consonant.
	enum class CharGroup
	{
		Vowel,
		Consonant
	};
	PG_EXPORTS inline boost::optional<CharGroup> classifyUkrainianChar(wchar_t ch);

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
		Noun,         // іменник, n
		Pronoun,      // займенник, pron
		Preposition,  // прийменник(в), предлог, eng(after,for)
		Verb,         // глагол
		Adverb,       // прислівник
		VerbalAdverb, // дієприслівник
		Adjective,    // прикметник
		Participle,   // дієприкметник
		Numeral,      // числівник
		Conjunction,  // сполучник, conj
		Interjection, // вигук, int
		Particle,     // частка
		Irremovable,  // незмінюване слово
	};

	enum class WordCase
	{
		Nominative,  // називний, nom
		Genitive,    // родовий, gen
		Dative,      // давальний, dat
		Acusative,   // знахідний
		Instrumental,// орудний
		Locative,    // місцевий
		Vocative,    // кличний
	};

	inline const boost::wstring_view toString(WordCase x)
	{
		switch (x)
		{
		case WordCase::Nominative: return L"Nominative";
		case WordCase::Genitive: return L"Genitive";
		case WordCase::Dative: return L"Dative";
		case WordCase::Acusative: return L"Acusative";
		case WordCase::Instrumental: return L"Instrumental";
		case WordCase::Locative: return L"Locative";
		case WordCase::Vocative: return L"Vocative";
		default: throw std::invalid_argument("Undefined value of WordCase enum");
		}
	}

	enum class EntityMultiplicity
	{
		Singular, // sing
		Plural // pl
	};

	inline const boost::wstring_view toString(EntityMultiplicity x)
	{
		switch (x)
		{
		case EntityMultiplicity::Singular: return L"Singular";
		case EntityMultiplicity::Plural: return L"Plural";
		default: throw std::invalid_argument("Undefined value of EntityMultiplicity enum");
		}
	}

	enum class NumeralCardinality
	{
		Cardinal, // means quantity; ua=кількісний, ru=колличественный (три)
		Ordinal // means order, rank; ua=порядковий, ru=порядковый (третий)
		// Nominal? the same as Cardinal but aims to not show qunatity ("player number three")number
	};

	inline const boost::wstring_view toString(NumeralCardinality x)
	{
		switch (x)
		{
		case NumeralCardinality::Cardinal: return L"Cardinal";
		case NumeralCardinality::Ordinal: return L"Ordinal";
		default: throw std::invalid_argument("Undefined value of NumeralCardinality enum");
		}
	}

	enum class WordGender
	{
		Masculine, // m
		Feminine, // f
		Neuter // neutral, abbrev=n, середній
	};

	inline const boost::wstring_view toString(WordGender x)
	{
		switch (x)
		{
		case WordGender::Masculine: return L"Masculine";
		case WordGender::Feminine: return L"Feminine";
		case WordGender::Neuter: return L"Neuter";
		default: throw std::invalid_argument("Undefined value of WordGender enum");
		}
	}

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
		boost::optional<WordClass> WordClass;
		boost::optional<WordCase> Case;
		boost::optional<ActionTense> Tense;
		boost::optional<WordPerson> Person;
		boost::optional<EntityMultiplicity> Multiplicity;
		/// <summary>
		/// The beginning for of the verb.
		/// (інфінітив)
		/// </summary>
		boost::optional<bool> IsInfinitive;
		boost::optional<bool> Mandative; // the form of order
		boost::optional<WordGender> Gender;
		boost::optional<WordDegree> Degree;
		boost::optional<WordActiveOrPassive> ActiveOrPassive;
		WordDeclensionGroup* OwningWordGroup;

		inline bool isNotAvailable() const { return Name == L"-"; }
		
		bool validateConsistency() const
		{
			// rule: if Gender => Singular
			// exception: чисельник 'дві' жіночого роду (чисельник має рід - виняток), одже це - однина. Проте 'дві' це - множина.
			if (Gender != boost::none)
			{
				bool ok = Multiplicity == boost::none || Multiplicity == EntityMultiplicity::Singular;
				if (!ok)
					return false;
			}
			return true;
		}
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

		boost::optional<EntityMultiplicity> NumberCategory;

		boost::optional<WordPerfectiveAspect> PerfectiveAspect;

		boost::optional<WordTransitive> Transitive;

		boost::optional<WordPerson> Person;
	};

	PG_EXPORTS bool loadUkrainianWordDeclensionXml(const std::wstring& declensionDictPath, std::unordered_map<std::wstring, std::unique_ptr<WordDeclensionGroup>>& declinedWords, std::wstring* errMsg);

	// Calculates the number of unique word forms in the declination dictionary.
	PG_EXPORTS int uniqueDeclinedWordsCount(const std::unordered_map<std::wstring, std::unique_ptr<WordDeclensionGroup>>& declinedWords);

	enum class NumeralLexicalView
	{
		Word, // десять
		Arabic, // 10, 11, 12
		Roman   // X, XI, XII
	};

	/// The text language.
	enum class TextLanguage
	{
		Ukrainian, // 'uk'
		Russian,   // 'ru'
		English    // 'en'
	};

	//constexpr
	inline const boost::wstring_view toString(TextLanguage x)
	{
		switch (x)
		{
		case TextLanguage::Ukrainian: return L"Ukrainian";
		case TextLanguage::Russian: return L"Russian";
		case TextLanguage::English: return L"English";
		default: throw std::invalid_argument("Undefined value of TextLanguage enum");
		}
	}

	/// The part of text.
	struct RawTextLexeme
	{
		boost::wstring_view ValueStr;
		TextRunType RunType;
		boost::optional<TextLanguage> Lang;
		boost::optional<WordClass> Class;
		boost::optional<WordCase> Case;
		boost::optional<WordGender> Gender;
		boost::optional<EntityMultiplicity> Mulitplicity;
		boost::optional<NumeralLexicalView> NumeralLexView; // valid for Numerals
		boost::optional<NumeralCardinality> NumeralCardOrd; // valid for Numerals
		bool NonLiterate = false; // true for 'pa4ent'

		boost::wstring_view StrLowerCase() { return ValueStr; };

		void setGender(WordGender gender)
		{
			Gender = gender;
			Mulitplicity = EntityMultiplicity::Singular; // gender is available for singular entities
		}
	};

	/// Converts integer into the list of words.
	class PG_EXPORTS IntegerToUaWordConverter
	{
		struct BaseNumToWordMap
		{
			ptrdiff_t Num;
			boost::optional<WordGender> Gender;
			boost::wstring_view NumCardinal;
			boost::wstring_view NumOrdinal;

			BaseNumToWordMap(int num, const boost::wstring_view& num_cardinal, const boost::wstring_view& num_ordinal)
				: Num(num),
				NumCardinal(num_cardinal),
				NumOrdinal(num_ordinal)
			{
			}
			BaseNumToWordMap(int num, WordGender gender, const boost::wstring_view& num_cardinal, const boost::wstring_view& num_ordinal)
				: Num(num),
				Gender(gender),
				NumCardinal(num_cardinal),
				NumOrdinal(num_ordinal)
			{
			}
		};

		std::vector<BaseNumToWordMap> baseNumbers_;
		std::vector<BaseNumToWordMap> zerosWords_;
		std::unique_ptr<
			std::unordered_map<std::wstring, std::unique_ptr<WordDeclensionGroup>>> declinedWords_;
	public:
		IntegerToUaWordConverter();
		bool load(const boost::filesystem::path& declensionDictPath, std::wstring* errMsg);

		void convertBaseNumber(gsl::span<const BaseNumToWordMap> numInfos, int baseNum, NumeralCardinality cardOrd, WordCase wordCase, boost::optional<EntityMultiplicity> singPl,
		                        boost::optional<WordGender> gender, bool compoundNumeral, std::vector<RawTextLexeme>& lexemes);

		/// Converts number to a text.
		/// Returns false if the number is too large to represent as a text.
		bool convert(ptrdiff_t num, NumeralCardinality cardOrd, WordCase numCase, boost::optional<EntityMultiplicity> singPl, boost::optional<WordGender> gender, std::vector<RawTextLexeme>& lexemes, std::wstring* errMsg);

		/// @compoundOrdinal whether the triple should be the single word
		void convertOneTriple(ptrdiff_t num, NumeralCardinality cardOrd, WordCase numCase, boost::optional<EntityMultiplicity> singPl, boost::optional<WordGender> gender,
			bool compoundOrdinal, bool isLastTriple, std::vector<RawTextLexeme>& lexemes);
	};

	void split();

	/// Expands abbreviations into full textual form.
	class PG_EXPORTS AbbreviationExpanderUkr
	{
		std::vector<RawTextLexeme>* lexemes_ = nullptr;
		int curLexInd_ = -1;
		IntegerToUaWordConverter int2WordCnv_;
	public:
		std::shared_ptr<GrowOnlyPinArena<wchar_t>> stringArena_;
	public:
		bool load(const boost::filesystem::path& declensionDictPath, std::wstring* errMsg);
		void expandInplace(int startLexInd, std::vector<RawTextLexeme>& lexemes);
	private:
		inline RawTextLexeme& curLex();
		inline RawTextLexeme& nextLex(int stepsCount = 1);

		/// Returns true if there is a lexeme some steps ahead.
		inline bool hasNextLex(int stepsCount = 1) const;
		static bool isWord(const RawTextLexeme& x);

		bool ruleTemplate() { return true; }
		// eg. "пір^'я"
		bool ruleComposeWordWithApostropheInside();
		// eg. "в ^XX столітті"
		bool ruleExpandRomanNumber();
		// eg. "У ^90-ті роки"
		bool ruleNumberDiapasonAndEnding();
		/// eg. "У дев'яності ^рр."
		bool ruleNumberRokyRoku();
		/// "на XVI ^ст. припадає"
		bool ruleNumberStolittya();
		/// "^№146" or "^№ 146"
		bool ruleSignNAndNumber();
		/// eg. ty¬coon -> tycoon
		bool ruleRepairWordSplitByOptionalHyphen();
		/// "80—ті"->"80-ті"
		bool ruleUnifyHyphen();
		/// Converts number to words if context information is ready.
		bool ruleUpgradeNumberToWords();
		/// eg "від ^11 грудня 2003 ^р."
		bool ruleDayMonthYear();
	};

	/// Provides text stream in the form of blocks of different size.
	struct PG_EXPORTS TextBlockReader
	{
		virtual ~TextBlockReader() {}

		virtual bool nextBlock(std::vector<wchar_t>& buff) = 0;
	};

	class PG_EXPORTS SentenceParser
	{
	public:
		typedef std::function < auto (gsl::span<const RawTextLexeme>&) -> void> OnNextSentence;
	private:
		TextBlockReader* textReader_ = nullptr;
		std::shared_ptr<AbbreviationExpanderUkr> abbrevExpand_;
		std::unique_ptr<GrowOnlyPinArena<wchar_t>> stringArena_;
		OnNextSentence onNextSent_;
		//std::function < auto (gsl::span<RawTextLexeme>&) -> void> onNextSent_;
		//std::function < void (gsl::span<RawTextLexeme>&)> onNextSent_;
		std::vector<RawTextRun> curSentRuns_; // the initial sentence before the transformations are done
	public:
		explicit SentenceParser(size_t stringArenaLineSize);

		void setTextBlockReader(TextBlockReader* textReader);
		void setAbbrevExpander(std::shared_ptr<AbbreviationExpanderUkr> abbrevExpand);

		void setOnNextSentence(OnNextSentence onNextSent);

		/// Forms the sentence which possibly spans multiple text blocks.
		/// @return True if a sentence was constructed, or False if there is no text block to process.
		void run();

		gsl::span<const RawTextRun> curSentRuns() const;
	};

	PG_EXPORTS void registerInArena(GrowOnlyPinArena<wchar_t>& stringArena, gsl::span<const RawTextLexeme> inWords, std::vector<RawTextLexeme>& outWords);

	PG_EXPORTS void removeWhitespaceLexemes(std::vector<RawTextRun>& textRuns);
	PG_EXPORTS void removeWhitespaceLexemes(std::vector<RawTextLexeme>& lexemes);
}
