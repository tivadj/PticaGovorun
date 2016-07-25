#include "stdafx.h"
#include "AppHelpers.h"
#include <QApplication>
#include <QDir>
#include <QSettings>
#include "assertImpl.h"

namespace PticaGovorun
{
	QString AppHelpers::mapPath(QString appExeRelPath)
	{
		QString appDir = QApplication::applicationDirPath();
		return QDir(appDir).filePath(appExeRelPath);
	}

	std::string AppHelpers::mapPathStdString(QString appExeRelPath)
	{
		return mapPath(appExeRelPath).toStdString();
	}

	namespace
	{
		QVariant configParamCore(QString paramName, QVariant defaultValue)
		{
			QString iniFilePath = QApplication::applicationFilePath() + ".ini";
			QSettings settings(iniFilePath, QSettings::IniFormat);
			QVariant valVar = settings.value(paramName, defaultValue);
			return valVar;
		}
	}

	int AppHelpers::configParamInt(QString paramName, int defaultValue)
	{
		QVariant valVar = configParamCore(paramName, QVariant(defaultValue));
		bool ok = false;
		int result = valVar.toInt(&ok);
		PG_Assert2(ok, "Variant to int conversion failed");
		return result;
	}

	bool AppHelpers::configParamBool(QString paramName, bool defaultValue)
	{
		QVariant valVar = configParamCore(paramName, QVariant(defaultValue));
		bool result = valVar.toBool();
		return result;
	}

	double AppHelpers::configParamDouble(QString paramName, double defaultValue)
	{
		QVariant valVar = configParamCore(paramName, QVariant(defaultValue));
		bool ok = false;
		double result = valVar.toDouble(&ok);
		PG_Assert2(ok, "Variant to double conversion failed");
		return result;
	}
}