#include "stdafx.h"
#include "CoreUtils.h"
#include <ctime> // time_t
#include <cassert>

namespace PticaGovorun
{

void appendTimeStampNow(std::string& str)
{
	time_t  t1 = time(0); // now time

	// Convert now to tm struct for local timezone
	tm now1;
	errno_t err = localtime_s(&now1, &t1);
	assert(err == 0);

	char buf[80];
	strftime(buf, sizeof(buf), "%Y%m%d%H%M%S", &now1); // 20120601070015

	str = buf;
}

}