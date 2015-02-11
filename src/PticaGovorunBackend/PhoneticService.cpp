#include "stdafx.h"
#include "PhoneticService.h"
#include <array>
#include <QFile>
#include <QDebug>


namespace PticaGovorun
{
	namespace
	{
		const wchar_t Letter_Hyphen = L'-';
		const wchar_t Letter_Apostrophe = L'\'';
		const wchar_t Letter_D = L'ä';
		const wchar_t Letter_JI = L'¿';
		const wchar_t Letter_JE = L'º';
		const wchar_t Letter_ZH = L'æ';
		const wchar_t Letter_Z = L'ç';
		const wchar_t Letter_SHCH = L'ù';
		const wchar_t Letter_SoftSign = L'ü';
		const wchar_t Letter_JU = L'þ';
		const wchar_t Letter_JA = L'ÿ';
	}

	void Pronunc::setPhones(const std::vector<std::string>& phones)
	{
		for (const std::string& ph : phones)
		{
			pushBackPhone(ph);
		}
	}

	void Pronunc::pushBackPhone(const std::string& phone)
	{
		if (!StrDebug.empty())
			StrDebug.push_back(' ');
		std::copy(std::begin(phone), std::end(phone), std::back_inserter(StrDebug));
		Phones.push_back(phone);
	}

	bool operator == (const Pronunc& a, const Pronunc& b)
	{
		bool eqSize = a.Phones.size() == b.Phones.size();
		if (!eqSize)
			return false;
		for (int i = 0; i < a.Phones.size(); ++i)
		{
			auto p1 = a.Phones[i];
			auto p2 = b.Phones[i];
			bool eqPhones = p1 == p2;
			if (!eqPhones)
				return false;
		}
		return true;
	}

	bool operator < (const Pronunc& a, const Pronunc& b)
	{
		size_t minSize = std::min(a.Phones.size(), b.Phones.size());
		for (int i = 0; i < minSize; ++i)
		{
			auto p1 = a.Phones[i];
			auto p2 = b.Phones[i];
			bool eq = p1 == p2;
			if (!eq)
				return std::less<>()(p1, p2);
		}
		if (     a.Phones.size() < b.Phones.size())
			return true;
		else if (a.Phones.size() > b.Phones.size())
			return false;
		return false;
	}

	std::tuple<bool, const char*> loadPronunciationVocabulary(const std::wstring& vocabFilePathAbs, std::map<std::wstring, std::vector<std::string>>& wordToPhoneList, const QTextCodec& textCodec)
	{
		// file contains text in Windows-1251 encoding
		QFile file(QString::fromStdWString(vocabFilePathAbs));
		if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
			return std::make_tuple(false, "Can't open file");

		// 
		std::array<char, 1024> lineBuff;

		// each line has a format:
		// sure\tsh u e\n
		while (true) {
			auto readBytes = file.readLine(lineBuff.data(), lineBuff.size());
			if (readBytes == -1) // EOF
			{
				break;
			}

			// read the first word

			char* pMutStr = lineBuff.data(); // note, strtok modifies the buffer
			char* pMutStrNext = nullptr;

			const char* DictDelim = " \t\n";

			// strtok seems to be quicker than std::regex or QString::split approaches
			pMutStr = strtok_s(pMutStr, DictDelim, &pMutStrNext);
			if (pMutStr == nullptr)
			{
				// the line contains only the whitespace
				continue;
			}

			QString word = textCodec.toUnicode(pMutStr);

			// read the tail of phones
			std::vector<std::string> phones;
			while (true)
			{
				pMutStr = strtok_s(nullptr, DictDelim, &pMutStrNext);
				if (pMutStr == nullptr)
					break;

				auto len = strlen(pMutStr);
				std::string phoneStr(pMutStr, len);
				phones.push_back(std::move(phoneStr));
			}

			wordToPhoneList.insert(std::make_pair<std::wstring, std::vector<std::string>>(word.toStdWString(), std::move(phones)));
		}

		return std::make_tuple(true, nullptr);
	}

