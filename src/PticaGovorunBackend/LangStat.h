#pragma once
#include <vector>
#include <array>
#include <map>
#include <functional>
#include <unordered_map>
#include <QString>
#include "PticaGovorunCore.h"
#include "ClnUtils.h" // wv::slice

namespace PticaGovorun
{
	enum class WordPartSide
	{
		// Word part is the prefix
		LeftPart,
		MiddlePart,
		
		//Word part is the suffix.
		RightPart,

		// It is not a part of the word but the whole word itself.
		WholeWord
	};

	class PG_EXPORTS WordPart
	{
		int id_ = 0;
		std::wstring partText_;
		WordPartSide partSide_;

		// true if word is denied
		bool removed_ = false;
	public:
		WordPart(const std::wstring& partText, WordPartSide partSide);
		//WordPart(const WordPart&) = delete; // TODO: who refers to copy ctr?

		int id() const;
		void setId(int value);
		const std::wstring& partText() const;
		WordPartSide partSide() const;
		size_t hashKey() const;
		bool removed() const;
		void setRemoved(bool removed);
	};

	bool operator==(const WordPart& a, const WordPart& b);
	PG_EXPORTS void toStringWithTilde(const WordPart& a, std::wstring& result, bool doSphinxMangle = false);

	// TODO: obsolete, use toString()
	template <typename StreatT>
	void printWordPart(const WordPart* wordPart, StreatT& stream)
	{
		if (wordPart->partSide() == WordPartSide::RightPart || wordPart->partSide() == WordPartSide::MiddlePart)
			stream << "~";
		stream << QString::fromStdWString(wordPart->partText());
		if (wordPart->partSide() == WordPartSide::LeftPart || wordPart->partSide() == WordPartSide::MiddlePart)
			stream << "~";
	}

	struct PG_EXPORTS WordSeqKey
	{
		std::array<int, 2> PartIds;

		int PartCount; // number of part ids

		WordSeqKey(std::initializer_list<int> wordIds);

		size_t hashKey() const;
	};

	bool operator==(const WordSeqKey& a, const WordSeqKey& b);

	// Represens a consecutive sequence of words.
	struct PG_EXPORTS WordSeqUsage
	{
		WordSeqKey Key;

		// Number of times this word was used in the text.
		ptrdiff_t UsedCount = 0;

		WordSeqUsage(WordSeqKey wordIds);
	};

	class PG_EXPORTS WordsUsageInfo
	{
		struct StringRefHasher
		{
			size_t operator()(const std::wstring* pStr) const
			{
				return std::hash<std::wstring>{}(*pStr);
			}
		};
		struct WordSeqKeyHasher
		{
			size_t operator()(const WordSeqKey& key) const
			{
				return key.hashKey();
			}
		};

	private:
		int nextWordPartId_ = 1;
		std::unordered_map<int, WordPart> allWordParts_; // TODO: wordPart as unique_ptr?
		std::unordered_multimap<std::wstring, WordPart*> partTextToWordPart_;
		std::unordered_map<WordSeqKey, WordSeqUsage, WordSeqKeyHasher> wordSeqKeyToUsage_;
	public:
		//int pushWordPart(std::unique_ptr<WordPart> wordPart);
		WordPart* pushWordPart(WordPart&& wordPart);
		const WordPart* pushWordPart(WordPart&& wordPart) const;

		const WordPart* wordPartById(int wordPartId) const;
		
		WordPart* wordPartByValue(const std::wstring& partText, WordPartSide partSide);
		const WordPart* wordPartByValue(const std::wstring& partText, WordPartSide partSide) const;

		WordPart* getOrAddWordPart(const std::wstring& partText, WordPartSide partSide, bool* wasAdded = nullptr);
		const WordPart* getOrAddWordPart(const std::wstring& partText, WordPartSide partSide, bool* wasAdded = nullptr) const;

	public:
		WordSeqUsage* getOrAddWordSequence(WordSeqKey wordIds, bool* wasAdded = nullptr);
		const WordSeqUsage* getWordSequence(WordSeqKey wordIds) const;
		ptrdiff_t getWordSequenceUsage(WordSeqKey wordIds) const;
		int wordPartsCount() const;
		int wordSeqCount() const;
		void wordSeqCountPerSeqSize(wv::slice<ptrdiff_t> wordsSeqSizes) const;
		void copyWordParts(std::vector<const WordPart*>& wordParts) const;
		void copyWordParts(std::vector<WordPart*>& wordParts);
		void copyWordSeq(std::vector<WordSeqKey>& wordSeqItems);
		void copyWordSeq(std::vector<const WordSeqUsage*>& wordSeqItems) const;
	};

	/// Queries word's usage statistics. Checks that usage number is either in usage object or usage map.
	auto wordUsageOneSource(int wordPartId, const WordsUsageInfo& wordUsage, const std::map<int, ptrdiff_t>* wordPartIdToRecoveredUsage)-> ptrdiff_t;
}

