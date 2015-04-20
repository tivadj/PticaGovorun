#pragma once
#include <cassert>
#include <opencv2/core.hpp>

namespace PticaGovorun {
	typedef long PGFrameInd;
	static const PGFrameInd NullSampleInd = -1;
}

#if defined PGAPI_EXPORTS
#define PG_EXPORTS __declspec(dllexport)
#else
//#define PG_EXPORTS __declspec(dllimport)
#define PG_EXPORTS
#endif

//#define PG_Assert(assertCond) assert(assertCond)
#define PG_Assert(assertCond) CV_Assert(assertCond)
#define PG_DbgAssert(assertCond) CV_Assert(assertCond)

//#define PG_Assert(assertCond) \
//#if defined(PG_DEBUG) \
//	CV_Assert(assertCond) \
//#endif

//inline void PG_Assert(assertCond)
//{
//#if defined(PG_DEBUG)
//	CV_Assert(assertCond)
//#endif
//}