	std::tuple<bool, const char*> loadPronunciationVocabulary2(const std::wstring& vocabFilePathAbs, std::map<std::wstring, std::vector<Pronunc>>& wordToPhoneList, const QTextCodec& textCodec)
	{
		// file contains text in Windows-1251 encoding
		QFile file(QString::fromStdWString(vocabFilePathAbs));
		if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
			return std::make_tuple(false, "Can't open file");

		// 
		std::array<char, 1024> lineBuff;

		// each line has a format:
		// sure\tsh u e\n
		while (true) {
			auto readBytes = file.readLine(lineBuff.data(), lineBuff.size());
			if (readBytes == -1) // EOF
			{
				break;
			}
			//if (rand() % 50 >= 1) // filter for speed
			//	continue;

			// read the first word

			char* pMutStr = lineBuff.data(); // note, strtok modifies the buffer
			char* pMutStrNext = nullptr;

			const char* DictDelim = " \t\n";

			// strtok seems to be quicker than std::regex or QString::split approaches
			pMutStr = strtok_s(pMutStr, DictDelim, &pMutStrNext);
			if (pMutStr == nullptr)
			{
				// the line contains only the whitespace
				continue;
			}

			QString word = textCodec.toUnicode(pMutStr);

			// read the tail of phones
			std::vector<std::string> phones;
			while (true)
			{
				pMutStr = strtok_s(nullptr, DictDelim, &pMutStrNext);
				if (pMutStr == nullptr)
					break;

				auto len = strlen(pMutStr);
				std::string phoneStr(pMutStr, len);
				phones.push_back(std::move(phoneStr));
			}

			std::wstring wordW = word.toStdWString();
			auto it = wordToPhoneList.find(wordW);
			if (it == std::end(wordToPhoneList))
			{
				std::vector<Pronunc> prons;
				wordToPhoneList.insert({ wordW, prons });
				it = wordToPhoneList.find(wordW);
				assert(it != std::end(wordToPhoneList) && "Element must be in the map");
			}
			Pronunc pron;
			pron.setPhones(phones);
			it->second.push_back(std::move(pron));
		}

		return std::make_tuple(true, nullptr);
	}

	void normalizePronunciationVocabulary(std::map<std::wstring, std::vector<Pronunc>>& wordToPhoneList)
	{
		for (auto& pair : wordToPhoneList)
		{
			std::vector<Pronunc>& prons = pair.second;
			for (auto& pron : prons)
			{
				for (auto& phone : pron.Phones)
				{
					QString phoneStr = QString::fromStdString(phone);
					if (phoneStr[phoneStr.size()-1].isDigit())
					{
						phoneStr = phoneStr.left(phoneStr.size() - 1); // remove last digit
					}
					phoneStr = phoneStr.toUpper();
					phone = phoneStr.toStdString();
				}
			}

			std::sort(std::begin(prons), std::end(prons));
			auto it = std::unique(std::begin(prons), std::end(prons));
			size_t newSize = std::distance(std::begin(prons), it);
			prons.resize(newSize);
		}
	}

	void parsePhoneListStrs(const std::string& phonesStr, std::vector<std::string>& result)
	{
		QStringList list = QString::fromStdString(phonesStr).split(' ');
		for (int i = 0; i < list.size(); ++i)
		{
			QString phoneQ = list[i];
			if (phoneQ.isEmpty()) // a phone can't be an empty string
				continue;
			result.push_back(phoneQ.toStdString());
		}
	}

	std::tuple<bool, const char*>  parsePronuncLines(const std::wstring& prons, std::vector<PronunciationFlavour>& result)
	{
		QString pronsQ = QString::fromStdWString(prons);
		QStringList pronItems = pronsQ.split('\n', QString::SkipEmptyParts);
		for (int pronInd = 0; pronInd < pronItems.size(); ++pronInd)
		{
			QString pronLine = pronItems[pronInd];
			int pronAsWordEndInd = pronLine.indexOf('\t');
			if (pronAsWordEndInd == -1)
				return std::make_tuple(false, "First part of line doesn't contain pronunciation id");

			QString pronAsWord = pronLine.left(pronAsWordEndInd);
			QString phonesStr = pronLine.mid(pronAsWordEndInd+1);

			std::vector<std::string> phones;
			parsePhoneListStrs(phonesStr.toStdString(), phones);

			PronunciationFlavour pron;
			pron.PronAsWord = pronAsWord.toStdWString();
			pron.PhoneStrs = phones;
			result.push_back(pron);
		}
		return std::make_tuple(true, nullptr);
	}

