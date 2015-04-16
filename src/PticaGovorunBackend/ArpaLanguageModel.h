#pragma once
#include <memory>
#include <vector>
#include <unordered_map>
#include <boost/optional.hpp>
#include "LangStat.h"
#include "PhoneticService.h"

namespace PticaGovorun
{
	namespace
	{
		template <typename T, size_t FixedSize>
		struct ShortArray
		{
			std::array<T, FixedSize> Array;
			size_t ActualSize;
		};
	}

	// The row (unigram or bigram) of ARPA model file.
	struct NGramRow
	{
		int Order = -1; // order of n-gram in the output file; bigrams must be in the order of corresponding unigrams
		ShortArray<const WordPart*, 2> WordParts;
		long UsageCounter = 0;
		boost::optional<float> LogProb;
		long BackOffCounter = 0;
		boost::optional<float> BackOffLogProb;
		NGramRow* LowOrderNGram = nullptr; // pointer to the one part shorter n-gram
	};

	// Represents ARPA language model.
	// Note: This class can't be dllexport-ed as a whole because it contains std::unique_ptr members
	class ArpaLanguageModel
	{
		int gramMaxDimensions_; // 1=create unigram model, 2=bigram
		std::unordered_map<int, NGramRow*> wordPartIdToUnigram_;
		std::vector<std::unique_ptr<NGramRow>> unigrams_;
		std::vector<NGramRow> bigrams_;
	public:
		PG_EXPORTS ArpaLanguageModel();
		ArpaLanguageModel(const ArpaLanguageModel&) = delete;
		PG_EXPORTS void generate(wv::slice<const WordPart*> seedUnigrams, const UkrainianPhoneticSplitter& phoneticSplitter);
		size_t ngramCount(int ngramDim) const;
		NGramRow* getUnigramFromWordPartId(int wordPartId);

		PG_EXPORTS void setGramMaxDimensions(int value);
		const std::vector<std::unique_ptr<NGramRow>>& unigrams() const;
		const std::vector<NGramRow>& bigrams() const;
	private:
		void buildBigramsWholeWordsOnly(const UkrainianPhoneticSplitter& phoneticSplitter);
		void buildBigramsWordPartsAware(const UkrainianPhoneticSplitter& phoneticSplitter);
	};

	PG_EXPORTS void writeArpaLanguageModel(const ArpaLanguageModel& langModel, const wchar_t* lmFilePath);
}