#pragma once
#include <string>
#include <array>
#include "PticaGovorunCore.h"

namespace PticaGovorun
{
	// The Int which can be adorned with some useful debug information. In Release it can be trimmed to just Int.
	template <typename IdT, typename CharT, size_t BuffSize>
	struct IdWithDebugStr
	{
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

	template <typename T>
	struct NoDeleteFunctor
	{
		void operator()(T* pObj) const
		{
			// no op
		}
	};
}