#pragma once
#include <string>
#include <boost/filesystem.hpp>
#include "PticaGovorunCore.h" // PG_EXPORTS
#include "ComponentsInfrastructure.h"

namespace PticaGovorun
{
	PG_EXPORTS bool readAllText(const boost::filesystem::path& appExeRelPath, std::string& text, ErrMsgList* errMsg);
}