	bool phoneToStr(UkrainianPhoneId phone, std::string& result)
	{
		switch (phone)
		{
		case UkrainianPhoneId::P_A:
			result = "A";
			break;
		case UkrainianPhoneId::P_B:
			result = "B";
			break;
		case UkrainianPhoneId::P_CH:
			result = "CH";
			break;
		case UkrainianPhoneId::P_D:
			result = "D";
			break;
		case UkrainianPhoneId::P_DZ:
			result = "DZ";
			break;
		case UkrainianPhoneId::P_DZH:
			result = "DZH";
			break;
		case UkrainianPhoneId::P_E:
			result = "E";
			break;
		case UkrainianPhoneId::P_F:
			result = "F";
			break;
		case UkrainianPhoneId::P_G:
			result = "G";
			break;
		case UkrainianPhoneId::P_H:
			result = "H";
			break;
		case UkrainianPhoneId::P_I:
			result = "I";
			break;
		case UkrainianPhoneId::P_J:
			result = "J";
			break;
		case UkrainianPhoneId::P_K:
			result = "K";
			break;
		case UkrainianPhoneId::P_KH:
			result = "KH";
			break;
		case UkrainianPhoneId::P_L:
			result = "L";
			break;
		case UkrainianPhoneId::P_M:
			result = "M";
			break;
		case UkrainianPhoneId::P_N:
			result = "N";
			break;
		case UkrainianPhoneId::P_O:
			result = "O";
			break;
		case UkrainianPhoneId::P_P:
			result = "P";
			break;
		case UkrainianPhoneId::P_R:
			result = "R";
			break;
		case UkrainianPhoneId::P_S:
			result = "S";
			break;
		case UkrainianPhoneId::P_SH:
			result = "SH";
			break;
		case UkrainianPhoneId::P_T:
			result = "T";
			break;
		case UkrainianPhoneId::P_TS:
			result = "TS";
			break;
		case UkrainianPhoneId::P_U:
			result = "U";
			break;
		case UkrainianPhoneId::P_V:
			result = "V";
			break;
		case UkrainianPhoneId::P_Y:
			result = "Y";
			break;
		case UkrainianPhoneId::P_Z:
			result = "Z";
			break;
		case UkrainianPhoneId::P_ZH:
			result = "ZH";
			break;
		default:
			return false;
		}
		return true;
	}

	bool pronuncToStr(const std::vector<UkrainianPhoneId>& pron, Pronunc& result)
	{
		std::string str;
		
		result.Phones.clear();
		for (int i = 0; i < pron.size(); ++i)
		{
			if (!phoneToStr(pron[i], str))
				return false; // can't convert some phone to string

			result.pushBackPhone(str);
		}
		return true;
	}

	// Returns true if conversion was successfull
	auto charToPhoneSimple(wchar_t letter, UkrainianPhoneId* phone) -> bool
	{
		struct CharToPhone
		{
			wchar_t Letter;
			UkrainianPhoneId Phone;
		};
		//wchar_t ukAlpha[] = L"àáâã´äåºæçè³¿éêëìíîïðñòóôõö÷øùüþÿ";
		static std::array<CharToPhone, 35> ukAlphaArray = {
			CharToPhone{ L'à', UkrainianPhoneId::P_A },
			CharToPhone{ L'á', UkrainianPhoneId::P_B },
			CharToPhone{ L'â', UkrainianPhoneId::P_V },
			CharToPhone{ L'ã', UkrainianPhoneId::P_H },
			CharToPhone{ L'´', UkrainianPhoneId::P_G },
			CharToPhone{ Letter_D, UkrainianPhoneId::Nil },
			CharToPhone{ L'å', UkrainianPhoneId::P_E },
			CharToPhone{ Letter_JE, UkrainianPhoneId::Nil },
			CharToPhone{ Letter_ZH, UkrainianPhoneId::P_ZH },
			CharToPhone{ Letter_Z, UkrainianPhoneId::P_Z },
			CharToPhone{ L'è', UkrainianPhoneId::P_Y },
			CharToPhone{ L'³', UkrainianPhoneId::P_I },
			CharToPhone{ Letter_JI, UkrainianPhoneId::Nil },
			CharToPhone{ L'é', UkrainianPhoneId::P_J },
			CharToPhone{ L'ê', UkrainianPhoneId::P_K },
			CharToPhone{ L'ë', UkrainianPhoneId::P_L },
			CharToPhone{ L'ì', UkrainianPhoneId::P_M },
			CharToPhone{ L'í', UkrainianPhoneId::P_N },
			CharToPhone{ L'î', UkrainianPhoneId::P_O },
			CharToPhone{ L'ï', UkrainianPhoneId::P_P },
			CharToPhone{ L'ð', UkrainianPhoneId::P_R },
			CharToPhone{ L'ñ', UkrainianPhoneId::P_S },
			CharToPhone{ L'ò', UkrainianPhoneId::P_T },
			CharToPhone{ L'ó', UkrainianPhoneId::P_U },
			CharToPhone{ L'ô', UkrainianPhoneId::P_F },
			CharToPhone{ L'õ', UkrainianPhoneId::P_KH },
			CharToPhone{ L'ö', UkrainianPhoneId::P_TS },
			CharToPhone{ L'÷', UkrainianPhoneId::P_CH },
			CharToPhone{ L'ø', UkrainianPhoneId::P_SH },
			CharToPhone{ Letter_SHCH, UkrainianPhoneId::Nil },
			CharToPhone{ Letter_SoftSign, UkrainianPhoneId::Nil },
			CharToPhone{ Letter_JU, UkrainianPhoneId::Nil },
			CharToPhone{ Letter_JA, UkrainianPhoneId::Nil },
			CharToPhone{ Letter_Hyphen, UkrainianPhoneId::Nil },
			CharToPhone{ Letter_Apostrophe, UkrainianPhoneId::Nil },
		};

		static std::map<wchar_t, UkrainianPhoneId> charToPhoneMap;

		if (charToPhoneMap.empty())
		{
			for (size_t i = 0; i < ukAlphaArray.size(); ++i)
			{
				const CharToPhone& c = ukAlphaArray[i];
				//qDebug() << c.Letter << " " << (int)c.Letter;
				charToPhoneMap[c.Letter] = c.Phone;
			}
		}

		auto it = charToPhoneMap.find(letter);
		if (it != std::end(charToPhoneMap))
		{
			*phone = it->second;
			return true;
		}

		return false;
	}

