#include "stdafx.h"
#include "ArpaLanguageModel.h"
#include <iostream>
#include <QFile>
#include "PhoneticService.h"

namespace PticaGovorun
{
	const double LogProbMinusInf = -99; // Interpreted as log(0)

	auto assertLeftBackOffImpossible = []() {};

	void ArpaLanguageModel::buildBigramsWordPartsAware(const UkrainianPhoneticSplitter& phoneticSplitter)
	{
		// note, the order of bigrams must be the same as for unigrams

		const WordsUsageInfo& wordUsage = phoneticSplitter.wordUsage();
		bigrams_.reserve(unigrams_.size());

		size_t progressBigramStepSize = unigrams_.size() / 10;
		for (size_t uniIndex1 = 0; uniIndex1 < unigrams_.size(); ++uniIndex1)
		{
			if (uniIndex1 % progressBigramStepSize == 0)
				std::cout << "progress bigram uniIndex1=" << uniIndex1 / unigrams_.size() << std::endl;

			NGramRow& uni1 = *unigrams_[uniIndex1];

			for (size_t uniIndex2 = 0; uniIndex2 < unigrams_.size(); ++uniIndex2)
			{
				// assert: case uniIndex1 == uniIndex2 is allowed
				// (word1,word1) combinations is necessary to exclude impossible combination of word parts, eg (~R,~R)

				const NGramRow& uni2 = *unigrams_[uniIndex2];

				// bigram (<s>,~Right) is impossible; handled via back off
				if (uni1.WordParts.Array[0] == phoneticSplitter.sentStartWordPart() &&
					uni2.WordParts.Array[0]->partSide() == WordPartSide::RightPart)
				{
					// as <s> is ordinary word, all possible bigrams must be discarded by enumeration
					NGramRow bigram;
					bigram.LowOrderNGram = &uni1;
					bigram.WordParts.ActualSize = 2;
					bigram.WordParts.Array[0] = uni1.WordParts.Array[0];
					bigram.WordParts.Array[1] = uni2.WordParts.Array[0];
					bigram.LogProb = LogProbMinusInf;
					bigrams_.push_back(bigram);
					continue;
				}
				// bigram (Left~,</s>) is impossible; handled via back off
				if (uni1.WordParts.Array[0]->partSide() == WordPartSide::LeftPart &&
					uni2.WordParts.Array[0] == phoneticSplitter.sentEndWordPart())
				{
					assertLeftBackOffImpossible();
					continue;
				}

				// specific case to discard (</s>,*)
				// (</s>,<s>) is ok
				if (uni1.WordParts.Array[0] == phoneticSplitter.sentEndWordPart())
				{
					if (uni2.WordParts.Array[0] == phoneticSplitter.sentStartWordPart())
					{
						WordSeqKey seqKey({ uni1.WordParts.Array[0]->id(), uni2.WordParts.Array[0]->id() });
						long seqUsage = wordUsage.getWordSequenceUsage(seqKey);

						uni1.UsageCounter += seqUsage;

						NGramRow bigram;
						bigram.LowOrderNGram = &uni1;
						bigram.WordParts.ActualSize = 2;
						bigram.WordParts.Array[0] = uni1.WordParts.Array[0];
						bigram.WordParts.Array[1] = uni2.WordParts.Array[0];
						bigram.UsageCounter = seqUsage;
						bigrams_.push_back(bigram);
					}

					// bigram (</s>,*) is not possible
					continue;
				}

				//
				if (uni1.WordParts.Array[0]->partSide() == WordPartSide::LeftPart)
				{
					// 2nd: Left or Whole are handled via back off probability
					if (uni2.WordParts.Array[0]->partSide() == WordPartSide::RightPart)
					{
						NGramRow bigram;
						bigram.LowOrderNGram = &uni1;
						bigram.WordParts.ActualSize = 2;
						bigram.WordParts.Array[0] = uni1.WordParts.Array[0];
						bigram.WordParts.Array[1] = uni2.WordParts.Array[0];

						WordSeqKey seqKey({ uni1.WordParts.Array[0]->id(), uni2.WordParts.Array[0]->id() });
						long seqUsage = wordUsage.getWordSequenceUsage(seqKey);
						if (seqUsage > 0)
						{
							uni1.UsageCounter += seqUsage;
							bigram.UsageCounter = seqUsage;
						}
						else
						{
							// prohibit impossible bigram (L~,~R) by enumeration
							bigram.LogProb = LogProbMinusInf;
						}
						bigrams_.push_back(bigram);
					}
				}
				else if (uni1.WordParts.Array[0]->partSide() == WordPartSide::RightPart ||
					uni1.WordParts.Array[0]->partSide() == WordPartSide::WholeWord)
				{
					if (uni2.WordParts.Array[0]->partSide() == WordPartSide::RightPart)
					{
						// prohibit such case
						NGramRow bigram;
						bigram.LowOrderNGram = &uni1;
						bigram.WordParts.ActualSize = 2;
						bigram.WordParts.Array[0] = uni1.WordParts.Array[0];
						bigram.WordParts.Array[1] = uni2.WordParts.Array[0];
						bigram.LogProb = LogProbMinusInf;
						bigrams_.push_back(bigram);
					}
					else if (uni2.WordParts.Array[0]->partSide() == WordPartSide::LeftPart ||
						uni2.WordParts.Array[0]->partSide() == WordPartSide::WholeWord)
					{
						WordSeqKey seqKey({ uni1.WordParts.Array[0]->id(), uni2.WordParts.Array[0]->id() });
						long seqUsage = wordUsage.getWordSequenceUsage(seqKey);
						if (seqUsage > 0)
						{
							uni1.UsageCounter += seqUsage;

							NGramRow bigram;
							bigram.LowOrderNGram = &uni1;
							bigram.WordParts.ActualSize = 2;
							bigram.WordParts.Array[0] = uni1.WordParts.Array[0];
							bigram.WordParts.Array[1] = uni2.WordParts.Array[0];
							bigram.UsageCounter = seqUsage;
							bigrams_.push_back(bigram);
						}
					}
				}
			}
		}
	}

