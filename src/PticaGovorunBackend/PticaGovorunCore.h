#pragma once
#include <cassert>

#if defined PGAPI_EXPORTS
#define PG_EXPORTS __declspec(dllexport)
#else
//#define PG_EXPORTS __declspec(dllimport)
#define PG_EXPORTS
#endif

#define PG_Assert(assertCond) assert(assertCond)