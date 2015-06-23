#pragma once
#include <string>
#include <sstream>
#include <vector>
#include <ctime> // time_t, strftime
#include <QString>

#include <boost/utility/string_ref.hpp>

#include "PticaGovorunCore.h"

namespace PticaGovorun
{
	PG_EXPORTS void appendTimeStampNow(std::wstring& str);
	PG_EXPORTS void appendTimeStampNow(std::string& strBuf);
	PG_EXPORTS void appendTimeStampNow(std::stringstream& str);

	template <typename StreamT>
	void appendTimeStampNow(StreamT& str)
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

	// Copyies string into buffer and returns reference to it.
	PG_EXPORTS boost::wstring_ref toWStringRef(const QString& str, std::vector<wchar_t>& buff);
	PG_EXPORTS boost::wstring_ref toWStringRef(const QStringRef& str, std::vector<wchar_t>& buff);

	PG_EXPORTS QString toQString(boost::wstring_ref text);
	PG_EXPORTS std::wstring toStdWString(boost::wstring_ref text);
	PG_EXPORTS void toStdWString(boost::wstring_ref text, std::wstring& result);
}