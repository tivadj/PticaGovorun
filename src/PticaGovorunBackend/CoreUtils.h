#pragma once
#include <vector>
#include <QString>
#include <boost/utility/string_view.hpp>
#include "PticaGovorunCore.h"
#include <boost/filesystem.hpp>

namespace PticaGovorun
{
	PG_EXPORTS void appendTimeStampNow(std::string& strBuf);
	PG_EXPORTS void appendTimeStampNow(std::stringstream& str);

	PG_EXPORTS boost::wstring_view trim(boost::wstring_view text);

	PG_EXPORTS void utf8s2ws(boost::string_view strUtf8, std::wstring& target);
	PG_EXPORTS std::wstring utf8s2ws(boost::string_view str);
	PG_EXPORTS void toUtf8StdString(boost::wstring_view wstr, std::string& targetUtf8);
	PG_EXPORTS std::string toUtf8StdString(boost::wstring_view wstr);

	// Copyies string into buffer and returns reference to it.
	PG_EXPORTS boost::wstring_view toWStringRef(const QString& str, std::vector<wchar_t>& buff);
	PG_EXPORTS boost::wstring_view toWStringRef(const QStringRef& str, std::vector<wchar_t>& buff);
	
	PG_EXPORTS void toWStringRef(const QString& str, std::wstring& buff);
	PG_EXPORTS void toWStringRef(const QStringRef& str, std::wstring& buff);

	PG_EXPORTS QString utf8ToQString(boost::string_view text);
	PG_EXPORTS std::string toUtf8StdString(const QString& text);
	PG_EXPORTS std::string toStdString(boost::string_view text);
	PG_EXPORTS QString toQString(boost::wstring_view text);
	PG_EXPORTS std::wstring toStdWString(boost::wstring_view text);
	PG_EXPORTS void toStdWString(boost::wstring_view text, std::wstring& result);

	PG_EXPORTS QString toQStringBfs(const boost::filesystem::path& path);
	PG_EXPORTS boost::filesystem::path toBfs(const QString& path);
}