	void ArpaLanguageModel::buildBigramsWholeWordsOnly(const UkrainianPhoneticSplitter& phoneticSplitter)
	{
		const WordsUsageInfo& wordUsage = phoneticSplitter.wordUsage();
		bigrams_.reserve(wordUsage.wordSeqCount());

		std::vector<const WordSeqUsage*> wordSeqUsages;
		wordUsage.copyWordSeq(wordSeqUsages);
		for (const WordSeqUsage* seqUsage : wordSeqUsages)
		{
			if (seqUsage->Key.PartCount != 2) // 2=bigram
				continue;

			int wordPart1Id = seqUsage->Key.PartIds[0];
			int wordPart2Id = seqUsage->Key.PartIds[1];

			// two word statistic must map to existent unigrams
			NGramRow* pUni1 = getUnigramFromWordPartId(wordPart1Id);
			if (pUni1 == nullptr)
				continue;
			NGramRow* pUni2 = getUnigramFromWordPartId(wordPart2Id);
			if (pUni2 == nullptr)
				continue;

			pUni1->UsageCounter += seqUsage->UsedCount;

			const WordPart* wordPart1 = wordUsage.wordPartById(wordPart1Id);
			PG_Assert(wordPart1 != nullptr);

			const WordPart* wordPart2 = wordUsage.wordPartById(wordPart2Id);
			PG_Assert(wordPart2 != nullptr);

			NGramRow bigram;
			bigram.LowOrderNGram = pUni1;
			bigram.WordParts.ActualSize = 2;
			bigram.WordParts.Array[0] = wordPart1;
			bigram.WordParts.Array[1] = wordPart2;
			bigram.UsageCounter = seqUsage->UsedCount;
			bigrams_.push_back(bigram);
		}

		// note, the order of bigrams must be the same as for unigrams
		std::sort(bigrams_.begin(), bigrams_.end(), [this](const NGramRow& a, const NGramRow& b)
		{
			if (a.LowOrderNGram->Order != b.LowOrderNGram->Order)
				return a.LowOrderNGram->Order < b.LowOrderNGram->Order;

			// compare by next word part
			int wordPart1Id = a.WordParts.Array[1]->id();
			int wordPart2Id = b.WordParts.Array[1]->id();

			NGramRow* pUni1 = getUnigramFromWordPartId(wordPart1Id);
			PG_Assert(pUni1 != nullptr);
			NGramRow* pUni2 = getUnigramFromWordPartId(wordPart2Id);
			PG_Assert(pUni2 != nullptr);

			return pUni1->Order < pUni2->Order;
		});
	}

	ArpaLanguageModel::ArpaLanguageModel()
	{
	}

