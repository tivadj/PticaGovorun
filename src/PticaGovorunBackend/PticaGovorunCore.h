#pragma once
#include <cassert>

namespace PticaGovorun {
	typedef long PGFrameInd;
	static const PGFrameInd PGFrameIndNull = -1;
}

#if defined PGAPI_EXPORTS
#define PG_EXPORTS __declspec(dllexport)
#else
//#define PG_EXPORTS __declspec(dllimport)
#define PG_EXPORTS
#endif

#define PG_Assert(assertCond) assert(assertCond)