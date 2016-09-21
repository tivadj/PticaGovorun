#pragma once
#include <memory>
#include <vector>
#include <unordered_map>
#include <boost/optional.hpp>
#include "LangStat.h"
#include "PhoneticService.h"

namespace PticaGovorun
{
	template <typename T, size_t FixedSize>
	struct ShortArray
	{
		std::array<T, FixedSize> Array;
		size_t ActualSize;
	};

	// The row (unigram or bigram) of ARPA model file.
	struct PG_EXPORTS NGramRow
	{
		int Order = -1; // order of n-gram in the output file; bigrams must be in the order of corresponding unigrams
		ShortArray<const WordPart*, 2> WordParts;
		long UsageCounter = 0;
		boost::optional<float> LogProb;
		long BackOffCounter = 0;
		boost::optional<float> BackOffLogProb;
		NGramRow* LowOrderNGram = nullptr; // pointer to the one part shorter n-gram

		auto partsCount() const -> int;
		auto part(int partInd) const -> const WordPart*;
	};

	// Represents ARPA language model.
	// Note: This class can't be dllexport-ed as a whole because it contains std::unique_ptr members
	class PG_EXPORTS ArpaLanguageModel
	{
		int gramMaxDimensions_; // 1=create unigram model, 2=bigram
		std::unordered_map<int, NGramRow*> wordPartIdToUnigram_;
		std::vector<std::unique_ptr<NGramRow>> unigrams_;
		std::vector<NGramRow> bigrams_;
	public:
		ArpaLanguageModel(int gramMaxDimensions);
		ArpaLanguageModel(const ArpaLanguageModel&) = delete;
		ArpaLanguageModel& operator=(const ArpaLanguageModel&) = delete;


		/// Generates ARPA (or Doug Paul) format.
		// LM must contain </s>, otherwise Sphinx emits error on LM loading
		// ERROR: "ngram_search.c", line 221: Language model/set does not contain </s>, recognition will fail
		// LM must contain <s>, otherwise Sphinx emits error on samples decoding
		// ERROR: "ngram_search.c", line 1157 : Couldn't find <s> in first frame
		/// @wordPartIdUsage usage statistics of wordParts not covered by text corpus
		void generate(const std::vector<PhoneticWord>& seedUnigrams, const std::map<int, ptrdiff_t>& wordPartIdToRecoveredUsage, const UkrainianPhoneticSplitter& phoneticSplitter);

		size_t ngramCount(int ngramDim) const;
		NGramRow* getUnigramFromWordPartId(int wordPartId);

		void setGramMaxDimensions(int value);
		const std::vector<std::unique_ptr<NGramRow>>& unigrams() const;
		const std::vector<NGramRow>& bigrams() const;
	private:
		void buildBigramsWholeWordsOnly(const UkrainianPhoneticSplitter& phoneticSplitter);
		void buildBigramsWordPartsAware(const UkrainianPhoneticSplitter& phoneticSplitter);
		
		/// Check total probability of all unigrams = 1
		void checkUnigramTotalProbOne() const;
	};

	PG_EXPORTS void writeArpaLanguageModel(const ArpaLanguageModel& langModel, const wchar_t* lmFilePath);

	/// Calculates the total usage of a list of words.
	ptrdiff_t wordsTotalUsage(const WordsUsageInfo& wordUsage, const std::vector<PhoneticWord>& words, const std::map<int, ptrdiff_t>* wordPartIdToUsage);
}