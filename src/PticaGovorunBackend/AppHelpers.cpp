#include "AppHelpers.h"
#include <QApplication>
#include <QDir>
#include <QSettings>
#include "assertImpl.h"

namespace PticaGovorun
{
	boost::filesystem::path AppHelpers::mapPathBfs(const boost::filesystem::path& appExeRelPath)
	{
		QString result = mapPath(QString::fromStdWString(appExeRelPath.wstring()));
		return result.toStdWString();
	}

	QString AppHelpers::mapPath(QString appExeRelPath)
	{
		QString appDir = QApplication::applicationDirPath();
		return QDir(appDir).filePath(appExeRelPath);
	}

	std::string AppHelpers::mapPathStdString(QString appExeRelPath)
	{
		return mapPath(appExeRelPath).toStdString();
	}

	QString AppHelpers::appIniFilePathAbs()
	{
		QString iniFilePath = QApplication::applicationFilePath() + ".ini";
		return iniFilePath;
	}

	namespace
	{
		QVariant configParamCore(QString paramName, QVariant defaultValue)
		{
			QString iniFilePath = AppHelpers::appIniFilePathAbs();
			QSettings settings(iniFilePath, QSettings::IniFormat);
			QVariant valVar = settings.value(paramName, defaultValue);
			return valVar;
		}
	}

	QString AppHelpers::configParamQString(QString paramName, QString defaultValue)
	{
		QVariant valVar = configParamCore(paramName, QVariant(defaultValue));
		QString result = valVar.toString();
		return result;
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