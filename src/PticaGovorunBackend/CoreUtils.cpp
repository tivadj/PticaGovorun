#include "stdafx.h"
#include <ctime> // time_t
#include <array>
#include "CoreUtils.h"
#include "assertImpl.h"

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
	PG_Assert(err == 0);

	char buf[80];
	strftime(buf, sizeof(buf), "%Y%m%d%H%M%S", &now1); // 20120601070015

	str << buf;
}

void appendTimeStampNow(std::wstring& str)
{
	time_t  t1 = time(0); // now time

	// Convert now to tm struct for local timezone
	tm now1;
	errno_t err = localtime_s(&now1, &t1);
	PG_Assert(err == 0);

	std::array<char, 80> buf;
	size_t len = strftime(buf.data(), buf.size(), "%Y%m%d%H%M%S", &now1); // 20120601070015
	
	str.assign(buf.data(), buf.data() + len);
}

boost::wstring_view toWStringRef(const QString& str, std::vector<wchar_t>& buff)
{
	buff.resize(str.size());
	str.toWCharArray(buff.data());
	return boost::wstring_view(buff.data(), str.size());
}

boost::wstring_view toWStringRef(const QStringRef& str, std::vector<wchar_t>& buff)
{
	QString strQ = QString::fromRawData(str.constData(), str.size()); // no allocation
	return toWStringRef(strQ, buff);
}

void toWStringRef(const QString& str, std::wstring& buff)
{
	buff.resize(str.size());
	if (str.isEmpty())
		return;
	str.toWCharArray(&buff[0]);
}

void toWStringRef(const QStringRef& str, std::wstring& buff)
{
	QString strQ = QString::fromRawData(str.constData(), str.size()); // no allocation
	toWStringRef(strQ, buff);
}

QString toQString(boost::wstring_view text)
{
	return QString::fromWCharArray(text.data(), (int)text.size());
}

std::wstring toStdWString(boost::wstring_view text)
{
	return std::wstring(text.data(), text.size());
}

void toStdWString(boost::wstring_view text, std::wstring& result)
{
	result.assign(text.data(), text.size());
}
}