	// Generates ARPA (or Doug Paul) format.
	// LM must contain </s>, otherwise Sphinx emits error on LM loading
	// ERROR: "ngram_search.c", line 221: Language model/set does not contain </s>, recognition will fail
	// LM must contain <s>, otherwise Sphinx emits error on samples decoding
	// ERROR: "ngram_search.c", line 1157 : Couldn't find <s> in first frame
	void ArpaLanguageModel::generate(wv::slice<const WordPart*> seedUnigrams, const UkrainianPhoneticSplitter& phoneticSplitter)
	{
		const WordsUsageInfo& wordUsage = phoneticSplitter.wordUsage();

		long unigramTotalUsageCounter = 0;
		int unusedUnigramsCount = 0;
		for (const WordPart* wordPart : seedUnigrams)
		{
			WordSeqKey seqKey({ wordPart->id() });
			long seqUsage = wordUsage.getWordSequenceUsage(seqKey);
			unigramTotalUsageCounter += seqUsage;
			if (seqUsage == 0)
				unusedUnigramsCount++;
		}

		// assume that processed text covers P portion of the whole language (0..1)
		const float TextStatisticCover = 0.9;
		//const float TextStatisticCover = 1;

		// create unigrams

		unigrams_.reserve(seedUnigrams.size());
		int unigramOrder = 0;
		for (const WordPart* wordPart : seedUnigrams)
		{
			WordSeqKey seqKey({ wordPart->id() });
			long seqUsage = wordUsage.getWordSequenceUsage(seqKey);
			if (seqUsage == 0)
			{
				// assign it some minimal usage
				seqUsage = 1;
			}
			PG_Assert(seqUsage > 0);

			double prob = seqUsage / (double)unigramTotalUsageCounter;
			PG_Assert(prob > 0 && prob <= 1);
			double logProb = std::log10(prob);

			// attempt to block isolated right parts
			// bad, sentences become shorter even more
			//if (wordPart->partSide() == WordPartSide::RightPart)
			//	logProb = LogProbMinusInf; 

			auto gram = std::make_unique<NGramRow>();
			gram->Order = unigramOrder++;
			gram->WordParts.ActualSize = 1;
			gram->WordParts.Array[0] = wordPart;
			gram->LogProb = logProb;
			wordPartIdToUnigram_.insert({ wordPart->id(), gram.get() }); // assert gram != null
			unigrams_.push_back(std::move(gram));
		}

		if (gramMaxDimensions_ == 1)
			return;

		// TODO: merge this two routines
		//buildBigramsWordPartsAware(phoneticSplitter);
		buildBigramsWholeWordsOnly(phoneticSplitter);

		// set backoff probabilities
		for (std::unique_ptr<NGramRow>& uniPtr : unigrams_)
		{
			NGramRow& uni = *uniPtr;
			PG_Assert(uni.WordParts.ActualSize == 1);
			// unigram (<s>)
			if (uni.WordParts.Array[0] == phoneticSplitter.sentStartWordPart())
			{
				//uni.LogProb = LogProbMinusInf; // start the new sentence the latest (the lowest priority)

				//double backOff = 0; // the first word of a sentence may be any word, not just specified one
				// double backOff = 1.4124; // from CMU LM for English
				double backOff = std::log10(1 - TextStatisticCover);
				uni.BackOffLogProb = backOff;
				continue;
			}
			// unigram (</s>)
			//if (uni.WordParts.Array[0] == phoneticSplitter.sentEndWordPart())
			//{
			//	// this back off value is ignored if there is no (</s>, *) bigram
			//	uni.BackOffLogProb = 0; // all prob are in this case
			//	//uni.BackOffLogProb = LogProbMinusInf; // all prob are described in bigrams
			//	continue;
			//}

			if (uni.WordParts.Array[0]->partSide() == WordPartSide::LeftPart)
			{
				// only right part can go after left part; other cases must be prohibited
				assertLeftBackOffImpossible();
				uni.BackOffLogProb = LogProbMinusInf;
			}
			else if (uni.WordParts.Array[0]->partSide() == WordPartSide::RightPart ||
				uni.WordParts.Array[0]->partSide() == WordPartSide::WholeWord)
			{
				// allow any word composition if text statistic is not available for some word sequence
				//uni.BackOffLogProb = 0; // prob=100%
				uni.BackOffLogProb = std::log10(1 - TextStatisticCover);
			}
			else
			{
				PG_Assert(false);
				if (uni.BackOffLogProb != nullptr)
				{
					// back off > 0 means eg. L~ ~R which do not match
					//PG_Assert(gram.BackOffCounter == 0);
				}
				else
				{
					PG_Assert(uni.BackOffCounter > 0); // TODO:
					int unigramTotalBackOffCounter = 10;
					double prob = uni.BackOffCounter / (double)unigramTotalBackOffCounter;
					prob *= (1 - TextStatisticCover);
					PG_Assert(prob > 0);
					double logProb = std::log10(prob);
					uni.BackOffLogProb = logProb;
				}
			}
		}
		for (NGramRow& gram : bigrams_)
		{
			PG_Assert(gram.WordParts.ActualSize == 2);
			if (gram.LogProb == nullptr)
			{
				PG_Assert(gram.LowOrderNGram != nullptr);
				double prob = gram.UsageCounter / (double)gram.LowOrderNGram->UsageCounter;

				if (gram.WordParts.Array[0]->partSide() == WordPartSide::LeftPart)
				{
					// Everything is concentrated in the (L~,~R) pairs; backOff(L~)=-99
					// This way we prohibit (L~,~R) words, which are not enumerated - which is bad.
					// But we also prohibit (L~,L~) or (L~,W) - which is good.
					// Do not change probability!
				}
				else if (gram.WordParts.Array[0]->partSide() == WordPartSide::RightPart ||
					gram.WordParts.Array[0]->partSide() == WordPartSide::WholeWord)
				{
					// backOff(uni1)=log(1-TextStatisticCover), so here we should reduce max probability to TextStatisticCover
					prob *= TextStatisticCover;
				}
				else if (gram.WordParts.Array[0] == phoneticSplitter.sentEndWordPart() ||
					gram.WordParts.Array[0] == phoneticSplitter.sentStartWordPart())
				{

				}
				PG_Assert(prob >= 0 && prob <= 1);
				double logProb = std::log10(prob);
				gram.LogProb = logProb;
			}
		}
	}

