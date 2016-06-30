#pragma once
#include <QString>
#include <string>
namespace PticaGovorun
{
	struct AppHelpers
	{
		static QString     mapPath(QString appExeRelPath);
		static std::string mapPathStdString(QString appExeRelPath);
	};
}