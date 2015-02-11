#include "PresentationHelpers.h"
#include <QDateTime>


namespace PticaGovorun
{
	QString timeStampNowForLog()
	{
		return QTime::currentTime().toString("hh:mm:ss");
	}

	QString formatLogLineWithTime(QString message)
	{
		QString msg = QString("[%1]: %2").arg(timeStampNowForLog()).arg(message);
		return msg;
	}
}
