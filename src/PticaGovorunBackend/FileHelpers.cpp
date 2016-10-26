#include "FileHelpers.h"
#include <boost/format.hpp>
#include <QFile>
#include <QTextStream>

namespace PticaGovorun
{
	bool readAllText(const boost::filesystem::path& appExeRelPath, std::string& text, ErrMsgList* errMsg)
	{
		auto filePathQ = toQString(appExeRelPath.wstring());
		QFile file(filePathQ);
		if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
		{
			if (errMsg != nullptr) errMsg->utf8Msg = str(boost::format("Can't read file %s") % appExeRelPath.string());
			return false;
		}
		QTextStream in(&file);
		QString textQ = in.readAll();
		QByteArray textBytes = textQ.toUtf8();
		text.append(textBytes.data(), textBytes.size());
		return true;
	}
}