	size_t ArpaLanguageModel::ngramCount(int ngramDim) const
	{
		PG_Assert(ngramDim == 1 || ngramDim == 2);
		if (ngramDim == 1)
			return unigrams_.size();
		if (ngramDim == 2)
			return bigrams_.size();
		return 0;
	}

	NGramRow* ArpaLanguageModel::getUnigramFromWordPartId(int wordPartId)
	{
		auto gramIt = wordPartIdToUnigram_.find(wordPartId);
		if (gramIt != wordPartIdToUnigram_.end())
			return gramIt->second;
		return nullptr;
	}

	void ArpaLanguageModel::setGramMaxDimensions(int value)
	{
		gramMaxDimensions_ = value;
	}

	const std::vector<std::unique_ptr<NGramRow>>& ArpaLanguageModel::unigrams() const
	{
		return unigrams_;
	}

	const std::vector<NGramRow>& ArpaLanguageModel::bigrams() const
	{
		return bigrams_;
	}

	void writeArpaLanguageModel(const ArpaLanguageModel& langModel, const wchar_t* lmFilePath)
	{
		QFile lmFile(QString::fromStdWString(lmFilePath));
		if (!lmFile.open(QIODevice::WriteOnly | QIODevice::Text))
			return;
		QTextStream dumpFileStream(&lmFile);
		dumpFileStream.setCodec("UTF-8");
		dumpFileStream << R"out(\data\)out" << "\n";
		dumpFileStream << "ngram 1=" << langModel.ngramCount(1) << "\n";
		dumpFileStream << "ngram 2=" << langModel.ngramCount(2) << "\n"; // +2 for sentence start/end

		auto printNGram = [&dumpFileStream](const NGramRow& gram)
		{
			dumpFileStream.setFieldWidth(6);
			dumpFileStream << gram.LogProb.get();
			dumpFileStream.setFieldWidth(0);
			dumpFileStream << " ";

			for (int partInd = 0; partInd < gram.WordParts.ActualSize; ++partInd)
			{
				const WordPart* oneWordPart = gram.WordParts.Array[partInd];
				printWordPart(oneWordPart, dumpFileStream);
				dumpFileStream << " ";
			}

			if (gram.BackOffLogProb != nullptr)
			{
				dumpFileStream.setFieldWidth(6);
				dumpFileStream << gram.BackOffLogProb.get();
				dumpFileStream.setFieldWidth(0);
				dumpFileStream << " ";
			}

			dumpFileStream << "\n";
		};

		dumpFileStream << "\n"; // blank line
		dumpFileStream << "\\1-grams:" << "\n";
		for (const std::unique_ptr<NGramRow>& gram : langModel.unigrams())
			printNGram(*gram);

		dumpFileStream << "\n"; // blank line
		dumpFileStream << "\\2-grams:" << "\n";
		for (const NGramRow& gram : langModel.bigrams())
			printNGram(gram);

		dumpFileStream << "\n"; // blank line
		dumpFileStream << R"out(\end\)out" << "\n";
	}
}