#pragma once
#include <QString>
#include <string>
#include "PticaGovorunCore.h" // PG_EXPORTS

namespace PticaGovorun
{
	typedef long PGFrameInd;
	static const PGFrameInd NullSampleInd = -1;

	struct PG_EXPORTS AppHelpers
	{
		static QString     mapPath(QString appExeRelPath);
		static std::string mapPathStdString(QString appExeRelPath);
		static QString appIniFilePathAbs();
		static QString configParamQString(QString paramName, QString  defaultValue);
		static int  configParamInt(QString paramName, int  defaultValue);
		static bool configParamBool(QString paramName, bool defaultValue);
		static double configParamDouble(QString paramName, double defaultValue);
	};
}