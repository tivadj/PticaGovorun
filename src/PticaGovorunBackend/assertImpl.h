#pragma once

#include <iostream> // std::cerr
#include <sstream> // std::ostringstream
#include <cstdlib> // std::exit

namespace
{
	// NOTE: this allows to set a debugger's breakpoint to monitor bool value
	bool pgCond(bool c)
	{
		return c; // set a breakpoint here
	}
	// NOTE: this allow to break a debugger before exit
	void printMessageAndExit(const wchar_t* errStr) // [[noreturn]]
	{
		std::wcerr << errStr << std::endl;
		std::exit(-1);
	}
}

// msg(const char*)
// 'buf<<msg' is in a separate call to allow both char* and wchar_t* strings
#define PG_AssertHelper(assertCond, msg) if (pgCond(assertCond)) {} \
	else { \
		std::wostringstream buf; \
		buf << __FILE__ ":" << __LINE__ << "," __FUNCTION__ "(): "; \
		if (msg != nullptr) { \
			buf << msg; \
			buf << " -- "; \
		} \
		buf << #assertCond; \
		printMessageAndExit(buf.str().c_str()); \
	}

#define PG_Assert(assertCond)       PG_AssertHelper(assertCond, static_cast<const wchar_t*>(nullptr))
#define PG_Assert2(assertCond, msg) PG_AssertHelper(assertCond, msg)

// NOTE: PG_DbgAssert='no operation' if PG_DEBUG is not set
#ifdef PG_DEBUG
#define PG_DbgAssert(assertCond)      PG_AssertHelper(assertCond, static_cast<const wchar_t*>(nullptr))
#define PG_DbgAssert2(assertCond, msg) PG_AssertHelper(assertCond, msg)
#else
#define PG_DbgAssert(assertCond) /* noop */
#define PG_DbgAssert2(assertCond, msg) /* noop */
#endif