	std::tuple<bool, const char*> spellWord(const std::wstring& word, std::vector<UkrainianPhoneId>& phones)
	{
		for (size_t i = 0; i < word.size(); ++i)
		{
			wchar_t letter = word[i];

			UkrainianPhoneId phone = UkrainianPhoneId::Nil;
			bool convOp = charToPhoneSimple(letter, &phone);
			if (!convOp)
				return std::make_tuple(false, "Unknown letter");

			if (phone != UkrainianPhoneId::Nil)
			{
				phones.push_back(phone);
				continue;
			}
			
			// letter is converted in more complicated way

			if (letter == Letter_D)
			{
				if (i + 1 == word.size()) // last letter?
					phones.push_back(UkrainianPhoneId::P_D);
				else
				{
					wchar_t nextLetter = word[i + 1];
					if (nextLetter == Letter_Z)
					{
						phones.push_back(UkrainianPhoneId::P_DZ);
						i += 1; // skip next letter
					}
					else if (nextLetter == Letter_ZH)
					{
						phones.push_back(UkrainianPhoneId::P_DZH);
						i += 1; // skip next letter
					}
					else
						phones.push_back(UkrainianPhoneId::P_D);
					continue;
				}
			}
			else if (letter == Letter_JI)
			{
				// Rule: letter YI always converts as J and I
				phones.push_back(UkrainianPhoneId::P_Y);
				phones.push_back(UkrainianPhoneId::P_I);
			}
			else if (letter == Letter_JE)
			{
				phones.push_back(UkrainianPhoneId::P_J);
				phones.push_back(UkrainianPhoneId::P_E);
			}
			else if (letter == Letter_SHCH)
			{
				phones.push_back(UkrainianPhoneId::P_SH);
				phones.push_back(UkrainianPhoneId::P_CH);
			}
			else if (letter == Letter_JU)
			{
				phones.push_back(UkrainianPhoneId::P_J);
				phones.push_back(UkrainianPhoneId::P_U);
			}
			else if (letter == Letter_JA)
			{
				phones.push_back(UkrainianPhoneId::P_J);
				phones.push_back(UkrainianPhoneId::P_A);
			}
			else if (
				letter == Letter_SoftSign ||
				letter == Letter_Apostrophe ||
				letter == Letter_Hyphen)
			{
				// ignore, soft sign has no pronunciation
			}
			else
			{
				assert(false && "Letter must be processed already");
			}
		}
		std::make_tuple(true, nullptr);
	}

	void loadPhoneticVocabulary(const std::wstring& vocaPath, std::map<std::wstring, std::vector<std::string>>& wordToPronList)
	{
		
	}
}