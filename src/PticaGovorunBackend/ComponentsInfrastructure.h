#pragma once
#include <string>
#include <array>
#include <memory>
#include <unordered_map>
#include <QString>
#include <boost/utility/string_view.hpp>
#include "assertImpl.h"
#include "CoreUtils.h"
#include "assertImpl.h"

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
		size_t operator()(const IdWithDebugStr<IdT, CharT, BuffSize> idEx) const
		{
			return std::hash<IdT>{}(idEx.Id);
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
			clear();
		}
		GrowOnlyPinArena() {
			clear();
		}
		
		/// Returns object to the state after the initial construction.
		void clear()
		{
			rootLine_.NextLine = nullptr;
			rootLine_.Line.clear();
			currentLine_ = &rootLine_;
			linesNumDebug_ = 1;
		}

		void setLineSize(size_t value)
		{
			lineSize_ = value;
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
		size_t lineSize_ = 1024; // the size of every line in the arena
		size_t linesNumDebug_; // the total number of lines in the arena
	};

	template <typename CharT>
	bool registerWord(boost::basic_string_view<CharT, std::char_traits<CharT>> word, 
		GrowOnlyPinArena<CharT>& stringArena, 
		boost::basic_string_view<CharT, std::char_traits<CharT>>& arenaWord)
	{
		CharT* arenaWordPtr = stringArena.put(word.begin(), word.end());
		if (arenaWordPtr == nullptr)
			return false;
		arenaWord = boost::basic_string_view<CharT, std::char_traits<CharT>>(arenaWordPtr, word.size());
		return true;
	}

	template <typename CharT>
	void registerWordThrow(GrowOnlyPinArena<CharT>& stringArena,
		const boost::basic_string_view<CharT, std::char_traits<CharT>> word,
		boost::basic_string_view<CharT, std::char_traits<CharT>>* arenaWord)
	{
		CharT* arenaWordPtr = stringArena.put(word.begin(), word.end());
		if (arenaWordPtr == nullptr)
			throw std::bad_alloc();

		*arenaWord = boost::basic_string_view<CharT, std::char_traits<CharT>>(arenaWordPtr, word.size());
	}

	template <typename CharT>
	auto registerWordThrow2(GrowOnlyPinArena<CharT>& stringArena,
		const boost::basic_string_view<CharT, std::char_traits<CharT>> word) -> auto
	{
		CharT* arenaWordPtr = stringArena.put(word.begin(), word.end());
		if (arenaWordPtr == nullptr)
			throw std::bad_alloc();

		return boost::basic_string_view<CharT, std::char_traits<CharT>>(arenaWordPtr, word.size());
	}

	inline bool registerWord(QString word, GrowOnlyPinArena<wchar_t>& stringArena, boost::wstring_view& arenaWord)
	{
		std::vector<wchar_t> wordBuff;
		boost::wstring_view wordRef = toWStringRef(word, wordBuff);
		return registerWord(wordRef, stringArena, arenaWord);
	}

	/// The list of error messages.
	struct ErrMsgList
	{
		std::string utf8Msg;
		std::unique_ptr<ErrMsgList> next;
	};

	inline std::string str(const ErrMsgList& err)
	{
		std::string result;
		for (const ErrMsgList* cur = &err; cur != nullptr; )
		{
			const auto& str = cur->utf8Msg;
			result += str;
			result += ' '; // separator

			cur = cur->next != nullptr ? cur->next.get() : nullptr;
		}
		return result;
	}

	inline QString combineErrorMessages(const ErrMsgList& err)
	{
		QByteArray buf;
		for (const ErrMsgList* cur = &err; cur != nullptr; )
		{
			QString str = QString::fromUtf8(cur->utf8Msg.data(), cur->utf8Msg.size());
			buf += str;
			buf += ' '; // separator

			cur = cur->next != nullptr ? cur->next.get() : nullptr;
		}
		return QString(buf);
	}

	/// Adds a message to a list of error messages.
	/// The 'topErrMsg' pointer is changed to point to the new message.
	template <typename ErrorMsgInitializer>
	void pushErrorMsg(ErrMsgList* topErrMsg, ErrorMsgInitializer newMsgInitFun)
	{
		//static_assert([]() { static_cast<ErrorMsgInitializer*>(nullptr)->operator()(ErrMsgList{}); return true; }(), "failed function signature: fn ErrorMsgInitializer(ErrMsgList&)->void");
		if (topErrMsg == nullptr)
			return;

		ErrMsgList newTopMsg;
		newMsgInitFun(newTopMsg);

		newTopMsg.next = std::make_unique<ErrMsgList>();
		*newTopMsg.next = std::move(*topErrMsg);
		*topErrMsg = std::move(newTopMsg);
	}

}