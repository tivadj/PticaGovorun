#pragma once
#include <vector>
#include <QString>
#include <boost/utility/string_view.hpp>
#include "PticaGovorunCore.h"

namespace PticaGovorun
{
	PG_EXPORTS void appendTimeStampNow(std::string& strBuf);
	PG_EXPORTS void appendTimeStampNow(std::stringstream& str);

	PG_EXPORTS boost::wstring_view trim(boost::wstring_view text);

	PG_EXPORTS void s2ws(boost::string_view strUtf8, std::wstring& target);
	PG_EXPORTS std::wstring s2ws(boost::string_view str);
	PG_EXPORTS void ws2s(boost::wstring_view wstr, std::string& targetUtf8);
	PG_EXPORTS std::string ws2s(boost::wstring_view wstr);

	// Copyies string into buffer and returns reference to it.
	PG_EXPORTS boost::wstring_view toWStringRef(const QString& str, std::vector<wchar_t>& buff);
	PG_EXPORTS boost::wstring_view toWStringRef(const QStringRef& str, std::vector<wchar_t>& buff);
	
	PG_EXPORTS void toWStringRef(const QString& str, std::wstring& buff);
	PG_EXPORTS void toWStringRef(const QStringRef& str, std::wstring& buff);

	PG_EXPORTS QString toQString(boost::wstring_view text);
	PG_EXPORTS std::wstring toStdWString(boost::wstring_view text);
	PG_EXPORTS void toStdWString(boost::wstring_view text, std::wstring& result);
}