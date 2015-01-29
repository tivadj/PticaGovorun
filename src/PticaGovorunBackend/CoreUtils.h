#pragma once
#include <string>
#include <sstream>

namespace PticaGovorun
{
	PG_EXPORTS void appendTimeStampNow(std::string& strBuf);
	PG_EXPORTS void appendTimeStampNow(std::stringstream& str);
}