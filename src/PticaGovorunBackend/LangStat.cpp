#include "stdafx.h"
#include <numeric>
#include "LangStat.h"
#include "assertImpl.h"
#include "SphinxModel.h"

namespace PticaGovorun
{
	WordPart::WordPart(const std::wstring& partText, WordPartSide partSide) 
		: partText_(partText),
		partSide_(partSide)
	{
	}

	int WordPart::id() const
	{
		return id_;
	}

	void WordPart::setId(int value)
	{
		id_ = value;
	}

	const std::wstring& WordPart::partText() const
	{
		return partText_;
	}

	WordPartSide WordPart::partSide() const
	{
		return partSide_;
	}

	size_t WordPart::hashKey() const
	{
		return stdext::hash_value(partSide()) ^ stdext::hash_value(partText());
	}

	bool WordPart::removed() const
	{
		return removed_;
	}

	void WordPart::setRemoved(bool removed)
	{
		removed_ = removed;
	}

	bool operator==(const WordPart& a, const WordPart& b)
	{
		if (a.partSide() == b.partSide())
			return std::less<std::wstring>()(a.partText(), b.partText());

		// the first goes the whole words
		if (a.partSide() == WordPartSide::WholeWord)
			return true;
		else if (b.partSide() == WordPartSide::WholeWord)
			return false;

		// then prefixes
		if (a.partSide() == WordPartSide::LeftPart)
			return true;
		else if (b.partSide() == WordPartSide::LeftPart)
			return false;

		// then internal part
		if (a.partSide() == WordPartSide::MiddlePart)
			return true;
		else if (b.partSide() == WordPartSide::MiddlePart)
			return false;

		// then suffixes
		if (a.partSide() == WordPartSide::RightPart)
			return true;
		else if (b.partSide() == WordPartSide::RightPart)
			return false;

		PG_Assert2(false, "Unrecognized WordPartSide enum");
		return false;
	}

	void toStringWithTilde(const WordPart& a, std::wstring& result, bool doSphinxMangle)
	{
		const WordPart* wordPart = &a;
		
		result.clear();
		if (wordPart->partSide() == WordPartSide::RightPart || wordPart->partSide() == WordPartSide::MiddlePart)
			result += L"~";
		
		if (doSphinxMangle)
			manglePronCodeSphinx(wordPart->partText(), result);
		else
			result += wordPart->partText();
		
		if (wordPart->partSide() == WordPartSide::LeftPart || wordPart->partSide() == WordPartSide::MiddlePart)
			result += L"~";
	}

	//

	WordSeqKey::WordSeqKey(std::initializer_list<int> wordIds)
	{
		PG_Assert2(wordIds.size() <= PartIds.size(), "Not enough space to store all word ids");
		PartIds.fill(0);
		std::copy_n(wordIds.begin(), wordIds.size(), PartIds.begin());
		PartCount = (int)wordIds.size();
	}

	size_t WordSeqKey::hashKey() const
	{
		size_t result = std::accumulate(PartIds.data(), PartIds.data() + PartCount, (size_t)0, [](size_t acc, int wordId)
		{
			std::hash<int> h;
			return acc ^ h(wordId);
		});
		return result;
	}

	bool operator==(const WordSeqKey& a, const WordSeqKey& b)
	{
		if (a.PartCount != b.PartCount)
			return false;

		for (int i = 0; i < a.PartCount; ++i)
			if (a.PartIds[i] != b.PartIds[i])
				return false;
		return true;
	}

	//

	WordSeqUsage::WordSeqUsage(WordSeqKey wordIds)
		:Key(wordIds)
	{
	}

	WordPart* WordsUsageInfo::pushWordPart(WordPart&& wordPart)
	{
		PG_Assert(wordPart.id() == 0);

		int wordId = nextWordPartId_;
		nextWordPartId_++;

		wordPart.setId(wordId);
		allWordParts_.insert(std::make_pair(wordId, std::move(wordPart)));
		WordPart* result = &allWordParts_.at(wordId);

		partTextToWordPart_.insert(std::make_pair(result->partText(), result));

		return result;
	}

	const WordPart* WordsUsageInfo::pushWordPart(WordPart&& wordPart) const
	{
		return const_cast<WordsUsageInfo*>(this)->pushWordPart(std::move(wordPart));
	}

	//int WordsUsageInfo::pushWordPart(std::unique_ptr<WordPart> wordPart)
	//{
	//	PG_Assert(wordPart->id() == 0);

	//	int wordId = nextWordPartId_;
	//	nextWordPartId_++;

	//	wordPart->setId(wordId);
	//	partTextToWordPart_.insert(std::make_pair(wordPart->partText(), wordId));
	//	allWordParts_.insert(std::make_pair(nextWordPartId_, std::move(wordPart)));

	//	return wordId;
	//}

	WordPart* WordsUsageInfo::wordPartByValue(const std::wstring& partText, WordPartSide partSide)
	{
		auto wordPartIt = partTextToWordPart_.find(partText);
		if (wordPartIt != partTextToWordPart_.end())
		{
			// find the set of parts with the same partText but differnt partSide
			auto partsFamilyIt = partTextToWordPart_.equal_range(partText);

			for (auto it = partsFamilyIt.first; it != partsFamilyIt.second; ++it)
			{
				if (it->second->partSide() == partSide)
				{
					return it->second;
				}
			}
		}
		return nullptr;
	}

	const WordPart* WordsUsageInfo::wordPartByValue(const std::wstring& partText, WordPartSide partSide) const
	{
		return const_cast<WordsUsageInfo*>(this)->wordPartByValue(partText, partSide);
	}

