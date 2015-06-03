#pragma once
#include <string>
#include <array>
#include <memory>
#include <unordered_map>
#include <boost/utility/string_ref.hpp>
#include "PticaGovorunCore.h"
#include "CoreUtils.h"

namespace PticaGovorun
{
	// The Int which can be adorned with some useful debug information. In Release it can be trimmed to just Int.
	template <typename IdT, typename CharT, size_t BuffSize>
	struct IdWithDebugStr
	{
		typedef IdT id_type;
		typedef CharT char_type;
		static const size_t buff_size = BuffSize;

		IdT Id;
#if PG_DEBUG
		std::array<CharT, BuffSize> Str; // debug only

		void fillStr(const std::basic_string<CharT>& value)
		{
			PG_DbgAssert(value.size() <= Str.size());
			Str.fill(0);
			std::copy(value.cbegin(), value.cend(), Str.begin());
		}
#endif
	};

	template <typename IdT, typename CharT, size_t BuffSize>
	bool operator==(const IdWithDebugStr<IdT, CharT, BuffSize>& x, const IdWithDebugStr<IdT, CharT, BuffSize>& y)
	{
		return x.Id == y.Id;
	}
	template <typename IdT, typename CharT, size_t BuffSize>
	bool operator<(const IdWithDebugStr<IdT, CharT, BuffSize>& x, const IdWithDebugStr<IdT, CharT, BuffSize>& y)
	{
		return x.Id < y.Id;
	}

	template <typename IdT, typename CharT, size_t BuffSize>
	struct IdWithDebugStrHasher
	{
		size_t operator()(const IdWithDebugStr<IdT, CharT, BuffSize> idEx)
		{
			return std::hash_value(idEx.Id);
		}
	};

	template <typename IdWithDebugStrType>
	struct IdWithDebugStrTraits
	{
		typedef IdWithDebugStrHasher<typename IdWithDebugStrType::id_type, typename IdWithDebugStrType::char_type, IdWithDebugStrType::buff_size> hasher;
	};

	template <typename T>
	struct NoDeleteFunctor
	{
		void operator()(T* pObj) const
		{
			// no op
		}
	};

	struct BoostStringRefHasher
	{
		size_t operator()(const boost::wstring_ref strRef)
		{
			return std::hash_value(strRef.data());
		}
	};

	// Provides space for elements. Once an element is put in this arena, the element never moves.
	// A use case is to put a lot of small strings in arena and use pointers to strings, without pointers being invalidated.
	template <typename CharT>
	class GrowOnlyPinArena
	{
		// An allocation block to hold elements in it.
		struct ArenaLine
		{
			std::unique_ptr<ArenaLine> NextLine;
			std::vector<CharT> Line;
		};

	public:
		GrowOnlyPinArena(size_t lineSize) : lineSize_(lineSize) {
			PG_Assert(lineSize >= 0);
			currentLine_ = &rootLine_;
			currentLine_->Line.reserve(lineSize);
			linesNumDebug_ = 1;
		}

		// Puts data vector into the arena.
		// Returns pointer to the result data.
		CharT* put(const CharT* beg, const CharT* end)
		{
			size_t newSize = std::distance(beg, end);
			if (newSize > lineSize_)
				return nullptr;

			if (currentLine_->Line.size() + newSize > currentLine_->Line.capacity())
			{
				// restructure space
				auto newLine = std::make_unique<ArenaLine>();
				newLine->Line.reserve(lineSize_);

				currentLine_->NextLine = std::move(newLine);
				currentLine_ = currentLine_->NextLine.get();
				linesNumDebug_++;
			}

			CharT* dst = currentLine_->Line.data();
			std::advance(dst, currentLine_->Line.size());

			// allocate space
			currentLine_->Line.resize(currentLine_->Line.size() + newSize);

			std::copy(beg, end, dst);

			return dst;
		}

	private:
		ArenaLine* currentLine_;
		ArenaLine rootLine_; // root point to keep entire list of allocated lines
		size_t lineSize_; // the size of every line in the arena
		size_t linesNumDebug_; // the total number of lines in the arena
	};

	template <typename CharT>
	bool registerWord(boost::basic_string_ref<CharT, std::char_traits<CharT>> word, 
		GrowOnlyPinArena<CharT>& stringArena, 
		boost::basic_string_ref<CharT, std::char_traits<CharT>>& arenaWord)
	{
		CharT* arenaWordPtr = stringArena.put(word.begin(), word.end());
		if (arenaWordPtr == nullptr)
			return false;
		arenaWord = boost::basic_string_ref<CharT, std::char_traits<CharT>>(arenaWordPtr, word.size());
		return true;
	};

	inline bool registerWord(QString word, GrowOnlyPinArena<wchar_t>& stringArena, boost::wstring_ref& arenaWord)
	{
		std::vector<wchar_t> wordBuff;
		boost::wstring_ref wordRef = toWStringRef(word, wordBuff);
		return registerWord(wordRef, stringArena, arenaWord);
	};

}