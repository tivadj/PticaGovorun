#pragma once
#include <QString>
#include <string>

namespace PticaGovorun
{
	typedef long PGFrameInd;
	static const PGFrameInd NullSampleInd = -1;

	struct AppHelpers
	{
		static QString     mapPath(QString appExeRelPath);
		static std::string mapPathStdString(QString appExeRelPath);
	};
}