	WordPart* WordsUsageInfo::getOrAddWordPart(const std::wstring& partText, WordPartSide partSide, bool* wasAdded)
	{
		WordPart* existingWordPart = wordPartByValue(partText, partSide);
		if (existingWordPart != nullptr)
		{
			if (wasAdded != nullptr)
				*wasAdded = false;
			return existingWordPart;
		}

		if (wasAdded != nullptr)
			*wasAdded = true;

		//auto wordPart = std::make_unique<WordPart>(partText, partSide);
		//const WordPart* result = wordPart.get();
		//pushWordPart(std::move(wordPart));

		WordPart wordPart(partText, partSide);
		auto result = pushWordPart(std::move(wordPart));
		return result;
	}

	const WordPart* WordsUsageInfo::getOrAddWordPart(const std::wstring& partText, WordPartSide partSide, bool* wasAdded) const
	{
		return const_cast<WordsUsageInfo*>(this)->getOrAddWordPart(partText, partSide, wasAdded);
	}

	const WordPart* WordsUsageInfo::wordPartById(int wordPartId) const
	{
		auto it = allWordParts_.find(wordPartId);
		if (it == std::end(allWordParts_))
			return nullptr;

		return &it->second;
	}


	WordSeqUsage* WordsUsageInfo::getOrAddWordSequence(WordSeqKey wordIds, bool* wasAdded)
	{
		auto it = wordSeqKeyToUsage_.find(wordIds);
		if (it != wordSeqKeyToUsage_.end())
		{
			if (wasAdded != nullptr)
				*wasAdded = false;
			
			return &it->second;
		}
		
		if (wasAdded != nullptr)
			*wasAdded = true;

		WordSeqUsage usage(wordIds);
		wordSeqKeyToUsage_.insert(std::make_pair(wordIds, usage));
		return &wordSeqKeyToUsage_.at(wordIds);
	}

	const WordSeqUsage* WordsUsageInfo::getWordSequence(WordSeqKey wordIds) const
	{
		auto it = wordSeqKeyToUsage_.find(wordIds);
		if (it != wordSeqKeyToUsage_.end())
		{
			return &it->second;
		}
		return nullptr;
	}

	ptrdiff_t WordsUsageInfo::getWordSequenceUsage(WordSeqKey wordIds) const
	{
		auto it = wordSeqKeyToUsage_.find(wordIds);
		if (it != wordSeqKeyToUsage_.end())
		{
			return it->second.UsedCount;
		}
		// there is no usage data for given part
		return 0;
	}

	int WordsUsageInfo::wordPartsCount() const
	{
		return (int)allWordParts_.size();
	}

	int WordsUsageInfo::wordSeqCount() const
	{
		return (int)wordSeqKeyToUsage_.size();
	}

	void WordsUsageInfo::wordSeqCountPerSeqSize(wv::slice<ptrdiff_t> wordsSeqSizes) const
	{
		std::fill_n(std::begin(wordsSeqSizes), wordsSeqSizes.size(), 0);

		for (const auto& pair : wordSeqKeyToUsage_)
		{
			PG_Assert2(pair.first.PartCount > 0, "Word seq must contain some parts");
			int accumInd = pair.first.PartCount - 1;
			if (accumInd < wordsSeqSizes.size())
			{
				wordsSeqSizes[accumInd] += 1; // count words, not usage
			}
		}
	}

	void WordsUsageInfo::copyWordParts(std::vector<const WordPart*>& wordParts) const
	{
		wordParts.reserve(allWordParts_.size());
		for (const auto& pair : allWordParts_)
		{
			const WordPart* part = &pair.second;
			wordParts.push_back(part);
		}
	}

	void WordsUsageInfo::copyWordParts(std::vector<WordPart*>& wordParts)
	{
		wordParts.reserve(allWordParts_.size());
		for (auto& pair : allWordParts_)
		{
			WordPart* part = &pair.second;
			wordParts.push_back(part);
		}
	}

	void WordsUsageInfo::copyWordSeq(std::vector<WordSeqKey>& wordSeqItems)
	{
		for (auto& pair : wordSeqKeyToUsage_)
		{
			wordSeqItems.push_back(pair.first);
		}
	}

	void WordsUsageInfo::copyWordSeq(std::vector<const WordSeqUsage*>& wordSeqItems) const
	{
		for (auto& pair : wordSeqKeyToUsage_)
		{
			wordSeqItems.push_back(&pair.second);
		}
	}

	auto wordUsageOneSource(int wordPartId, const WordsUsageInfo& wordUsage, const std::map<int, ptrdiff_t>* wordPartIdToRecoveredUsage) -> ptrdiff_t
	{
		WordSeqKey seqKey({ wordPartId });
		auto seqUsage1 = wordUsage.getWordSequenceUsage(seqKey);
		
		if (wordPartIdToRecoveredUsage == nullptr)
			return seqUsage1;

		ptrdiff_t seqUsage2 = 0;
		auto usageIt = wordPartIdToRecoveredUsage->find(wordPartId);
		if (usageIt != std::end(*wordPartIdToRecoveredUsage))
			seqUsage2 = usageIt->second;

		bool onlyOne = (seqUsage1 > 0) ^ (seqUsage2 > 0);
		PG_Assert2(onlyOne, QString("Must be only one source of word's usage statistics. word=%1").arg(toQString(wordUsage.wordPartById(wordPartId)->partText())).toStdWString().c_str());

		return seqUsage1 > 0 ? seqUsage1 : seqUsage2;
	}
}
