#include "stdafx.h"
#include "CoreUtils.h"
#include <ctime> // time_t
#include <cassert>

namespace PticaGovorun
{

void appendTimeStampNow(std::string& str)
{
	std::stringstream buf;
	appendTimeStampNow(buf);

	str = buf.str();
}

void appendTimeStampNow(std::stringstream& str)
{
	time_t  t1 = time(0); // now time

	// Convert now to tm struct for local timezone
	tm now1;
	errno_t err = localtime_s(&now1, &t1);
	assert(err == 0);

	char buf[80];
	strftime(buf, sizeof(buf), "%Y%m%d%H%M%S", &now1); // 20120601070015

	str << buf;
}

boost::wstring_ref toWStringRef(const QString& str, std::vector<wchar_t>& buff)
{
	buff.resize(str.size());
	str.toWCharArray(buff.data());
	return boost::wstring_ref(buff.data(), str.size());
}

boost::wstring_ref toWStringRef(const QStringRef& str, std::vector<wchar_t>& buff)
{
	QString strQ = QString::fromRawData(str.constData(), str.size()); // no allocation
	return toWStringRef(strQ, buff);
}

QString toQString(boost::wstring_ref text)
{
	return QString::fromWCharArray(text.data(), (int)text.size());
}

std::wstring toStdWString(boost::wstring_ref text)
{
	return std::wstring(text.data(), text.size());
}

void toStdWString(boost::wstring_ref text, std::wstring& result)
{
	result.assign(text.data(), text.size());
}
}