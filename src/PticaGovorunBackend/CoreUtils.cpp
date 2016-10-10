#include <ctime> // time_t
#include <array>
#include <locale> // std::wstring_convert
#include <codecvt> // std::codecvt_utf8_utf16
#include <ctime> // time_t, strftime
#include <string>
#include <sstream>
#include <functional>
#include <boost/utility/string_view.hpp>
#include "CoreUtils.h"
#include "assertImpl.h"

namespace PticaGovorun
{
	/// Formats time as string.
	template <size_t N>
	size_t formatTimeStampNow(std::array<char, N>& buf)
	{
		time_t  t1 = time(0); // now time

		// Convert now to tm struct for local timezone
		tm* now1 = std::localtime(&t1);
		PG_Assert(now1 != nullptr);

		return strftime(buf.data(), buf.size(), "%Y%m%d%H%M%S", now1); // 20120601070015
	}

	void appendTimeStampNow(std::string& str)
	{
		std::array<char, 80> buf;
		size_t len = formatTimeStampNow(buf);
		str.append(buf.data(), len);
	}

	void appendTimeStampNow(std::stringstream& str)
	{
		std::array<char, 80> buf;
		formatTimeStampNow(buf);
		str << buf.data();
	}

	boost::wstring_view trim(boost::wstring_view text)
	{
		auto isSpace = [](const wchar_t ch) { return ch == L' ' || ch == L'\t'; };
		int left = 0;
		while (left < (int)text.size() && isSpace(text[left]))
			left += 1;

		int right = -1 + (int)text.size();
		while (right > left && isSpace(text[right]))
			right -= 1;

		int len = right + 1 - left;
		if (len == 0) return boost::wstring_view();
		return text.substr(left, len);
	}

	std::wstring utf8s2ws(boost::string_view str)
	{
		std::wstring_convert<std::codecvt_utf8<wchar_t>> utf8_wchar;
		std::wstring strWChar = utf8_wchar.from_bytes(str.begin(), str.end());
		return strWChar;
	}

	void utf8s2ws(boost::string_view strUtf8, std::wstring& target)
	{
		std::wstring_convert<std::codecvt_utf8<wchar_t>> utf8_wchar;
		// TODO: unnecessary allocation: converter should just add to provided buffer
		std::wstring strWChar = utf8_wchar.from_bytes(strUtf8.begin(), strUtf8.end());
		target.append(strWChar.begin(), strWChar.end());
	}

	void toUtf8StdString(boost::wstring_view wstr, std::string& targetUtf8)
	{
		std::wstring_convert<std::codecvt_utf8<wchar_t>> utf8_wchar;
		// TODO: unnecessary allocation: converter should just add to provided buffer
		std::string strChar = utf8_wchar.to_bytes(wstr.begin(), wstr.end());
		targetUtf8.append(strChar.data(), strChar.size());
	}

	std::string toUtf8StdString(boost::wstring_view wstr)
	{
		std::wstring_convert<std::codecvt_utf8<wchar_t>> utf8_wchar;
		std::string strChar = utf8_wchar.to_bytes(wstr.begin(), wstr.end());
		return strChar;
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

	QString utf8ToQString(boost::string_view text)
	{
		return QString::fromUtf8(text.data(), text.size());
	}

	std::string toUtf8StdString(const QString& text)
	{
		QByteArray bytes = text.toUtf8();
		return std::string(bytes.data(), bytes.size());
	}

	std::string toStdString(boost::string_view text)
	{
		return std::string(text.data(), text.size());
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

	QString toQStringBfs(const boost::filesystem::path& text)
	{
		auto ws = text.wstring();
		return QString::fromStdWString(ws);
	}

	boost::filesystem::path toBfs(const QString& path)
	{
		auto ws = path.toStdWString();
		return boost::filesystem::path(ws);
	}
}