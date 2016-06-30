#include "AppHelpers.h"
#include <QApplication>
#include <QDir>


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
}