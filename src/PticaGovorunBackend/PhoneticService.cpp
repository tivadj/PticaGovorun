#include "stdafx.h"
#include "PhoneticService.h"
#include <array>
#include <QFile>
#include <QDir>
#include <QDirIterator>
#include <QDebug>
#include <QXmlStreamReader>
#include <QString>
#include "CoreUtils.h"

namespace PticaGovorun
{
	namespace
	{
		const wchar_t Letter_Hyphen = L'-';
		const wchar_t Letter_Apostrophe = L'\'';
		const wchar_t Letter_A = L'�';
		const wchar_t Letter_B = L'�';
		const wchar_t Letter_H = L'�';
		const wchar_t Letter_D = L'�';
		const wchar_t Letter_JE = L'�';
		const wchar_t Letter_ZH = L'�';
		const wchar_t Letter_Z = L'�';
		const wchar_t Letter_JI = L'�';
		const wchar_t Letter_K = L'�';
		const wchar_t Letter_N = L'�';
		const wchar_t Letter_S = L'�';
		const wchar_t Letter_T = L'�';
		const wchar_t Letter_U = L'�';
		const wchar_t Letter_TS = L'�';
		const wchar_t Letter_CH = L'�';
		const wchar_t Letter_SH = L'�';
		const wchar_t Letter_SHCH = L'�';
		const wchar_t Letter_SoftSign = L'�';
		const wchar_t Letter_JU = L'�';
		const wchar_t Letter_JA = L'�';
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

	bool parsePhoneListStrs(const std::string& phonesStr, std::vector<UkrainianPhoneId>& result)
	{
		QStringList list = QString::fromStdString(phonesStr).split(' ');
		for (int i = 0; i < list.size(); ++i)
		{
			QString phoneQ = list[i];
			if (phoneQ.isEmpty()) // a phone can't be an empty string
				continue;

			bool parseOp = false;
			UkrainianPhoneId phoneId = phoneStrToId(phoneQ.toStdString(), &parseOp);
			if (!parseOp)
				return false;
			result.push_back(phoneId);
		}
		return true;
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

	UkrainianPhoneId phoneStrToId(const std::string& phoneStr, bool* parseSuccess)
	{
		if (parseSuccess != nullptr)
			*parseSuccess = true;

		if (phoneStr == "A")
			return UkrainianPhoneId::P_A;
		else if (phoneStr == "B")
			return UkrainianPhoneId::P_B;
		else if (phoneStr == "CH")
			return UkrainianPhoneId::P_CH;
		else if (phoneStr == "D")
			return UkrainianPhoneId::P_D;
		else if (phoneStr == "DZ")
			return UkrainianPhoneId::P_DZ;
		else if (phoneStr == "DZH")
			return UkrainianPhoneId::P_DZH;
		else if (phoneStr == "E")
			return UkrainianPhoneId::P_E;
		else if (phoneStr == "F")
			return UkrainianPhoneId::P_F;
		else if (phoneStr == "G")
			return UkrainianPhoneId::P_G;
		else if (phoneStr == "H")
			return UkrainianPhoneId::P_H;
		else if (phoneStr == "I")
			return UkrainianPhoneId::P_I;
		else if (phoneStr == "J")
			return UkrainianPhoneId::P_J;
		else if (phoneStr == "K")
			return UkrainianPhoneId::P_K;
		else if (phoneStr == "KH")
			return UkrainianPhoneId::P_KH;
		else if (phoneStr == "L")
			return UkrainianPhoneId::P_L;
		else if (phoneStr == "M")
			return UkrainianPhoneId::P_M;
		else if (phoneStr == "N")
			return UkrainianPhoneId::P_N;
		else if (phoneStr == "O")
			return UkrainianPhoneId::P_O;
		else if (phoneStr == "P")
			return UkrainianPhoneId::P_P;
		else if (phoneStr == "R")
			return UkrainianPhoneId::P_R;
		else if (phoneStr == "S")
			return UkrainianPhoneId::P_S;
		else if (phoneStr == "SH")
			return UkrainianPhoneId::P_SH;
		else if (phoneStr == "T")
			return UkrainianPhoneId::P_T;
		else if (phoneStr == "TS")
			return UkrainianPhoneId::P_TS;
		else if (phoneStr == "U")
			return UkrainianPhoneId::P_U;
		else if (phoneStr == "V")
			return UkrainianPhoneId::P_V;
		else if (phoneStr == "Y")
			return UkrainianPhoneId::P_Y;
		else if (phoneStr == "Z")
			return UkrainianPhoneId::P_Z;
		else if (phoneStr == "ZH")
			return UkrainianPhoneId::P_ZH;
		
		if (parseSuccess != nullptr)
			*parseSuccess = false;
		return UkrainianPhoneId::Nil;
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
		// the nil at right part of the map means, that mapping is more complex than one-one
		//wchar_t ukAlpha[] = L"��������賿��������������������";
		static std::array<CharToPhone, 35> ukAlphaArray = {
			CharToPhone{ L'�', UkrainianPhoneId::P_A },
			CharToPhone{ Letter_B, UkrainianPhoneId::Nil },
			CharToPhone{ L'�', UkrainianPhoneId::P_V },
			CharToPhone{ Letter_H, UkrainianPhoneId::Nil },
			CharToPhone{ L'�', UkrainianPhoneId::P_G },
			CharToPhone{ Letter_D, UkrainianPhoneId::Nil },
			CharToPhone{ L'�', UkrainianPhoneId::P_E },
			CharToPhone{ Letter_JE, UkrainianPhoneId::Nil },
			CharToPhone{ Letter_ZH, UkrainianPhoneId::Nil },
			CharToPhone{ Letter_Z, UkrainianPhoneId::Nil },
			CharToPhone{ L'�', UkrainianPhoneId::P_Y },
			CharToPhone{ L'�', UkrainianPhoneId::P_I },
			CharToPhone{ Letter_JI, UkrainianPhoneId::Nil },
			CharToPhone{ L'�', UkrainianPhoneId::P_J },
			CharToPhone{ L'�', UkrainianPhoneId::P_K },
			CharToPhone{ L'�', UkrainianPhoneId::P_L },
			CharToPhone{ L'�', UkrainianPhoneId::P_M },
			CharToPhone{ Letter_N, UkrainianPhoneId::Nil },
			CharToPhone{ L'�', UkrainianPhoneId::P_O },
			CharToPhone{ L'�', UkrainianPhoneId::P_P },
			CharToPhone{ L'�', UkrainianPhoneId::P_R },
			CharToPhone{ Letter_S, UkrainianPhoneId::Nil },
			CharToPhone{ Letter_T, UkrainianPhoneId::Nil },
			CharToPhone{ L'�', UkrainianPhoneId::P_U },
			CharToPhone{ L'�', UkrainianPhoneId::P_F },
			CharToPhone{ L'�', UkrainianPhoneId::P_KH },
			CharToPhone{ Letter_TS, UkrainianPhoneId::P_TS },
			CharToPhone{ Letter_CH, UkrainianPhoneId::P_CH },
			CharToPhone{ Letter_SH, UkrainianPhoneId::P_SH },
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

			bool isFirstLetter = i == 0;
			bool isLastLetter = i + 1 == word.size();

			if (letter == Letter_B || letter == Letter_H || letter == Letter_D || letter == Letter_ZH || letter == Letter_Z)
			{
				if (letter == Letter_D)
				{
					if (isLastLetter)
					{
						phones.push_back(UkrainianPhoneId::P_D);
						continue;
					}
					else
					{
						wchar_t nextLetter = word[i + 1];
						if (nextLetter == Letter_Z)
						{
							phones.push_back(UkrainianPhoneId::P_DZ);
							i += 1; // skip next letter
							continue;
						}
						else if (nextLetter == Letter_ZH)
						{
							phones.push_back(UkrainianPhoneId::P_DZH);
							i += 1; // skip next letter
							continue;
						}
					}
				}
				if (letter == Letter_Z)
				{
					if (i + 1 < word.size()) // size(ZH)=1
					{
						// Z ZH -> ZH ZH
						// ���� [ZH ZH E R]
						if (word[i + 1] == Letter_ZH)
						{
							// skip first T
							phones.push_back(UkrainianPhoneId::P_ZH);
							phones.push_back(UkrainianPhoneId::P_ZH);
							i += 1;
							continue;
						}
					}
					if (i + 2 < word.size()) // size(D ZH)=2
					{
						// Z D ZH -> ZH DZH
						// �'�������� [Z J I ZH DZH A J U T]
						if (word[i + 1] == Letter_D && word[i + 2] == Letter_ZH)
						{
							// skip first T
							phones.push_back(UkrainianPhoneId::P_ZH);
							phones.push_back(UkrainianPhoneId::P_DZH);
							i += 2;
							continue;
						}
					}
				}

				// B->P, H->KH, D->T, ZH->SH, Z->S before unvoiced sound
				// B->P ��������� [N E O P KH I D N O]
				// H->KH ��������� [D O P O M O KH T Y]
				// D->T ������ [SH V Y T K O]
				// ZH->SH ����� [D U SH CH E]
				// Z->S ������� [B E S P E K A]
				bool beforeUnvoiced = false;
				if (!isLastLetter)
				{
					wchar_t nextLetter = word[i + 1];
					if (isUnvoicedCharUk(nextLetter))
						beforeUnvoiced = true;
					else
					{
						// check if the next letter is a soft sign and then the unvoiced consonant
						if (nextLetter == Letter_SoftSign && i + 2 < word.size())
						{
							wchar_t nextNextLetter = word[i + 2];
							if (isUnvoicedCharUk(nextNextLetter))
								beforeUnvoiced = true;
						}
					}
				}
				if (!beforeUnvoiced)
				{
					if (letter == Letter_B)
						phones.push_back(UkrainianPhoneId::P_B);
					else if (letter == Letter_H)
						phones.push_back(UkrainianPhoneId::P_H);
					else if (letter == Letter_D)
						phones.push_back(UkrainianPhoneId::P_D);
					else if (letter == Letter_ZH)
						phones.push_back(UkrainianPhoneId::P_ZH);
					else if (letter == Letter_Z)
						phones.push_back(UkrainianPhoneId::P_Z);
					else PG_Assert(false);
				}
				else
				{
					if (letter == Letter_B)
						phones.push_back(UkrainianPhoneId::P_P);
					else if (letter == Letter_H)
						phones.push_back(UkrainianPhoneId::P_KH);
					else if (letter == Letter_D)
						phones.push_back(UkrainianPhoneId::P_T);
					else if (letter == Letter_ZH)
						phones.push_back(UkrainianPhoneId::P_SH);
					else if (letter == Letter_Z)
						phones.push_back(UkrainianPhoneId::P_S);
					else PG_Assert(false);
				}
				continue;
			}
			else if (letter == Letter_JI)
			{
				// Rule: letter JI always converts as J and I
				phones.push_back(UkrainianPhoneId::P_J);
				phones.push_back(UkrainianPhoneId::P_I);
			}
			else if (letter == Letter_SHCH)
			{
				phones.push_back(UkrainianPhoneId::P_SH);
				phones.push_back(UkrainianPhoneId::P_CH);
			}
			else if (letter == Letter_JE || letter == Letter_JU || letter == Letter_JA)
			{
				bool doublePhone = false;
				if (isFirstLetter)
					doublePhone = true;
				else
				{
					wchar_t prevLetter = word[i - 1];
					bool prevVowel = isUkrainianVowel(prevLetter);
					if (prevVowel)
						doublePhone = true;
				}

				if (doublePhone)
				{
					// First letter:
					// ����� [J E V R E J]
					// ���� [J U N A K]
					// ������ [J A B L U K O]
					// Previous letter is the vowel:
					// ������ [V Z A J E M N O]
					// ������ [N A S T O J U]
					// ����� [A B Y J A K]
					phones.push_back(UkrainianPhoneId::P_J);
				}
				
				if (letter == Letter_JE)
				{
					// ������ [S U T T E V O]
					phones.push_back(UkrainianPhoneId::P_E);
				}
				else if (letter == Letter_JU)
				{
					// ������� [O L E K S U K]
					// ���� [B U R U]
					phones.push_back(UkrainianPhoneId::P_U);
				}
				else if (letter == Letter_JA)
				{
					// ���� [B U R A]
					// ������� [Z O R A N Y J]
					phones.push_back(UkrainianPhoneId::P_A);
				}
				else
					PG_Assert(false, "Current letter=JE or JU or JA");
			}
			else if (letter == Letter_N)
			{
				if (i + 3 < word.size()) // size(T S T)=3
				{
					// N T S T -> N S T
					if (word[i + 1] == Letter_T && word[i + 2] == Letter_S && word[i + 3] == Letter_T)
					{
						// skip first T
						phones.push_back(UkrainianPhoneId::P_N);
						phones.push_back(UkrainianPhoneId::P_S);
						phones.push_back(UkrainianPhoneId::P_T);
						i += 3;
						continue;
					}
				}
				if (i + 4 < word.size()) // size(T S 1 K)=4
				{
					// N T S 1 K -> N S1 K
					if (word[i + 1] == Letter_T && word[i + 2] == Letter_S && word[i + 3] == Letter_SoftSign && word[i + 4] == Letter_K)
					{
						// skip first T
						phones.push_back(UkrainianPhoneId::P_N);
						phones.push_back(UkrainianPhoneId::P_S);
						phones.push_back(UkrainianPhoneId::P_K);
						i += 4;
						continue;
					}
				}
				phones.push_back(UkrainianPhoneId::P_N);
			}
			else if (letter == Letter_S)
			{
				if (i + 1 < word.size()) // size(SH)=1
				{
					// S SH -> SH SH
					// ������ [D O N I SH SH Y]
					if (word[i + 1] == Letter_SH)
					{
						phones.push_back(UkrainianPhoneId::P_SH);
						phones.push_back(UkrainianPhoneId::P_SH);
						i += 1;
						continue;
					}
				}
				if (i + 2 < word.size()) // size(T D)=2
				{
					// S T D -> Z D
					// ��������� [SH I Z D E S A T]
					if (word[i + 1] == Letter_T && word[i + 2] == Letter_D)
					{
						phones.push_back(UkrainianPhoneId::P_Z);
						phones.push_back(UkrainianPhoneId::P_D);
						i += 2;
						continue;
					}
				}
				// check STS1K group before STS group, because latter is inside the former
				if (i + 4 < word.size()) // size(T S 1 K)=4
				{
					// S T S 1 K -> S1 K
					// ���������� [N A TS Y S1 K O J I]
					if (word[i + 1] == Letter_T && word[i + 2] == Letter_S && word[i + 3] == Letter_SoftSign && word[i + 4] == Letter_K)
					{
						phones.push_back(UkrainianPhoneId::P_S);
						phones.push_back(UkrainianPhoneId::P_K);
						i += 4;
						continue;
					}
				}
				if (i + 2 < word.size()) // size(T S)=2
				{
					// S T S -> S S
					// ������� [SH I S S O T]
					if (word[i + 1] == Letter_T && word[i + 2] == Letter_S)
					{
						phones.push_back(UkrainianPhoneId::P_S);
						phones.push_back(UkrainianPhoneId::P_S);
						i += 2;
						continue;
					}
				}
				if (i + 2 < word.size()) // size(T TS)=2
				{
					// S T TS -> S TS
					// �������� [V I D P U S TS I]
					if (word[i + 1] == Letter_T && word[i + 2] == Letter_TS)
					{
						phones.push_back(UkrainianPhoneId::P_S);
						phones.push_back(UkrainianPhoneId::P_TS);
						i += 2;
						continue;
					}
				}
				phones.push_back(UkrainianPhoneId::P_S);
			}
			else if (letter == Letter_T)
			{
				if (i + 1 < word.size()) // size(S)=1
				{
					// T S -> TS
					// �'����� [P J A TS O T]
					if (word[i + 1] == Letter_S)
					{
						phones.push_back(UkrainianPhoneId::P_TS);
						i += 1;
						continue;
					}
				}
				if (i + 2 < word.size()) // size(1 S)=2
				{
					// T 1 S -> TS
					// ������������� [T R Y M A T Y M E TS A]
					if (word[i + 1] == Letter_SoftSign && word[i + 2] == Letter_S)
					{
						phones.push_back(UkrainianPhoneId::P_TS);
						phones.push_back(UkrainianPhoneId::P_TS); // �� -> TS TS
						i += 2;
						continue;
					}
				}
				if (i + 1 < word.size()) // size(TS)=1
				{
					// T TS -> TS TS
					// ����� [K L I TS TS I]
					if (word[i + 1] == Letter_TS)
					{
						phones.push_back(UkrainianPhoneId::P_TS);
						phones.push_back(UkrainianPhoneId::P_TS);
						i += 1;
						continue;
					}
				}
				if (i + 1 < word.size()) // size(CH)=1
				{
					// T CH -> CH CH
					// ���� [O CH CH E]
					if (word[i + 1] == Letter_CH)
					{
						phones.push_back(UkrainianPhoneId::P_CH);
						phones.push_back(UkrainianPhoneId::P_CH);
						i += 1;
						continue;
					}
				}
				phones.push_back(UkrainianPhoneId::P_T);
			}
			else if (letter == Letter_SoftSign)
			{
				if (!isLastLetter)
				{
					wchar_t nextLetter = word[i + 1];
					if (nextLetter == Letter_JA)
					{
						// ������� [K O N J A K]
						phones.push_back(UkrainianPhoneId::P_J);
						phones.push_back(UkrainianPhoneId::P_A);
						i += 1; // skip next letter
					}
					else if (nextLetter == Letter_JE)
					{
						// ����� [M O S J E]
						phones.push_back(UkrainianPhoneId::P_J);
						phones.push_back(UkrainianPhoneId::P_E);
						i += 1; // skip next letter
					}
					else if (nextLetter == Letter_JU)
					{
						// ��� [N J U]
						phones.push_back(UkrainianPhoneId::P_J);
						phones.push_back(UkrainianPhoneId::P_U);
						i += 1; // skip next letter
					}
					else
					{
						// ignore it
					}
				}
			}
			else if (letter == Letter_Apostrophe)
			{
				if (isLastLetter)
				{
					// apostrophe is the last char when the given word is a truncated part of some complete word
					// ignore it
				}
				else
				{
					wchar_t nextLetter = word[i + 1];
					if (nextLetter == Letter_JA)
					{
						// ���'�� [B U R J A N]
						phones.push_back(UkrainianPhoneId::P_J);
						phones.push_back(UkrainianPhoneId::P_A);
						i += 1; // skip next letter
					}
					else if (nextLetter == Letter_JE)
					{
						// ���'�� [K A R J E R]
						phones.push_back(UkrainianPhoneId::P_J);
						phones.push_back(UkrainianPhoneId::P_E);
						i += 1; // skip next letter
					}
					else if (nextLetter == Letter_JU)
					{
						// ����'���� [K O M P J U T E R]
						phones.push_back(UkrainianPhoneId::P_J);
						phones.push_back(UkrainianPhoneId::P_U);
						i += 1; // skip next letter
					}
					else
					{
						// ignore it
					}
				}
			}
			else if (letter == Letter_Hyphen)
			{
				// ignore, soft sign has no pronunciation
			}
			else
			{
				PG_Assert(false && "Letter must be processed already");
			}
		}
		return std::make_tuple(true, nullptr);
	}

	//

	template <typename T>
	bool endsWith(wv::slice<T> items, wv::slice<T> suffix)
	{
		if (suffix.size() > items.size())
			return false; // suffix will not fit the items?

		size_t itemsStartInd = items.size() - suffix.size();
		for (int i = 0; i < suffix.size(); ++i)
		{
			if (suffix[i] == items[itemsStartInd + i])
				continue;
			return false;
		}
		return true;
	}
	template <typename T>
	bool startsWith(wv::slice<T> items, wv::slice<T> prefix)
	{
		if (prefix.size() > items.size())
			return false; // prefix will not fit the items?

		for (int i = 0; i < prefix.size(); ++i)
		{
			if (prefix[i] == items[i])
				continue;
			return false;
		}
		return true;
	}

	// Finds the length of the common prefix of two words.
	template <typename T>
	size_t commonPrefixSize(wv::slice<T> word, wv::slice<T> other)
	{
		size_t minSize = std::min(word.size(), other.size());

		for (size_t i = 0; i < minSize; ++i)
		{
			if (word[i] != other[i])
				return i;
		}
		return minSize;
	}

	struct SuffixEnd
	{
		std::wstring MatchSuffix;
		int TakeCharsCount;
		WordClass WordClass = WordClass::Verb;
		int UsedCount = 0;

		SuffixEnd(const std::wstring& matchSuffix, int takeCharsCount)
			: MatchSuffix(matchSuffix),
			TakeCharsCount(takeCharsCount) {}
		SuffixEnd(const std::wstring& matchSuffix, int takeCharsCount, PticaGovorun::WordClass wordClass)
			: MatchSuffix(matchSuffix),
			TakeCharsCount(takeCharsCount),
			WordClass(wordClass) {}
	};

	std::vector<SuffixEnd> sureSuffixes;
	std::map<std::wstring, std::wstring> participleSuffixToWord;
	std::map<std::wstring, std::wstring> participleSuffixToWord2;

	void ensureSureSuffixesInitialized()
	{
		// good: �������� �������
		// verb+t1
		// ���
		static std::vector<SuffixEnd> sureSuffixesStatic = {
			{ std::wstring(    L"�"), 1, WordClass::Noun }, // ����~� noun
			{ std::wstring(    L"�"), 1, WordClass::Numeral }, // �������������~� ��~�
			{ std::wstring(    L"�"), 1, WordClass::Adjective }, // �����~� adj
			{ std::wstring(  L"���"), 2 }, // �����~��
			{ std::wstring(L"�����"), 4 }, // ����~���� ��������~����
			{ std::wstring(L"�����"), 4 }, // ������~����
			{ std::wstring(  L"���"), 2 }, // ��~�� ��~��
			{ std::wstring(  L"���"), 2 }, // ������~��
			//{ std::wstring(  L"���"), 2 }, // ����������~��
			{ std::wstring( L"���"), 3 }, // ����������~���
			{ std::wstring(  L"���"), 2 }, // ����~��
			{ std::wstring(  L"���"), 2 }, // ������~��
			{ std::wstring(  L"���"), 2, WordClass::Numeral}, // ���'���~��
			{ std::wstring(  L"���"), 3, WordClass::Numeral}, // ��~��� ���'���~���
			{ std::wstring(  L"���"), 3, WordClass::Adjective }, // �������~���

			{ std::wstring(  L"��"), 1 }, // ����~� ������~� ��~� ������~�
			{ std::wstring(L"����"), 3 }, // ��������~��� ����~���
			{ std::wstring(L"����"), 3 }, // ������~���
			//{ std::wstring(  L"��"), 1 }, // ?? NOT �����~
			{ std::wstring(  L"��"), 1 }, // ��������~� ��~�
			{ std::wstring(  L"��"), 1 }, // ������~�
			{ std::wstring(  L"��"), 2, WordClass::Noun }, // ������~�� noun
			{ std::wstring(  L"��"), 2, WordClass::Noun }, // �����~�� ��������~�� noun
			//{ std::wstring(  L"��"), 1 }, // ��������~� ����������~�
			{ std::wstring( L"���"), 2 }, // ����������~��
			{ std::wstring( L"��"), 2 }, // ����������~�� verb
			{ std::wstring(  L"��"), 1 }, // ����~� (weird word, =�������)
			{ std::wstring(  L"��"), 2, WordClass::Noun }, // ����~��
			{ std::wstring(  L"��"), 1 }, // ������~�

			{ std::wstring(        L"�"), 1 }, // ���~� �����~�
			{ std::wstring(        L"�"), 1, WordClass::Adjective }, // �����~� ������~�
			{ std::wstring(        L"�"), 1, WordClass::Noun }, // ������~�
			{ std::wstring(        L"�"), 1, WordClass::Numeral }, // �������������~�
			{ std::wstring(      L"���"), 2 }, // ������~��
			{ std::wstring(    L"�����"), 4 }, // ������~����
			{ std::wstring(    L"�����"), 4 }, // ��������~����
			{ std::wstring(  L"�������"), 6 }, // ����~��~���� ��������~������
			{ std::wstring(  L"�������"), 6 }, // ������~��~����
			{ std::wstring(    L"�����"), 4 }, // ����~���� ��~����
			{ std::wstring(    L"�����"), 4 }, // ������~����
			//{ std::wstring(    L"�����"), 4 }, // ���~����
			{ std::wstring(   L"�����"), 5 }, // ���~�����
			{ std::wstring(    L"�����"), 4 }, // ����~����
			{ std::wstring(    L"�����"), 4 }, // ������~����
			{ std::wstring(     L"����"), 3 }, // ������~��� ������~���
			{ std::wstring(     L"����"), 3 }, // ���~���
			{ std::wstring(     L"����"), 3 }, // ����������~���
			{ std::wstring(     L"����"), 3 }, // �~��� ����~���
			{ std::wstring(     L"����"), 3 }, // ������~���
			//{ std::wstring(       L"��"), 2 }, // �������~��
			{ std::wstring(      L"���"), 2 }, // ����~�� ������~��
			//{ std::wstring(     L"����"), 4 }, // ���~����
			{ std::wstring(    L"�����"), 4 }, // ���~����
			{ std::wstring(  L"�������"), 6 }, // ����~��~������ ��~������ ���-������
			{ std::wstring(  L"�������"), 6 }, // ��������~������ ��~������
			{ std::wstring(  L"�������"), 6 }, // ������~������
			//{ std::wstring(  L"�������"), 6 }, // ���~������
			{ std::wstring(  L"�������"), 7 }, // ���~�������
			{ std::wstring(  L"�������"), 6 }, // ����~������
			{ std::wstring(  L"�������"), 6 }, // ������~������
			{ std::wstring(L"���������"), 8 }, // ��������~��������
			{ std::wstring(L"���������"), 8 }, // ������~��������
			{ std::wstring(    L"�����"), 4 }, // ���~����
			{ std::wstring(     L"���"), 3 }, // ���~���
			{ std::wstring(     L"����"), 3 }, // ���~���
			{ std::wstring(     L"���"), 3 }, // ����~���
			{ std::wstring(     L"����"), 3 }, // ������~���
			{ std::wstring(     L"'���"), 3 }, // �'~���
			{ std::wstring(      L"���"), 2 }, // �������~��
			{ std::wstring(      L"���"), 2 }, // ���~�� �����~��
			//{ std::wstring(      L"���"), 2 }, // ����������~��
			{ std::wstring(     L"���"), 3 }, // ����������~���
			{ std::wstring(     L"����"), 3 }, // ��~���
			{ std::wstring(      L"���"), 2 }, // �������~��
			{ std::wstring(      L"���"), 3, WordClass::Adjective }, // �������~���
			{ std::wstring(      L"���"), 3, WordClass::Adverb }, // ������~���
			{ std::wstring( L"�"), 1, WordClass::Adjective }, // ����������~�
			{ std::wstring(L"�"), 1 }, // ������~�
			{ std::wstring(L"�"), 2, WordClass::Adjective }, // �����������~� ���������~� ������~� adj
			{ std::wstring(L"��"), 1 }, // ���~�
			{ std::wstring(L"�"), 1 }, // ����~�
			{ std::wstring(L"��"), 1 }, // ������~�
			{ std::wstring(L"'�"), 1, WordClass::Noun }, // ��'~�

			{ std::wstring(     L"�"), 1, WordClass::Noun }, // ����~� �����~� �����~� ������~� noun
			{ std::wstring(     L"�"), 1, WordClass::Numeral }, // ���������~�
			{ std::wstring(   L"���"), 2 }, // ����~��
			{ std::wstring( L"�����"), 4 }, // ��������~���� ����~����
			{ std::wstring( L"�����"), 4 }, // ������~���� (soft U, not J-U)
			{ std::wstring(   L"���"), 2 }, // ��~��
			{ std::wstring(   L"���"), 2 }, // ������~��
			//{ std::wstring(   L"���"), 2 }, // ����������~��
			{ std::wstring(  L"���"), 3 }, // ����������~���
			{ std::wstring(   L"���"), 2 }, // ����~��
			{ std::wstring(   L"���"), 2 }, // ������~��
			{ std::wstring(   L"���"), 3, WordClass::Numeral }, // ���������~���
			{ std::wstring(   L"���"), 2 }, // ������~��
			{ std::wstring(   L"���"), 3, WordClass::Numeral }, // �������������~���
			//{ std::wstring( L"�����"), 5 }, // adjective
			{ std::wstring(   L"���"), 3, WordClass::Noun }, // ������~��� ���~��� ����~��� ������~���
			{ std::wstring(   L"���"), 3, WordClass::Noun }, // �����~��� �������������~���
			//{ std::wstring(  L"����"), 2 }, // �~�� noun
			{ std::wstring(  L"'���"), 3, WordClass::Noun }, // ��'~���
			{ std::wstring(   L"���"), 2 }, // ������~�� ���~��
			{ std::wstring( L"�����"), 4 }, // ������~���� (soft U)
			{ std::wstring(   L"���"), 2 }, // ��~�� ������~�� ��~��
			{ std::wstring(   L"���"), 2 }, // ������~��
			//{ std::wstring(   L"���"), 2 }, // ����������~��
			{ std::wstring(  L"���"), 3 }, // ����������~���
			{ std::wstring(   L"���"), 2 }, // ����~��
			{ std::wstring(   L"���"), 2 }, // ������~��
			{ std::wstring( L"�����"), 4 }, // ��������~����
			{ std::wstring(   L"���"), 2, WordClass::VerbalAdverb }, // �������~�� �������~��
			{ std::wstring(   L"���"), 2, WordClass::VerbalAdverb }, // ����~�� ������~��
			{ std::wstring(   L"���"), 3, WordClass::VerbalAdverb }, // ���~��� ?
			//{ std::wstring(  L"����"), 3, WordClass::VerbalAdverb }, // �������~��� ����~���
			//{ std::wstring(  L"����"), 3, WordClass::VerbalAdverb }, // ���~��� ������~���
			//{ std::wstring(  L"����"), 3, WordClass::VerbalAdverb }, // �~��� ����~���
			//{ std::wstring(  L"����"), 3, WordClass::VerbalAdverb }, // ������~���
			//{ std::wstring(  L"'���"), 3, WordClass::VerbalAdverb }, // �'~���
			{ std::wstring(   L"���"), 3, WordClass::VerbalAdverb }, // ������~��� �����~��� ���~���
			{ std::wstring(  L"����"), 3, WordClass::VerbalAdverb }, // ���~���
			{ std::wstring(  L"����"), 3, WordClass::VerbalAdverb }, // ��~��� ���~��� ���~���
			{ std::wstring(  L"����"), 3, WordClass::VerbalAdverb }, // ������~��� �����~���
			//{ std::wstring(  L"����"), 3, WordClass::VerbalAdverb }, // ����������~���
			{ std::wstring( L"����"), 4, WordClass::VerbalAdverb }, // ����������~����
			{ std::wstring(  L"����"), 3, WordClass::VerbalAdverb }, // ������~��� ��~���
			{ std::wstring(  L"����"), 3, WordClass::VerbalAdverb }, // ��������~��� ��~���
			{ std::wstring(  L"����"), 3, WordClass::VerbalAdverb }, // ����~���
			{ std::wstring(L"������"), 5, WordClass::VerbalAdverb }, // �������~����� ������~�����
			{ std::wstring(L"������"), 5, WordClass::VerbalAdverb }, // ����~����� ��������~����� ��~�����

			{ std::wstring(   L"�"), 1, WordClass::Noun }, // ���~� �����~�
			{ std::wstring(   L"�"), 1, WordClass::Adjective }, // �����~� ������~�
			{ std::wstring(   L"�"), 1, WordClass::Numeral }, // �������������~�
			{ std::wstring(L"��"), 3, WordClass::Noun }, // ��������~�� noun
			{ std::wstring( L"��"), 3, WordClass::Noun }, // ����~��
			{ std::wstring( L"��"), 3, WordClass::Noun }, // �����~��
			{ std::wstring( L"��"), 3, WordClass::Noun }, // ������~�� noun
			{ std::wstring( L"���"), 3, WordClass::Adjective }, // �������~��� adj

			{ std::wstring( L"�"), 1, WordClass::Adjective }, // �����~�
			{ std::wstring( L"�"), 1, WordClass::Noun }, // ��������~�
			{ std::wstring(L"�"), 1, WordClass::Noun }, // ����~�
			{ std::wstring(L"��"), 2, WordClass::Pronoun }, // ��~��
			{ std::wstring(L"�"), 1, WordClass::Noun }, // ������~�
			{ std::wstring(L"��"), 2, WordClass::Adjective }, // ���������~�� �����������~�� adj
			{ std::wstring(L"��"), 1, WordClass::Noun }, // �����i���i~� �����������~� ��~�
			{ std::wstring(L"�"), 2, WordClass::Adjective }, // ������~�
			{ std::wstring(L"�"), 1, WordClass::Noun }, // ��������~�
			{ std::wstring(L"�"), 1, WordClass::Numeral }, // �������������~�
			{ std::wstring(L"�"), 1, WordClass::Noun }, // �����~�
			{ std::wstring(L"��"), 1 }, // ���~�
			{ std::wstring(L"��"), 1, WordClass::Noun }, // ����~�

			{ std::wstring(      L"��"), 1 }, // ������~� ������~�
			{ std::wstring(      L"��"), 2, WordClass::Noun }, // ��~��
			{ std::wstring(      L"��"), 2, WordClass::Adjective }, // ����~�� ���������~�� adj 
			{ std::wstring(      L"��"), 2, WordClass::Numeral }, // �������������~��
			{ std::wstring(      L"��"), 2, WordClass::Adjective }, // ��������~�� ������~�� ����������~�� adj
			{ std::wstring(      L"��"), 1, WordClass::Noun }, // �������~� ���������~� noun
			{ std::wstring(      L"��"), 1, WordClass::Numeral }, // �������������~��
			{ std::wstring(      L"��"), 2, WordClass::Pronoun }, // ���~��
			{ std::wstring(      L"��"), 2, WordClass::Adjective }, // ������~��
			{ std::wstring(      L"��"), 1 }, // ����������~�
			{ std::wstring(      L"��"), 1 }, // ����~�
			{ std::wstring(      L"��"), 1 }, // ������~�
			{ std::wstring(    L"����"), 3, WordClass::Participle }, // ���~��� ������~��� ��
			{ std::wstring(    L"����"), 3, WordClass::Participle }, // �~��� ���~��� ��
			{ std::wstring(  L"������"), 5, WordClass::Participle }, // ��������~����� ������~����� �������~�����
			{ std::wstring(  L"������"), 5, WordClass::Participle }, // ��������~�����
			{ std::wstring(  L"������"), 5, WordClass::Participle }, // ������~�����
			{ std::wstring(    L"����"), 3, WordClass::Participle }, // �������~��� ������~��� �������~��� ��
			{ std::wstring(    L"����"), 4, WordClass::Adjective }, // ����������~���� ���~���� �������~���� ��
			{ std::wstring(   L"����"), 4, WordClass::Adjective }, // ����������~���� ���~���� �������~����
			{ std::wstring(    L"����"), 3, WordClass::Participle }, // ��~���
			{ std::wstring(    L"����"), 3, WordClass::Participle }, // ����~���
			{ std::wstring(    L"����"), 3, WordClass::Participle }, // ������~���
			{ std::wstring(    L"����"), 3, WordClass::Participle }, // �������~��� �������~���
			{ std::wstring(    L"����"), 3, WordClass::Participle}, // ����~��� ������~��� ������~��� ����~���
			{ std::wstring(    L"����"), 4, WordClass::Participle }, // compound ���~����
			//{ std::wstring(   L"�����"), 4, WordClass::Participle }, // ������~����
			//{ std::wstring(   L"�����"), 4, WordClass::Participle }, // ���~���� ������~����
			//{ std::wstring(   L"�����"), 4 , WordClass::Participle}, // ����~����
			//{ std::wstring(   L"�����"), 4, WordClass::Participle }, // ������~����
			//{ std::wstring(   L"'����"), 4, WordClass::Participle }, // �'~����
			{ std::wstring(    L"����"), 4, WordClass::Participle }, // ������~���� ���~����
			{ std::wstring(   L"�����"), 4, WordClass::Participle }, // ���~����
			{ std::wstring(     L"���"), 3, WordClass::Adjective }, // �����~���
			{ std::wstring(   L"�����"), 4, WordClass::Participle }, // ���~����
			{ std::wstring( L"�������"), 6, WordClass::Participle }, // ����~������ ��������~������ ��~������
			{ std::wstring( L"�������"), 6, WordClass::Participle }, // ������~������ ������~������
			{ std::wstring(   L"�����"), 4, WordClass::Participle }, // ��������~���� ��~����
			{ std::wstring(   L"�����"), 4, WordClass::Participle }, // ������~���� �����~����
			//{ std::wstring(   L"�����"), 4 }, // ����������~����
			{ std::wstring(  L"�����"), 5, WordClass::Participle }, // ����������~�����
			{ std::wstring(   L"�����"), 4, WordClass::Participle }, // ����~����
			{ std::wstring(   L"�����"), 4, WordClass::Participle }, // ������~���� ��~����
			{ std::wstring(    L"����"), 4, WordClass::Adjective }, // �������~����

			{ std::wstring(      L"��"), 2, WordClass::Noun }, // ������~��
			{ std::wstring(      L"��"), 2, WordClass::Numeral }, // ����������~��
			{ std::wstring(      L"��"), 1 }, // ���~�� �����~��
			{ std::wstring(      L"��"), 2, WordClass::Noun }, // �������~�� ERROR: �������~ �������~(��������)
			{ std::wstring(    L"����"), 3 }, // ������~���
			{ std::wstring(  L"������"), 5 }, // ���~�����
			{ std::wstring(  L"������"), 5 }, // ����~�����
			{ std::wstring(L"��������"), 7 }, // ����~��~����� ��������~�������
			{ std::wstring(L"��������"), 7 }, // ������~��~�����
			{ std::wstring(  L"������"), 5 }, // ����~����� ��~�����
			{ std::wstring(  L"������"), 5 }, // ����~����� ?
			//{ std::wstring( L"������"), 5 }, // ���~�����
			{ std::wstring( L"������"), 6 }, // ���~������
			{ std::wstring(  L"������"), 5 }, // ������~�����
			{ std::wstring(  L"������"), 5 }, // �������~������
			{ std::wstring(      L"��"), 2, WordClass::Noun }, // composite
			{ std::wstring(      L"��"), 2 }, // composite
			//{ std::wstring(     L"��"), 2 }, // ���~��
			//{ std::wstring(     L"��"), 2 }, // �������~�� noun
			//{ std::wstring(     L"���"), 2 }, // ������~�� noun
			//{ std::wstring(     L"��"), 2 }, // ��������~��
			//{ std::wstring(     L"��"), 2 }, // ��~��
			//{ std::wstring(     L"���"), 2 }, // ������~��
			//{ std::wstring(     L"'��"), 2 }, // �'~��
			{ std::wstring(      L"��"), 1 }, // �������~�
			{ std::wstring(      L"��"), 2, WordClass::Numeral }, // �������������~��
			{ std::wstring(      L"��"), 1 }, // �����~� ���~� �����~�
			{ std::wstring(      L"��"), 2, WordClass::Numeral }, // �������������~��
			//{ std::wstring(      L"��"), 1 }, // ����������~�
			{ std::wstring(     L"��"), 2 }, // ����������~��
			{ std::wstring(      L"��"), 2, WordClass::Noun }, // ������~��
			{ std::wstring(      L"��"), 2, WordClass::Numeral }, // ��~�� ����������~��
			{ std::wstring(      L"��"), 2, WordClass::Noun }, // compound ���~�� �������������~��
			//{ std::wstring(     L"���"), 2, WordClass::Noun }, // ��������~��
			//{ std::wstring(     L"'��"), 2, WordClass::Noun }, // ��'~��

			{ std::wstring(        L"�"), 1 }, // ����~�
			{ std::wstring(        L"�"), 1, WordClass::Adverb }, // ������~� adverb
			{ std::wstring(        L"�"), 1, WordClass::Numeral }, // ��~�
			{ std::wstring(      L"���"), 3, WordClass::Numeral }, // �������������~���
			{ std::wstring(     L"����"), 3, WordClass::Pronoun }, // ����~���
			{ std::wstring(      L"���"), 2 }, // �������~��
			{ std::wstring(    L"�����"), 4 }, // ����~���� ��������~����
			{ std::wstring(    L"�����"), 4 }, // ������~���� (soft U)
			{ std::wstring(      L"���"), 2 }, // ��~�� ��~��
			{ std::wstring(      L"���"), 2 }, // ������~��
			//{ std::wstring(      L"���"), 2 }, // ����������~��
			{ std::wstring(     L"���"), 3 }, // ����������~���
			{ std::wstring(      L"���"), 2 }, // ����~��
			{ std::wstring(      L"���"), 2 }, // ������~��
			//{ std::wstring(       L"��"), 2 }, // �������~�� ��������~��
			{ std::wstring(      L"���"), 2 }, // ����~�� ������~��
			{ std::wstring(      L"���"), 2 }, // �������~��
			{ std::wstring(  L"�������"), 6 }, // ����~��~������ ��~������
			{ std::wstring(  L"�������"), 6 }, // ��~������ ��~������
			{ std::wstring(  L"�������"), 6 }, // ������~������
			//{ std::wstring(  L"�������"), 6 }, // ���~������
			{ std::wstring( L"�������"), 7 }, // ���~�������
			{ std::wstring(  L"�������"), 6 }, // ����~������
			{ std::wstring(  L"�������"), 6 }, // ������~������
			{ std::wstring(L"���������"), 8 }, // ��������~��������
			{ std::wstring(L"���������"), 8 }, // ������~��������
			{ std::wstring(     L"���"), 3 }, // ���~���
			{ std::wstring(     L"����"), 3 }, // ���~���
			{ std::wstring(     L"���"), 3 }, // ����~���
			{ std::wstring(     L"����"), 3 }, // ������~���
			{ std::wstring(     L"'���"), 3 }, // �'~���
			{ std::wstring(      L"���"), 2 }, // ���~�� �����~�� �����~��
			//{ std::wstring(      L"���"), 2 }, // ����������~��
			{ std::wstring(     L"���"), 3 }, // ����������~���
			//{ std::wstring(      L"���"), 3 }, // NOT ����*���
			{ std::wstring(     L"����"), 3 }, // ������~��� ����~���
			//{ std::wstring(     L"����"), 3 }, // no words
			{ std::wstring(     L"����"), 3 }, // ���~��� ��~���
			{ std::wstring(     L"����"), 3 }, // ���~���
			{ std::wstring(     L"����"), 3 }, // ����������~���
			{ std::wstring(     L"����"), 3 }, // ����~��� ��������~���
			{ std::wstring(     L"����"), 3 }, // ������~���
			{ std::wstring(     L"����"), 3 }, // ������~���
			{ std::wstring(      L"���"), 2 }, // �������~�� ��������~��
			{ std::wstring(      L"���"), 3, WordClass::Numeral }, // ���'��~���
			
			{ std::wstring(      L"�"), 1, WordClass::Noun }, // noun, ����~� ���~� ������~� ������~� �����~�
			{ std::wstring(      L"�"), 1, WordClass::Numeral }, // �������������~�
			//{ std::wstring(     L"��"), 2 }, // not usable (���*�~�)
			//{ std::wstring(    L"���"), 2 }, // not usable (��*�~�)
			{ std::wstring(    L"���"), 2, WordClass::Pronoun }, // ���~��
			{ std::wstring(    L"���"), 2 }, // �����*��~��
			//{ std::wstring(   L"����"), 4 }, // �����*����; �*���� is too rare to use
			{ std::wstring(  L"�����"), 4 }, // ���~����
			{ std::wstring(  L"�����"), 4 }, // ��������~���� ��~����
			{ std::wstring(  L"�����"), 4 }, // ������~����
			//{ std::wstring(  L"�����"), 4 }, // ���~����
			{ std::wstring( L"�����"), 5 }, // ���~�����
			{ std::wstring(  L"�����"), 4 }, // ����~����
			{ std::wstring(  L"�����"), 4 }, // ����~����
			{ std::wstring(  L"�����"), 4 }, // ������~����
			{ std::wstring(L"�������"), 6 }, // ����~������ ��������~������
			{ std::wstring(L"�������"), 6 }, // ������~������
			{ std::wstring(    L"���"), 3, WordClass::Adjective }, // �������~��� adj
			{ std::wstring(    L"���"), 3, WordClass::Numeral }, // �������������~���
			//{ std::wstring(    L"���"), 2 }, // not usable (����*�~�)

			{ std::wstring(  L"��"), 2, WordClass::Noun }, // ����~�� ������~��
			{ std::wstring(  L"��"), 2, WordClass::Numeral }, // ����������~��
			{ std::wstring(  L"��"), 2, WordClass::Adjective }, // �������~��
			{ std::wstring(  L"��"), 2, WordClass::Numeral }, // �������������~��
			{ std::wstring(  L"��"), 2, WordClass::Numeral }, // ���'���~�� ��~��
			{ std::wstring(  L"��"), 2, WordClass::Noun }, // compound
			//{ std::wstring( L"���"), 2, WordClass::Noun }, // ��������~��
			//{ std::wstring(  L"��"), 2, WordClass::Noun }, // �������������~��
			//{ std::wstring( L"'��"), 2, WordClass::Noun }, // ��'~��

			{ std::wstring(      L"��"), 2 }, // ���~�� �����~��
			{ std::wstring(    L"����"), 3 }, // ������~���
			{ std::wstring(  L"������"), 5 }, // ���~����� ������~�����
			{ std::wstring(  L"������"), 5 }, // ��������~����� ��~�����
			{ std::wstring(  L"������"), 5 }, // ������~�����
			//{ std::wstring(  L"������"), 5 }, // ���~�����
			{ std::wstring( L"������"), 6 }, // ���~������
			{ std::wstring(  L"������"), 5 }, // ����~�����
			{ std::wstring(  L"������"), 5 }, // ������~�����
			//{ std::wstring(L"��������"), 7 }, // no words with such suffix
			{ std::wstring(L"��������"), 7 }, // ��������~������� ����~�������
			{ std::wstring(L"��������"), 7 }, // ������~������� (soft U)
			{ std::wstring(     L"��"), 2 }, // �~�� ��~��
			{ std::wstring(     L"���"), 2 }, // ���~��
			{ std::wstring(     L"��"), 2 }, // ����~��
			{ std::wstring(     L"���"), 2 }, // ������~��
			{ std::wstring(     L"'��"), 2 }, // �'~��
			{ std::wstring(      L"��"), 1 }, // ������~��
			{ std::wstring(      L"��"), 2, WordClass::Adverb }, // ������~��
			//{ std::wstring(      L"��"), 1 }, // ����������~�
			{ std::wstring(     L"��"), 2 }, // ����������~��

			{ std::wstring(      L"�����"), 4 }, // ������~����
			{ std::wstring(    L"�������"), 6 }, // ��������~������
			{ std::wstring(    L"�������"), 6 }, // �����~������
			{ std::wstring(      L"�����"), 4 }, // �����~���� ��~����
			//{ std::wstring(      L"�����"), 4 }, // ����������~����
			{ std::wstring(     L"�����"), 5 }, // ����������~�����
			{ std::wstring(      L"�����"), 4 }, // ����~����
			{ std::wstring(      L"�����"), 4 }, // �����~��������
			{ std::wstring(     L"������"), 5 }, // ��~�����
			{ std::wstring(       L"����"), 3 }, // ������~���
			{ std::wstring(     L"������"), 5 }, // ��������~�����
			{ std::wstring(     L"������"), 5 }, // �����~�����
			{ std::wstring(       L"����"), 3 }, // ��������~��� ��~���
			//{ std::wstring(       L"����"), 3 }, // ����������~���
			{ std::wstring(      L"����"), 4 }, // ����������~����
			{ std::wstring(       L"����"), 3 }, // ����~���
			{ std::wstring(       L"����"), 3 }, // �����~���
			{ std::wstring(       L"����"), 4 }, // ��~����
			// { std::wstring(        L"���"), 2 }, // no words
			{ std::wstring(   L"��������"), 8 }, // �����~��������
			{ std::wstring(  L"���������"), 8 }, // ������~��������
			//{ std::wstring(  L"���������"), 7 }, // ���~��������
			{ std::wstring( L"���������"), 9 }, // ���~���������
			{ std::wstring(  L"���������"), 8 }, // ����~��������
			{ std::wstring(L"�����������"), 10 }, // ��������~����������
			{ std::wstring(L"�����������"), 10 }, // �����~����������
			{ std::wstring(  L"���������"), 8 }, // �����~�������� ��~��������
			{ std::wstring(  L"���������"), 9 }, // ��~���������
			{ std::wstring(      L"�����"), 4 }, // ����~����
			{ std::wstring(      L"�����"), 5 }, // �'~�����
			{ std::wstring(     L"�����"), 5 }, // ��������~�����
			{ std::wstring(      L"�����"), 4 }, // ��������~����
			{ std::wstring(      L"�����"), 4 }, // ���~���� ����~���� ���~����
			//{ std::wstring(      L"�����"), 4 }, // ����������~����
			{ std::wstring(     L"�����"), 5 }, // ����������~����� ��~�����
			{ std::wstring(     L"������"), 5 }, // ������~�����
			{ std::wstring(     L"������"), 5 }, // ��~�����
			{ std::wstring(     L"������"), 5 }, // �~�����
			{ std::wstring(     L"������"), 5 }, // ����������~�����
			{ std::wstring(     L"������"), 5 }, // ��������~�����
			{ std::wstring(     L"������"), 5 }, // �����~�����
			{ std::wstring(     L"�����"), 5 }, // �~����� ������~�����
			{ std::wstring(     L"������"), 5 }, // �����~�����
			{ std::wstring(      L"�����"), 4 }, // �������~����
			{ std::wstring(        L"���"), 2, WordClass::Verb }, // ����~��
			{ std::wstring(        L"���"), 2, WordClass::VerbalAdverb }, // ������~��
			{ std::wstring(      L"�����"), 4 }, // ������~����
			{ std::wstring(    L"�������"), 6 }, // �����~������
			{ std::wstring(      L"�����"), 4 }, // �����~���� ��~����
			//{ std::wstring(      L"�����"), 4 }, // ����������~����
			{ std::wstring(     L"�����"), 5 }, // ����������~�����
			{ std::wstring(      L"�����"), 4 }, // ����~����
			{ std::wstring(      L"�����"), 4 }, // �����~����
			{ std::wstring(    L"�������"), 6 }, // ��������~������
			{ std::wstring(      L"�����"), 5 }, // ��~�����
			{ std::wstring(      L"�����"), 4 }, // ���~����
			{ std::wstring(    L"�������"), 6 }, // ��������~������
			{ std::wstring(    L"�������"), 6 }, // �����~������
			{ std::wstring(      L"�����"), 4 }, // �����~���� ��~����
			//{ std::wstring(      L"�����"), 4 }, // ����������~����
			{ std::wstring(     L"�����"), 5 }, // ����������~�����
			{ std::wstring(      L"�����"), 4 }, // ����~����
			{ std::wstring(      L"�����"), 4 }, // �����~����
			{ std::wstring(      L"�����"), 5 }, // ��~�����
			{ std::wstring(      L"�����"), 4, WordClass::VerbalAdverb }, // �����~���� �������~����
			{ std::wstring(      L"�����"), 4, WordClass::VerbalAdverb }, // ����~����
			{ std::wstring(      L"�����"), 5, WordClass::VerbalAdverb }, // compound
			//{ std::wstring(     L"������"), 5, WordClass::VerbalAdverb }, // ������~�����
			//{ std::wstring(     L"������"), 5, WordClass::VerbalAdverb }, // ����~�����
			//{ std::wstring(     L"������"), 5, WordClass::VerbalAdverb }, // �����~�����
			//{ std::wstring(      L"�����"), 5, WordClass::VerbalAdverb }, // ����~����
			//{ std::wstring(     L"'�����"), 5, WordClass::VerbalAdverb }, // �'~�����
			{ std::wstring(      L"�����"), 5, WordClass::VerbalAdverb }, // ��~����� ���~����� ����~����� ���~�����
			{ std::wstring(     L"������"), 5, WordClass::VerbalAdverb }, // ���~����� ��~�����

			{ std::wstring(     L"������"), 5, WordClass::VerbalAdverb }, // ������~����� ���~�����
			{ std::wstring(     L"������"), 5, WordClass::VerbalAdverb }, // ����~�����
			{ std::wstring(   L"��������"), 7, WordClass::VerbalAdverb }, // �����~������� ������~�������
			{ std::wstring(   L"��������"), 7, WordClass::VerbalAdverb }, // ��������~�������
			{ std::wstring(     L"������"), 5, WordClass::VerbalAdverb }, // �����~����� ��~�����
			//{ std::wstring(     L"������"), 5, WordClass::VerbalAdverb }, // ����������~�����
			{ std::wstring(    L"������"), 6, WordClass::VerbalAdverb }, // ����������~������
			{ std::wstring(     L"������"), 5, WordClass::VerbalAdverb }, // �����~����� ������~�����
			{ std::wstring(     L"������"), 5, WordClass::VerbalAdverb }, // ���~�����
			{ std::wstring(    L"�������"), 6, WordClass::VerbalAdverb }, // ��~������
			{ std::wstring(       L"����"), 3 }, // ���~��� ������~���
			{ std::wstring(       L"����"), 3 }, // �~��� ��~���
			{ std::wstring(       L"����"), 3 }, // �~���
			{ std::wstring(       L"����"), 3 }, // ����������~���
			{ std::wstring(       L"����"), 3 }, // ��������~���
			{ std::wstring(       L"����"), 3 }, // �����~���
			//{ std::wstring(        L"���"), 2 }, // no actual words (all are LOS or MOS) ����~�� ??
			{ std::wstring(      L"�����"), 5 }, // ���~���� ������~���� ���~����
			{ std::wstring(    L"�������"), 6 }, // �����~������
			{ std::wstring(      L"�����"), 4 }, // �����~���� ��~����
			//{ std::wstring(      L"�����"), 4 }, // ����������~����
			{ std::wstring(     L"�����"), 5 }, // ����������~�����
			{ std::wstring(      L"�����"), 4 }, // ����~����
			{ std::wstring(      L"�����"), 4 }, // �����-����
			{ std::wstring(    L"�������"), 6 }, // ��������~������
			{ std::wstring(      L"�����"), 5 }, // ��~�����
			// { std::wstring(       L"����"), 4 }, // 
			{ std::wstring(      L"�����"), 4 }, // ������~���� ����~����
			{ std::wstring(    L"�������"), 6 }, // �������~������ (we can't split �����*��������)
			{ std::wstring(  L"���������"), 8 }, // ���~�������� ����~��~��������
			{ std::wstring(  L"���������"), 8 }, // �����~�������� ��~��������
			//{ std::wstring(  L"���������"), 8 }, // ���~��������
			{ std::wstring(  L"���������"), 9 }, // ���~���������
			{ std::wstring(  L"���������"), 8 }, // ����~��������
			{ std::wstring(L"�����������"), 10 }, // ��������~����������
			{ std::wstring(L"�����������"), 10 }, // �����~����������
			{ std::wstring(  L"���������"), 9 }, // ��~���������
			{ std::wstring(      L"�����"), 5 }, // �'~�����
			//{ std::wstring(     L"�����"), 5 }, // ������~�����
			//{ std::wstring(     L"�����"), 5 }, // ����~�����
			//{ std::wstring(     L"������"), 5 }, // ���~�����
			//{ std::wstring(     L"�����"), 5 }, // �����~�����
			//{ std::wstring(     L"�����"), 5 }, // �������~����� ��������~�����
			//{ std::wstring(     L"������"), 5 }, // ��������~����� �����~�����
			//{ std::wstring(     L"������"), 5 }, // �����~�����
			//{ std::wstring(     L"'�����"), 5 }, // ���'~����� �'~�����
			{ std::wstring(      L"�����"), 4 }, // ��������~����
			//{ std::wstring(      L"�����"), 4 }, // ��~���� ����������~����
			{ std::wstring(      L"�����"), 4 }, // �����~���� ���~���� ���~����
			{ std::wstring(      L"�����"), 5 }, // compound
			//{ std::wstring(     L"�����"), 5 }, // ��~�����
			//{ std::wstring(     L"�����"), 5 }, // ��~����� ����������~�����
			{ std::wstring(      L"�����"), 5 }, // compound
			//{ std::wstring(     L"������"), 5 }, // ������~�����
			//{ std::wstring(     L"������"), 5 }, // ��~�����
			//{ std::wstring(     L"������"), 5 }, // �~�����
			//{ std::wstring(     L"������"), 5 }, // ����������~�����
			//{ std::wstring(     L"������"), 5 }, // ��������~�����
			//{ std::wstring(     L"������"), 5 }, // �����~�����
			{ std::wstring(      L"�����"), 4 }, // ��������~����
			{ std::wstring(        L"���"), 2 }, // ����~�� �������~��
			{ std::wstring(      L"�����"), 4 }, // �������~����
			{ std::wstring(    L"�������"), 6 }, // ������~������
			{ std::wstring(    L"�������"), 7 }, // compound
			//{ std::wstring(   L"�������"), 7 }, // ���~�������
			{ std::wstring(    L"�������"), 6 }, // ����~������
			{ std::wstring(  L"���������"), 8 }, // ��������~��������
			{ std::wstring(  L"���������"), 8 }, // �����~��������
			{ std::wstring(    L"�������"), 6 }, // �����~������ ��~������
			{ std::wstring(    L"�������"), 7 }, // ��~�������

			{ std::wstring(        L"���"), 3 }, // ����~��� �����~��� ���~���
			{ std::wstring(       L"����"), 3 }, // ������~���
			{ std::wstring(       L"����"), 3 }, // �����~���
			{ std::wstring(       L"����"), 3 }, // ��~���
			{ std::wstring(       L"����"), 3 }, // ��~���(?or zero) ����������~��� ERROR: ���~��� (need to keep letter 'o')
			{ std::wstring(       L"����"), 3 }, // ��������~���
			{ std::wstring(       L"����"), 4 }, // ��~����
			{ std::wstring(       L"����"), 3 }, // �����~��� �����~���
			{ std::wstring(       L"����"), 3 }, // ������~���
			{ std::wstring(       L"'���"), 3 }, // �'~���

			{ std::wstring(       L"���"), 2 }, // ���~�� �������~��
			{ std::wstring(     L"�����"), 4 }, // ����~����
			{ std::wstring(     L"�����"), 4 }, // ������~����
			{ std::wstring(       L"���"), 2 }, // ��������~�� ��~��
			{ std::wstring(       L"���"), 3 }, // ���~��� ������~��� �����~���
			//{ std::wstring(       L"���"), 3 }, // ����������~��
			{ std::wstring(      L"���"), 3 }, // ����������~���
			{ std::wstring(       L"���"), 3 }, // ���~��� ���~��� ���~��� ��~���
			{ std::wstring(       L"���"), 3 }, // ���~��� �����~���
			{ std::wstring(     L"�����"), 4 }, // ������~����
			{ std::wstring(   L"�������"), 6 }, // �~������ ������~������
			{ std::wstring(   L"�������"), 6 }, // ��~������ ��~������
			{ std::wstring(   L"�������"), 6 }, // ������~������
			//{ std::wstring(   L"�������"), 6 }, // ���~������
			{ std::wstring(  L"�������"), 7 }, // ���~�������
			{ std::wstring(   L"�������"), 6 }, // ����~������
			{ std::wstring(   L"�������"), 6 }, // ������~������
			{ std::wstring( L"���������"), 8 }, // ��������~��������
			{ std::wstring( L"���������"), 8 }, // ������~��������
			{ std::wstring(       L"���"), 3 }, // ���~���
			{ std::wstring(      L"����"), 3 }, // �����~��� ������~���
			{ std::wstring(      L"����"), 3 }, // ���~���
			{ std::wstring(      L"����"), 3 }, // ����~���
			{ std::wstring(      L"����"), 3 }, // ������~���
			{ std::wstring(      L"'���"), 3 }, // ������~��� �'~���
			{ std::wstring(       L"���"), 3 }, // ������~��� ���~���
			{ std::wstring(      L"����"), 3 }, // ����������~���

			{ std::wstring( L"�"), 1 }, // �����~�
			{ std::wstring( L"�"), 1 , WordClass::Noun}, // ���������~�
			{ std::wstring(L"��"), 1 }, // ������~�
			{ std::wstring(L"��"), 2, WordClass::Noun }, // ��'~��
			{ std::wstring(L"��"), 1 }, // ���~�
			{ std::wstring(L"��"), 1 }, // ���~�
			{ std::wstring(L"��"), 2, WordClass::Noun }, // ���������~�� ���~��
			{ std::wstring(L"��"), 2, WordClass::Numeral }, // �������������~��
			{ std::wstring(L"��"), 1 }, // ����~� ��������~�
			{ std::wstring(L"��"), 1 }, // �����~�
			{ std::wstring(L"'�"), 1, WordClass::Noun }, // ��'~�

			{ std::wstring(          L"�"), 1, WordClass::Adjective }, // ������~�
			{ std::wstring(          L"�"), 1, WordClass::Noun }, // �������������~�
			{ std::wstring(         L"��"), 2, WordClass::Adjective }, // �����������~�� ������~�� adj
			{ std::wstring(         L"��"), 2, WordClass::Adjective }, // ����������~��
			{ std::wstring(         L"'�"), 1, WordClass::Noun }, // ��'~�
			// { std::wstring(         L"��"), 2 }, // can't detach unvoiced 'Sya'
			//{ std::wstring(        L"���"), 2 }, // no words
			{ std::wstring(       L"����"), 4 }, // �����~����
			{ std::wstring(      L"�����"), 4 }, // ������~����
			{ std::wstring(      L"�����"), 4 }, // ����~����
			{ std::wstring(    L"�������"), 6 }, // ��������~������
			{ std::wstring(    L"�������"), 6 }, // �����~������
			{ std::wstring(      L"�����"), 4 }, // �����~���� ��~����
			//{ std::wstring(      L"�����"), 4 }, // ����������~����
			{ std::wstring(     L"�����"), 5 }, // ����������~�����
			{ std::wstring(      L"�����"), 4 }, // �����~����
			{ std::wstring(      L"�����"), 5 }, // ��~�����
			{ std::wstring(       L"����"), 3 }, // ������~��� ���~���
			{ std::wstring(     L"������"), 5 }, // ��������~�����
			{ std::wstring(     L"������"), 5 }, // �����~�����
			{ std::wstring(       L"����"), 3 }, // �����~��� ��~���
			//{ std::wstring(       L"����"), 3 }, // ����������~���
			{ std::wstring(      L"����"), 4 }, // ����������~����
			{ std::wstring(       L"����"), 3 }, // ����~���
			{ std::wstring(       L"����"), 3 }, // �����~���
			{ std::wstring(       L"����"), 3 }, // ���~���
			//{ std::wstring(       L"����"), 3 }, // �����~� (��� is too common)
			{ std::wstring(      L"�����"), 4 }, // ����~���� ������~����
			{ std::wstring(    L"�������"), 6 }, // �������~������
			{ std::wstring(  L"���������"), 8 }, // ������~��������
			{ std::wstring(L"�����������"), 10 }, // ��������~����������
			{ std::wstring(L"�����������"), 10 }, // �����~����������
			{ std::wstring(  L"���������"), 8 }, // �����~�������� ��~��������
			//{ std::wstring(  L"���������"), 8 }, // ���~��������
			{ std::wstring( L"���������"), 9 }, // ���~���������
			{ std::wstring(  L"���������"), 8 }, // ����~��������
			{ std::wstring(  L"���������"), 9 }, // ��~���������
			{ std::wstring(      L"�����"), 5 }, // compound
			//{ std::wstring(     L"�����"), 5 }, // ������~����� ������~�����
			//{ std::wstring(     L"�����"), 5 }, // ���~�����
			//{ std::wstring(     L"������"), 5 }, // �����~�����
			//{ std::wstring(     L"�����"), 5 }, // ��������~�����
			//{ std::wstring(     L"������"), 5 }, // �����~�����
			//{ std::wstring(     L"������"), 5 }, // ������~�����
			//{ std::wstring(     L"'�����"), 5 }, // �'~�����
			{ std::wstring(      L"�����"), 4 }, // �����~����
			{ std::wstring(      L"�����"), 4 }, // ���~���� ���~����
			//{ std::wstring(      L"�����"), 4 }, // ��~���� ����������~����
			{ std::wstring(     L"�����"), 5 }, // ��~����� ����������~�����
			{ std::wstring(      L"�����"), 5 }, // compound
			//{ std::wstring(     L"������"), 5 }, // ������~�����
			//{ std::wstring(     L"������"), 5 }, // ��~�����
			//{ std::wstring(     L"������"), 5 }, // �~�����
			//{ std::wstring(     L"������"), 5 }, // ����������~�����
			//{ std::wstring(     L"������"), 5 }, // ��������~�����
			//{ std::wstring(     L"������"), 5 }, // �����~�����
			{ std::wstring(      L"�����"), 4 }, // ������~����
			{ std::wstring(        L"���"), 3, WordClass::Verb }, // ����~�� ����~�� �����~���
			{ std::wstring(        L"���"), 3, WordClass::VerbalAdverb }, // �����~���
			{ std::wstring(      L"�����"), 4 }, // ������~����
			{ std::wstring(    L"�������"), 6 }, // ��������~������
			{ std::wstring(    L"�������"), 6 }, // �����~������
			{ std::wstring(      L"�����"), 4 }, // �����~���� ��~����
			//{ std::wstring(      L"�����"), 4 }, // ����������~����
			{ std::wstring(     L"�����"), 5 }, // ����������~�����
			{ std::wstring(      L"�����"), 5 }, // ����~����
			{ std::wstring(      L"�����"), 5 }, // �����~����
			{ std::wstring(      L"�����"), 5 }, // ���~����
			{ std::wstring(      L"�����"), 4 }, // ������~����
			{ std::wstring(    L"�������"), 6 }, // ��������~������
			{ std::wstring(    L"�������"), 6 }, // �����~������
			{ std::wstring(      L"�����"), 4 }, // �����~���� ��~����
			{ std::wstring(      L"�����"), 4 }, // ����~����
			//{ std::wstring(      L"�����"), 4 }, // ����������~����
			{ std::wstring(     L"�����"), 5 }, // ����������~�����
			{ std::wstring(      L"�����"), 4 }, // �����~����
			{ std::wstring(      L"�����"), 4 }, // ��~�����
			{ std::wstring(     L"������"), 5, WordClass::VerbalAdverb }, // ������~����� ���~�����
			{ std::wstring(     L"������"), 5, WordClass::VerbalAdverb }, // ����~�����
			{ std::wstring(   L"��������"), 7, WordClass::VerbalAdverb }, // ��������~�������
			{ std::wstring(   L"��������"), 7, WordClass::VerbalAdverb }, // �����~������� ������~�������
			{ std::wstring(     L"������"), 5, WordClass::VerbalAdverb }, // ��������~����� ��~�����
			//{ std::wstring(     L"������"), 5, WordClass::VerbalAdverb }, // ����������~�����
			{ std::wstring(    L"������"), 6, WordClass::VerbalAdverb }, // ����������~������
			{ std::wstring(     L"������"), 5, WordClass::VerbalAdverb }, // ������~����� ������~�����

			{ std::wstring(        L"���"), 3, WordClass::Participle }, // compound
			//{ std::wstring(       L"����"), 3, WordClass::Participle }, // ���� ������~���
			//{ std::wstring(       L"����"), 3, WordClass::Participle }, // ������~��� ��~���
			//{ std::wstring(       L"����"), 3, WordClass::Participle }, // �~���
			//{ std::wstring(       L"����"), 3, WordClass::Participle }, // ����������~���
			//{ std::wstring(       L"����"), 3, WordClass::Participle }, // ��������~���
			//{ std::wstring(       L"����"), 3, WordClass::Participle }, // �����~���
			{ std::wstring(    L"�������"), 6, WordClass::Participle }, // ���~������
			{ std::wstring(  L"���������"), 8, WordClass::Participle }, // ��������~��������
			{ std::wstring(  L"���������"), 8, WordClass::Participle }, // �����~�������� ������~��������
			{ std::wstring(    L"�������"), 6, WordClass::Participle }, // �����~������ ��~������
			//{ std::wstring(    L"�������"), 6 }, // ����������~������
			{ std::wstring(   L"�������"), 7, WordClass::Participle  }, // ����������~�������
			{ std::wstring(    L"�������"), 6, WordClass::Participle }, // ����~������
			{ std::wstring(    L"�������"), 6, WordClass::Participle }, // �����~������
			{ std::wstring(    L"�������"), 7, WordClass::Participle  }, // ��~�������
			{ std::wstring(       L"����"), 3 }, // ����~��� ������~��� ������~���
			{ std::wstring(     L"������"), 5 }, // �������~�����
			{ std::wstring(       L"����"), 4 }, // compound
			//{ std::wstring(      L"����"), 4 }, // ������~����
			//{ std::wstring(      L"����"), 4 }, // ��������~����
			//{ std::wstring(      L"�����"), 4 }, // �����~����
			//{ std::wstring(      L"'����"), 4 }, // �'~����
			{ std::wstring(   L"��������"), 7 }, // ���~�������
			{ std::wstring(   L"��������"), 7 }, // �����~������� ��~�������
			//{ std::wstring(   L"��������"), 7 }, // ���~�������
			{ std::wstring(  L"��������"), 8 }, // ���~��������
			{ std::wstring(   L"��������"), 7 }, // ����~�������
			{ std::wstring( L"����������"), 9 }, // ��������~���������
			{ std::wstring( L"����������"), 9 }, // �����~���������
			{ std::wstring(   L"��������"), 8 }, // ��~��������
			{ std::wstring(       L"����"), 3 }, // �����~���
			//{ std::wstring(       L"����"), 3 }, // ����������~���
			{ std::wstring(      L"����"), 4 }, // ����������~����
			//{ std::wstring(        L"���"), 2 }, // no words ����~��
			{ std::wstring(       L"����"), 4 }, // �����~����
			{ std::wstring(      L"�����"), 4 }, // ������~����
			{ std::wstring(    L"�������"), 6 }, // ��������~������
			{ std::wstring(    L"�������"), 6 }, // �����~������
			{ std::wstring(      L"�����"), 4 }, // ����~����
			{ std::wstring(      L"�����"), 4 }, // �����~���� ��~����
			//{ std::wstring(      L"�����"), 4 }, // ����������~����
			{ std::wstring(     L"�����"), 5 }, // ����������~�����
			{ std::wstring(      L"�����"), 4 }, // ����~����
			{ std::wstring(      L"�����"), 4 }, // �����~����
			{ std::wstring(      L"�����"), 5 }, // ��~�����
			{ std::wstring(       L"����"), 4 }, // typo:������~����
			{ std::wstring(      L"�����"), 4 }, // ����~����
			{ std::wstring(      L"�����"), 4 }, // ����~���� �������~����
			{ std::wstring(    L"�������"), 6 }, // �������~������
			{ std::wstring(  L"���������"), 8 }, // ������~��������
			{ std::wstring(L"�����������"), 10 }, // ��������~����������
			{ std::wstring(L"�����������"), 10 }, // �����~����������
			{ std::wstring(  L"���������"), 8 }, // �����~�������� ��~��������
			//{ std::wstring(  L"���������"), 8 }, // ���~��������
			{ std::wstring( L"���������"), 9 }, // ���~���������
			{ std::wstring(  L"���������"), 8 }, // ����~��������
			{ std::wstring(  L"���������"), 8 }, // ���~��������
			{ std::wstring(      L"�����"), 5 }, // compound
			//{ std::wstring(     L"�����"), 5 }, // ����~����� ������~�����
			//{ std::wstring(     L"�����"), 5 }, // ��������~�����
			//{ std::wstring(     L"������"), 5 }, // �����~�����
			//{ std::wstring(     L"'�����"), 5 }, // �'~�����
			{ std::wstring(      L"�����"), 4 }, // �����~���� ������-����
			{ std::wstring(      L"�����"), 4 }, // ���~����� ���~����
			//{ std::wstring(      L"�����"), 4 }, // no words
			//{ std::wstring(     L"�����"), 4 }, // too little words (��~�����)
			{ std::wstring(     L"�����"), 5 }, // ��~����� ����������~�����
			{ std::wstring(      L"�����"), 5 }, // compound
			//{ std::wstring(     L"������"), 5 }, // ������~����� �����~�����
			//{ std::wstring(     L"������"), 5 }, // ��~�����
			//{ std::wstring(     L"������"), 5 }, // �~�����
			//{ std::wstring(     L"������"), 5 }, // ����������~�����
			//{ std::wstring(     L"������"), 5 }, // ��������~�����
			//{ std::wstring(     L"������"), 5 }, // �����~�����
			{ std::wstring(      L"�����"), 4 }, // ��������~���� �����~���� ��������~����
			{ std::wstring(        L"���"), 3 }, // ���~��� ������~���
			{ std::wstring(      L"�����"), 4 }, // �������~����
			{ std::wstring(    L"�������"), 6 }, // ���~������
			{ std::wstring(    L"�������"), 6 }, // ����~������
			//{ std::wstring(    L"�������"), 6 }, // ���~������
			{ std::wstring(   L"�������"), 7 }, // ���~�������
			{ std::wstring(  L"���������"), 8 }, // ��������~��������
			{ std::wstring(  L"���������"), 8 }, // �����~��������
			{ std::wstring(    L"�������"), 6 }, // �����~������ ��~������
			{ std::wstring(    L"�������"), 6 }, // ��~�������
			{ std::wstring(       L"����"), 4 }, // ���~����
			{ std::wstring(     L"������"), 5 }, // �������~�����
			{ std::wstring(   L"��������"), 7 }, // ������~�������
			{ std::wstring(   L"��������"), 7 }, // ����~�������
			{ std::wstring( L"����������"), 9 }, // ��������~���������
			{ std::wstring( L"����������"), 9 }, // �����~���������
			{ std::wstring(   L"��������"), 7 }, // �����~������� ��~�������
			//{ std::wstring(   L"��������"), 7 }, // ���~�������
			{ std::wstring(  L"��������"), 8 }, // ���~��������
			{ std::wstring(   L"��������"), 8 }, // ��~��������
			{ std::wstring(       L"����"), 4 }, // compound
			//{ std::wstring(      L"����"), 4 }, // ������~����
			//{ std::wstring(      L"����"), 4 }, // ��������~����
			//{ std::wstring(      L"�����"), 4 }, // �����~����
			//{ std::wstring(      L"'����"), 4 }, // �'~����
			{ std::wstring(       L"����"), 4 }, // ����~����
			//{ std::wstring(       L"����"), 4 }, // ��~���� ����������~����
			{ std::wstring(      L"����"), 4 }, // ��~���� ����������~����
			{ std::wstring(        L"���"), 2 }, // ��������~��, do not work without SA
			//{ std::wstring(       L"����"), 4 }, // Must have vowel (Ut1sa) NOT:������*��~��
			{ std::wstring(      L"�����"), 4 }, // ������~����
			{ std::wstring(    L"�������"), 6 }, // ��������~������
			{ std::wstring(    L"�������"), 6 }, // �����~������
			{ std::wstring(      L"�����"), 4 }, // ����~���� ������~����
			{ std::wstring(    L"�������"), 6 }, // �������~������
			{ std::wstring(  L"���������"), 8 }, // ������~��������
			{ std::wstring(L"�����������"), 10 }, // �����~����������
			{ std::wstring(L"�����������"), 10 }, // ��������~����������
			{ std::wstring(L"���������"), 8 }, // �����~�������� ��~��������
			//{ std::wstring(  L"���������"), 8 }, // ���~��������
			{ std::wstring( L"���������"), 9 }, // ���~���������
			{ std::wstring(  L"���������"), 8 }, // ����~��������
			{ std::wstring(  L"���������"), 9 }, // ��~���������

			{ std::wstring(      L"�����"), 5 }, // compound
			//{ std::wstring(     L"�����"), 5 }, // ������~�����
			//{ std::wstring(     L"�����"), 5 }, // ��������~�����
			//{ std::wstring(     L"������"), 5 }, // �����~�����
			//{ std::wstring(     L"'�����"), 5 }, // �'~�����
			{ std::wstring(      L"�����"), 4 }, // �����~���� ��~����
			{ std::wstring(      L"�����"), 4 }, // ���~���� ���~���� �����~����
			//{ std::wstring(      L"�����"), 5 }, // ����������~�����
			{ std::wstring(     L"�����"), 5 }, // ����������~����� �������~�����
			{ std::wstring(      L"�����"), 4 }, // ����~����
			{ std::wstring(      L"�����"), 4 }, // �����~���� ����~����
			{ std::wstring(    L"�������"), 6 }, // �������~������
			{ std::wstring(  L"���������"), 8 }, // ������~��������
			{ std::wstring(  L"���������"), 8 }, // ����~��������
			{ std::wstring(L"�����������"), 10 }, // ��������~����������
			{ std::wstring(L"�����������"), 10 }, // �����~����������
			{ std::wstring(  L"���������"), 8 }, // �����~�������� ��~��������
			//{ std::wstring(  L"���������"), 9 }, // ���~���������
			{ std::wstring( L"���������"), 9 }, // ���~���������
			{ std::wstring(  L"���������"), 9 }, // ��~���������
			{ std::wstring(      L"�����"), 5 }, // compound ���~�����
			//{ std::wstring(     L"������"), 5 }, // ������~�����
			//{ std::wstring(     L"������"), 5 }, // �����~�����
			//{ std::wstring(     L"'�����"), 5 }, // �'~�����
			//{ std::wstring(      L"�����"), 4 }, // �������~����
			{ std::wstring(     L"������"), 5 }, // ��������~�����
			{ std::wstring(      L"�����"), 5}, // �������~����� ����~�����
			{ std::wstring(     L"������"), 5 }, // �������~����� ���~����� ��~�����
			{ std::wstring(        L"���"), 3 }, // compound �����~��� ����~���
			//{ std::wstring(       L"����"), 3 }, // ���~��� ������~���
			//{ std::wstring(       L"����"), 3 }, // ��~��� ����������~��� ���~���
			//{ std::wstring(       L"����"), 3 }, // ��������~���
			//{ std::wstring(       L"����"), 3 }, // �����~��� �����~���
			//{ std::wstring(       L"'���"), 3 }, // �'~���
		};

		static bool sureSuffixesInitialized = false;
		if (!sureSuffixesInitialized)
		{
			sureSuffixesInitialized = true;
			sureSuffixes = std::move(sureSuffixesStatic);;

			for (const SuffixEnd& suffixEnd : sureSuffixes)
			{
				if (suffixEnd.TakeCharsCount > suffixEnd.MatchSuffix.size())
				{
					PG_Assert(false && "actual suffix size must be <= of possible suffix size");
				}
			}
			std::sort(sureSuffixes.begin(), sureSuffixes.end(), [](SuffixEnd& a, SuffixEnd& b)
			{
				if (a.MatchSuffix.size() != b.MatchSuffix.size())
					return a.MatchSuffix.size() > b.MatchSuffix.size();
				return a.MatchSuffix < b.MatchSuffix;
			});
			for (size_t i = 0; i<sureSuffixes.size()-1; ++i)
			{
				const auto& curSuffix = sureSuffixes[i].MatchSuffix;
				if (curSuffix == sureSuffixes[i + 1].MatchSuffix && sureSuffixes[i].WordClass == sureSuffixes[i+1].WordClass)
				{
					PG_Assert(false && "Duplicate suffixs");
				}
			}
		}
	}

	bool isValidPhoneticSplit(wv::slice<wchar_t> word, int splitPos)
	{
		bool correct = splitPos > 0 && splitPos <= word.size();
		if (!correct)
			return false;

		PG_Assert(splitPos > 0);

		wchar_t prefixLastChar = word[splitPos - 1];
		wchar_t suffixFirstChar = word[splitPos];
		int suffixSize = word.size() - splitPos;

		// soft character modifies the previous character, the can't be separated
		if (suffixFirstChar == L'�')
			return false;

		// WHY?
		// allow suffixes like "jmo"
		//if (suffixFirstChar == L'�' && suffixSize == 1)
		//	return false;

		// apostrophe makes previous consonant stronger, do not separate it from prefix
		if (suffixFirstChar == L'\'' || suffixFirstChar == L'\'')
			return false;

		return true;
	}

	// Tries to split the word into two parts, so that the phonetic transcription is not corrupted.
	// Returns prefix size or -1 if word can't be split.
	int phoneticSplitOfWord(wv::slice<wchar_t> word, boost::optional<WordClass> wordClass, int* pMatchedSuffixInd)
	{
		ensureSureSuffixesInitialized();

		for (size_t suffixInd = 0; suffixInd < sureSuffixes.size(); ++suffixInd)
		{
			const SuffixEnd& suffixEnd = sureSuffixes[suffixInd];
			WordClass suffixClass = suffixEnd.WordClass;

			//WordClass suffixClass = suffixEnd.WordClass;
			//if (suffixClass == WordClass::Participle || suffixClass == WordClass::VerbalAdverb)
			//	suffixClass = WordClass::Verb;

			//WordClass wordClassTmp = wordClass.get();
			//if (wordClassTmp == WordClass::Participle || wordClassTmp == WordClass::VerbalAdverb)
			//	wordClassTmp = WordClass::Verb;

			if (wordClass != nullptr)
			{
				// match word class and suffix (word) class
				WordClass wordClassTmp = wordClass.get();
				if (wordClassTmp != suffixClass)
					continue;
			}

			const std::wstring& suffix = suffixEnd.MatchSuffix;
			if (endsWith<wchar_t>(word, suffix))
			{
				int suffixSize = suffixEnd.TakeCharsCount;
				int prefixSize = (int)word.size() - suffixSize;
				
				// prohibit detaching the soft sign from prefix
				if (word[prefixSize] == L'�' && prefixSize + 1 < word.size())
					prefixSize++;

				// trim vowels at the end of the prefix
				bool trimEndVowels = false;
				if (trimEndVowels)
				{
					bool isPrefixEndsVowel = true;
					while (prefixSize > 0 && isPrefixEndsVowel)
					{
						wchar_t prefixLastChar = word[prefixSize - 1];
						isPrefixEndsVowel = !PticaGovorun::isUkrainianConsonant(prefixLastChar);
						if (isPrefixEndsVowel)
							prefixSize--;
					}
				}

				// avoid short prefixes, as they will not participate in other words construction frequently
				// ��������� -> �
				// need prefixes with size>1 to distinguish (��~���, �~���)
				if (prefixSize <= 1) continue;

				if (!isValidPhoneticSplit(word, prefixSize))
					continue;

				if (wordClass == WordClass::Participle && suffixEnd.WordClass != WordClass::Participle)
				{
					participleSuffixToWord[suffixEnd.MatchSuffix] = std::wstring(word.data(), word.size());
				}
				if (wordClass == WordClass::VerbalAdverb && suffixEnd.WordClass != WordClass::VerbalAdverb)
				{
					participleSuffixToWord2[suffixEnd.MatchSuffix] = std::wstring(word.data(), word.size());
				}

				if (pMatchedSuffixInd != nullptr)
					*pMatchedSuffixInd = (int)suffixInd;
				
				return prefixSize;
			}
		}

		return -1;
	}

	bool isUnvoicedCharUk(wchar_t ch)
	{
		// ���������
		// ���������
		return
			ch == L'�' || ch == L'�' ||
			ch == L'�' || ch == L'�' ||
			ch == L'�' || ch == L'�' ||
			ch == L'�' || ch == L'�' ||
			ch == L'�' || ch == L'�' ||
			ch == L'�' || ch == L'�' ||
			ch == L'�' || ch == L'�' ||
			ch == L'�' || ch == L'�' ||
			ch == L'�' || ch == L'�';
	}

	// Returns number of made transformations or zero if the map was not changed.
	int reuseCommonPrefixesOneIteration(std::map<std::wstring, int>& mapPrefixToSize)
	{
		if (mapPrefixToSize.size() <= 1)
			return 0;

		std::vector<std::wstring> prefixes(mapPrefixToSize.size());
		std::transform(std::begin(mapPrefixToSize), std::end(mapPrefixToSize), std::begin(prefixes), [](const std::pair<std::wstring, int>& pair)
		{
			return pair.first;
		});
		std::sort(std::begin(prefixes), std::end(prefixes), [](std::wstring& a, std::wstring& b)
		{
			return a.size() < b.size();
		});

		int changesMade = 0;
		std::vector<uchar> processed(prefixes.size());
		for (size_t i = 0; i < processed.size(); ++i)
		{
			if (processed[i])
				continue;
			processed[i] = true;
			for (size_t j = i; j < processed.size(); ++j)
			{
				if (processed[j])
					continue;
				const std::wstring& s1 = prefixes[i];
				const std::wstring& s2 = prefixes[j];
				assert(s1.size() <= s2.size() && "Strings are ordered in ascending order");

				size_t prefixSize = commonPrefixSize<wchar_t>(s1, s2);
				auto countConsonant = [](wchar_t ch) { return isUkrainianConsonant(ch); };
				size_t conson1 = std::count_if(s1.begin() + prefixSize, s1.end(), countConsonant);
				size_t conson2 = std::count_if(s2.begin() + prefixSize, s2.end(), countConsonant);

				bool canSplit1 = isValidPhoneticSplit(s1, prefixSize);
				bool canSplit2 = isValidPhoneticSplit(s1, prefixSize);

				// can reduce two prefixes only if they differ in ending of all vowels
				bool canReduce = (prefixSize < s1.size() || prefixSize < s2.size()) && conson1 == 0 && conson2 == 0 && canSplit1 && canSplit2;
				if (!canReduce)
					continue;

				if (prefixSize < s1.size() ^ prefixSize < s2.size()) // one is the prefix of another
				{
					// keep shorter prefix, remove longer prefix
					const std::wstring *shorter = nullptr;
					const std::wstring *longer = nullptr;
					size_t longerWordInd;
					if (prefixSize < s1.size())
					{
						shorter = &s1;
						longer = &s2;
						longerWordInd = j;
					}
					else
					{
						shorter = &s2;
						longer = &s1;
						longerWordInd = i;
					}
					int usedCount = mapPrefixToSize[*longer];
					mapPrefixToSize.erase(*longer);

					mapPrefixToSize[*shorter] += usedCount;

					processed[longerWordInd] = true;
				}
				else
				{
					int usedCount1 = mapPrefixToSize[s1];
					int usedCount2 = mapPrefixToSize[s2];
					mapPrefixToSize.erase(s1);
					mapPrefixToSize.erase(s2);

					std::wstring prefix(s1.data(), prefixSize);
					mapPrefixToSize[prefix] += usedCount1 + usedCount2;
					processed[i] = true;
					processed[j] = true;
				}
				changesMade++;
			}
		}
		return changesMade;
	}

	void reuseCommonPrefixes(std::map<std::wstring, int>& mapPrefixToSize)
	{
		while (reuseCommonPrefixesOneIteration(mapPrefixToSize) > 0) {}
	}

	bool isVoiceless(wchar_t ch)
	{
		bool voiceless = ch == L'�'; // ������
		return voiceless;
	}

	UkrainianPhoneticSplitter::UkrainianPhoneticSplitter()
	{
		bool wasAdded = false;
		sentStartWordPart_ = wordUsage_.getOrAddWordPart(L"<s>", WordPartSide::WholeWord, &wasAdded);
		CV_Assert(wasAdded);
		sentEndWordPart_ = wordUsage_.getOrAddWordPart(L"</s>", WordPartSide::WholeWord, &wasAdded);
		CV_Assert(wasAdded);
	}

	void UkrainianPhoneticSplitter::bootstrap(const std::unordered_map<std::wstring, std::unique_ptr<WordDeclensionGroup>>& words, const std::wstring& targetWord, const std::unordered_set<std::wstring>& processedWords, int& totalWordsCount)
	{
		totalWordsCount = 0;
		for (const auto& pair : words)
		{
			const WordDeclensionGroup& wordGroup = *pair.second;
			bool contains = processedWords.find(wordGroup.Name) != processedWords.end();
			if (false && !contains)
				continue;

			if (!targetWord.empty() && wordGroup.Name != targetWord)
				continue;

			if (allowPhoneticWordSplit_)
			{
				for (const WordDeclensionForm& declWord : wordGroup.Forms)
				{
					const WordPart* wordPart = wordUsage_.getOrAddWordPart(declWord.Name, WordPartSide::WholeWord);
				}
				continue;
			}

			if (wordGroup.WordClass == WordClass::Irremovable ||
				wordGroup.WordClass == WordClass::Preposition ||
				wordGroup.WordClass == WordClass::Pronoun ||
				wordGroup.WordClass == WordClass::Conjunction ||
				wordGroup.WordClass == WordClass::Interjection ||
				wordGroup.WordClass == WordClass::Particle ||
				wordGroup.WordClass == WordClass::Irremovable)
			{
				// keep the word intact
				continue;
			}
			else if (
				wordGroup.WordClass == WordClass::Adjective ||
				wordGroup.WordClass == WordClass::Adverb ||
				wordGroup.WordClass == WordClass::Noun ||
				wordGroup.WordClass == WordClass::Numeral ||
				wordGroup.WordClass == WordClass::Verb ||
				wordGroup.WordClass == WordClass::VerbalAdverb ||
				wordGroup.WordClass == WordClass::Participle)
			{
				std::vector<std::wstring> unsplitWords;
				std::map<std::wstring, int> mapPrefixSizeToCount;
				for (const WordDeclensionForm& wordForm : wordGroup.Forms)
				{
					const std::wstring& word = wordForm.Name;

					// split comma separated words into words
					size_t newOffset;
					for (size_t offset = 0; offset < word.size(); offset = newOffset + 1) // +1 to skip comma
					{
						if (word[offset] == L'*')
							offset++;
						newOffset = word.find(L",", offset);
						if (newOffset == (size_t)-1)
							newOffset = word.size();

						wv::slice<wchar_t> subWord = wv::make_view(word.data() + offset, newOffset - offset);
						totalWordsCount++;

						int matchedSuffixInd = -1;
						WordClass curWordClass = wordGroup.WordClass.get();
						if (wordForm.WordClass != nullptr)
							curWordClass = wordForm.WordClass.get();

						int sepInd = phoneticSplitOfWord(subWord, curWordClass, &matchedSuffixInd);
						if (sepInd != -1)
							sureSuffixes[matchedSuffixInd].UsedCount++;

						if (sepInd == -1)
						{
							bool voiceless = isVoiceless(subWord[subWord.size() - 1]);

							wchar_t wordLastChar = subWord[subWord.size() - 1];
							bool ok =
								wordLastChar == L'�' ||
								wordLastChar == L'�' || // ������
								voiceless;

							// it is ok to not finding consonant+sa
							if (!ok && endsWith(subWord, (wv::slice<wchar_t>)std::wstring(L"��")))
							{
								wchar_t prefixLastChar = subWord[subWord.size() - 1 - 2];
								ok = isUkrainianConsonant(prefixLastChar);
							}
							if (!ok)
							{
								std::wstring subWordStr(subWord.data(), subWord.size());
								unsplitWords.push_back(subWordStr);
								//::DebugBreak();
								//::OutputDebugStringW(L"Suffix was not found for word: ");
								//::OutputDebugStringW(subWordStr.data());
								//::OutputDebugStringW(L"\n");
							}
						}

						//if (sepInd != -1)
						//{
						//	wv::slice<wchar_t> prefixWord = wv::make_view(subWord.data(), sepInd);
						//	static std::wstring suffixVa(L"��");
						//	if (endsWith(prefixWord, wv::make_view(suffixVa)))
						//	{
						//		// skip -va words
						//		//continue;
						//	}
						//}

						// store prefix to usage count
						{
							int sepIndTmp = sepInd;
							if (sepIndTmp == -1)
								sepIndTmp = subWord.size();

							// separation position was not found, use the whole word
							std::wstring prefixStr(subWord.data(), 0, sepIndTmp);
							mapPrefixSizeToCount[prefixStr]++;
						}

						int partsCount;
						std::array<std::wstring, 2> partsStrings;
						std::array<WordPartSide, 2> partsSides;
						if (sepInd == -1)
						{
							// separation position was not found, use the whole word
							partsCount = 1;
							partsStrings[0] = toString(subWord);
							partsSides[0] = WordPartSide::WholeWord;
						}
						else
						{
							partsCount = 2;

							wv::slice<wchar_t> prefix = wv::make_view(subWord.data(), sepInd);
							wv::slice<wchar_t> suffix = wv::make_view(subWord.data() + sepInd, subWord.size() - sepInd);

							partsStrings[0] = toString(prefix);
							partsSides[0] = WordPartSide::LeftPart;
							partsStrings[1] = toString(suffix);
							partsSides[1] = WordPartSide::RightPart;
						}

						//
						ShortArray<int, 2> splitParts;
						splitParts.Array.fill(-1);
						splitParts.ActualSize = partsCount;

						for (int partInd = 0; partInd < partsCount; ++partInd)
						{
							const std::wstring& partStr = partsStrings[partInd];
							WordPartSide partSide = partsSides[partInd];

							const WordPart* wordPart = wordUsage_.getOrAddWordPart(partStr, partSide);

							//WordSeqKey wordIds({ wordPart->id() });
							//WordSeqUsage* wordSeq = wordUsage_.getOrAddWordSequence(wordIds);
							//wordSeq->UsedCount++;

							splitParts.Array[partInd] = wordPart->id();
						}
						wordStrToPartIds_[word] = splitParts;
					}
				}

				size_t diffPrefixCountBefore = mapPrefixSizeToCount.size();
				reuseCommonPrefixes(mapPrefixSizeToCount);

				size_t diffPrefixCount = mapPrefixSizeToCount.size();
				for (const auto& prefixToCountPair : mapPrefixSizeToCount)
				{
					const auto& prefix = prefixToCountPair.first;
					wchar_t ch = prefix[prefix.size() - 1];
					bool voiceless = isVoiceless(ch);
					if (ch == L'�' || voiceless || ch == L'-')
						diffPrefixCount--;
				}
				//std::wcout << L"word finished diffPrefixCount=" << diffPrefixCount << std::endl;
			}
			else
				::DebugBreak(); // unknown word class
		}
		const auto& m1 = participleSuffixToWord;
		const auto& m2 = participleSuffixToWord2;
		OutputDebugStringW(L"PARTICIPLE\n");
		for (const auto& pair : participleSuffixToWord)
		{
			OutputDebugStringW(pair.first.c_str());
			OutputDebugStringW(L"\t");
			OutputDebugStringW(pair.second.c_str());
			OutputDebugStringW(L"\n");
		}
		OutputDebugStringW(L"VERBALADVERB\n");
		for (const auto& pair : participleSuffixToWord2)
		{
			OutputDebugStringW(pair.first.c_str());
			OutputDebugStringW(L"\t");
			OutputDebugStringW(pair.second.c_str());
			OutputDebugStringW(L"\n");
		}
	}

	const WordsUsageInfo& UkrainianPhoneticSplitter::wordUsage() const
	{
		return wordUsage_;
	}
	WordsUsageInfo& UkrainianPhoneticSplitter::wordUsage()
	{
		return wordUsage_;
	}

	void UkrainianPhoneticSplitter::printSuffixUsageStatistics() const
	{
		auto sureSuffixesCopy = sureSuffixes;
		std::sort(sureSuffixesCopy.begin(), sureSuffixesCopy.end(), [](SuffixEnd& a, SuffixEnd& b)
		{
			return a.UsedCount > b.UsedCount;
		});
		OutputDebugStringW(L"Suffix usage statistics:\n");
		for (const auto& suffixEnd : sureSuffixesCopy)
		{
			OutputDebugStringW(suffixEnd.MatchSuffix.data());
			OutputDebugStringW(L"\t");
			OutputDebugStringW(QString::number(suffixEnd.UsedCount).toStdWString().c_str());

			wchar_t* classStr = L"";
			if (suffixEnd.WordClass == WordClass::Verb)
				classStr = L"v";
			else if (suffixEnd.WordClass == WordClass::Noun)
				classStr = L"n";
			else if (suffixEnd.WordClass == WordClass::Adjective)
				classStr = L"adj";
			else if (suffixEnd.WordClass == WordClass::Adverb)
				classStr = L"adverb";
			OutputDebugStringW(L" ");
			OutputDebugStringW(classStr);
			OutputDebugStringW(L"\n");
		}
	}

	long UkrainianPhoneticSplitter::wordSeqCount(int wordsPerSeq) const
	{
		PG_Assert(wordsPerSeq <= 2);
		if (wordsPerSeq == 1)
			return seqOneWordCounter_;
		return seqTwoWordsCounter_;
	}

	const WordPart* UkrainianPhoneticSplitter::sentStartWordPart() const
	{
		return sentStartWordPart_;
	}

	const WordPart* UkrainianPhoneticSplitter::sentEndWordPart() const
	{
		return sentEndWordPart_;
	}

	void UkrainianPhoneticSplitter::setAllowPhoneticWordSplit(bool value)
	{
		allowPhoneticWordSplit_ = value;
	}

	void UkrainianPhoneticSplitter::buildLangModel(const wchar_t* textFilesDir, long& totalPreSplitWords, int maxFileToProcess, bool outputCorpus)
	{
		QFile corpusFile;
		QTextStream corpusStream;
		if (outputCorpus)
		{
			std::wstringstream corpusFileName;
			corpusFileName << "persianCorpus.";
			appendTimeStampNow(corpusFileName);
			corpusFileName << ".txt";

			corpusFile.setFileName(QString::fromStdWString(corpusFileName.str()));
			if (!corpusFile.open(QIODevice::WriteOnly | QIODevice::Text))
				return;
			corpusStream.setDevice(&corpusFile);
			corpusStream.setCodec("UTF-8");
		}

		QXmlStreamReader xml;
		totalPreSplitWords = 0;

		std::vector<wv::slice<wchar_t>> words;
		words.reserve(64);
		std::vector<const WordPart*> wordParts;
		wordParts.reserve(1024);
		TextParser wordsReader;

		QString textFilesDirQ = QString::fromStdWString(textFilesDir);
		QDirIterator it(textFilesDirQ, QStringList() << "*.fb2", QDir::Files, QDirIterator::Subdirectories);
		int processedFiles = 0;
		while (it.hasNext())
		{
			if (maxFileToProcess != -1 && processedFiles == maxFileToProcess)
				break;

			//if (processedFiles >= 2) break; // process less work

			QString txtPath = it.next();
			if (txtPath.contains("BROKEN", Qt::CaseSensitive))
			{
				qDebug() << "SKIPPED " << txtPath;
				continue;
			}
			qDebug() << txtPath;

			//
			QFile file(txtPath);
			if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
			{
				qDebug() << "Can't open file " << txtPath;
				return;
			}

			// parse file
			xml.setDevice(&file);
			while (!xml.atEnd())
			{
				xml.readNext();
				if (xml.isCharacters())
				{
					// BUG: if the lines inside the text are separated using LF only on windows machines
					//      the function below will concatenate the lines. For now, fix LF->CRLF for such files externally
					QStringRef elementText = xml.text();

					wv::slice<wchar_t> textToParse = wv::make_view((wchar_t*)elementText.data(), elementText.size());
					wordsReader.setInputText(textToParse);

					// extract all sentences from paragraph
					while (true)
					{
						words.clear();
						if (!wordsReader.parseSentence(words))
							break;
						if (words.empty())
							continue;

						{
							// augment the sentence with start/end terminators
							wordParts.push_back(sentStartWordPart_);

							selectWordParts(words, wordParts, totalPreSplitWords);

							// augment the sentence with start/end terminators
							wordParts.push_back(sentEndWordPart_);
						}

						// calculate statistic only if enough word parts is accumulated
						size_t calcStatWordPartsCount = 1000000;
						if (wordParts.size() > calcStatWordPartsCount)
						{
							calcNGramStatisticsOnWordPartsBatch(wordParts, outputCorpus, corpusStream);
						}
					}
				}
			}
			
			// flush the word parts buffer
			calcNGramStatisticsOnWordPartsBatch(wordParts, outputCorpus, corpusStream);

			++processedFiles;
		}
	}

	void UkrainianPhoneticSplitter::selectWordParts(const std::vector<wv::slice<wchar_t>>& words, std::vector<const WordPart*>& wordParts, long& preSplitWords)
	{
		for (int i = 0; i < words.size(); ++i)
		{
			const wv::slice<wchar_t>& wordSlice = words[i];
			PG_Assert(!wordSlice.empty());

			// keep ukrainian words only
			//auto filterOutWord = [](wv::slice<wchar_t> word)
			//{
			//	for (size_t charInd = 0; charInd < word.size(); ++charInd)
			//	{
			//		wchar_t ch = word[charInd];
			//		bool isDigit = isDigitChar(ch);
			//		//bool isLatin = isEnglishChar(ch);
			//		bool isSureLatin = isExclusiveEnglishChar(ch);
			//		bool isSureRus = isExclusiveRussianChar(ch);
			//		//if (isDigit || isLatin || isSureRus)
			//		if (isDigit || isSureLatin || isSureRus)
			//			return true;
			//	}
			//	return false;
			//};
			//if (filterOutWord(wordSlice))
			//{
			//	::OutputDebugStringW(toString(wordSlice).c_str());
			//	::OutputDebugStringW(L"\n");
			//	continue;
			//}

			auto wordCharUsage = [](wv::slice<wchar_t> word, int& digitsCount,
				int& engCount, int& exclEngCount,
				int& rusCount, int& exclRusCount,
				int& hyphenCount)
			{
				for (size_t charInd = 0; charInd < word.size(); ++charInd)
				{
					wchar_t ch = word[charInd];
					bool isDigit = isDigitChar(ch);
					bool isEng = isEnglishChar(ch);
					bool isExclEng = isExclusiveEnglishChar(ch);
					bool isRus = isRussianChar(ch);
					bool isExclRus = isExclusiveRussianChar(ch);
					if (isDigit)
						digitsCount++;
					if (isEng)
						engCount++;
					if (isExclEng)
						exclEngCount++;
					if (isRus)
						rusCount++;
					if (isExclRus)
						exclRusCount++;
					if (ch == L'-' || ch == L'\'')
						hyphenCount++;
				}
			};
			int digitsCount = 0;
			int engCount = 0;
			int exclEngCount = 0;
			int rusCount = 0;
			int exclRusCount = 0;
			int hyphenCount = 0;
			wordCharUsage(wordSlice, digitsCount, engCount, exclEngCount, rusCount, exclRusCount, hyphenCount);
			if (digitsCount > 0 || exclEngCount > 0 || exclRusCount > 0)
			{
				if (digitsCount == wordSlice.size() ||  // number
					(exclEngCount > 0 && (engCount + hyphenCount) == wordSlice.size()) || // english word
					(exclRusCount > 0 && (rusCount + hyphenCount) == wordSlice.size())    // russian word
					)
				{
					// do not even print the skipped word
					continue;
				}
				else
				{
					::OutputDebugStringW(toString(wordSlice).c_str());
					::OutputDebugStringW(L"\n");
				}
				wordParts.push_back(wordPartSeparator_); // word parts separator
				continue;
			}

			std::wstring str = toString(wordSlice);

			if (allowPhoneticWordSplit_)
			{
				auto preSplitWordIt = wordStrToPartIds_.find(str);
				if (preSplitWordIt != wordStrToPartIds_.end())
				{
					preSplitWords++;
					ShortArray<int, 2>& preSplit = preSplitWordIt->second;

					for (int splitInd = 0; splitInd < preSplit.ActualSize; ++splitInd)
					{
						int wordPartId = preSplit.Array[splitInd];
						const WordPart* wordPartPtr = wordUsage_.wordPartById(wordPartId);
						wordParts.push_back(wordPartPtr);
					}
				}
				else
				{
					doWordPhoneticSplit(str, wordParts);
				}
			}
			else
			{
				const WordPart* wordPart = wordUsage_.getOrAddWordPart(str, WordPartSide::WholeWord);
				wordParts.push_back(wordPart);
			}
		}
	}

	void UkrainianPhoneticSplitter::calcNGramStatisticsOnWordPartsBatch(std::vector<const WordPart*>& wordParts, bool outputCorpus, QTextStream& corpusStream)
	{
		std::vector<const WordPart*> wordPartsStraight;

		// select sequantial word parts without separator
		size_t wordPartInd = 0;
		auto takeWordPartsTillNull = [this, &wordParts, &wordPartInd](std::vector<const WordPart*>& outWordParts) -> bool
		{
			// returns true if result contains data
			while (wordPartInd < wordParts.size())
			{
				for (; wordPartInd < wordParts.size(); ++wordPartInd)
				{
					const WordPart* wordPartPtr = wordParts[wordPartInd];
					if (wordPartPtr == wordPartSeparator_)
					{
						wordPartInd++; // skip separator
						break;
					}
					outWordParts.push_back(wordPartPtr);
				}
				if (!outWordParts.empty())
					return true;
			}
			return false;
		};
		while (true)
		{
			wordPartsStraight.clear();
			if (!takeWordPartsTillNull(wordPartsStraight))
				break;

			calcLangStatistics(wordPartsStraight);

			if (outputCorpus)
			{
				for (const WordPart* wp : wordPartsStraight)
				{
					printWordPart(wp, corpusStream);
					corpusStream << " ";
				}
				corpusStream << "\n";
			}
		}
		// purge word parts buffer
		wordParts.clear();
	}

	void UkrainianPhoneticSplitter::calcLangStatistics(const std::vector<const WordPart*>& wordParts)
	{
		const WordPart* prevWordPart = nullptr;

		for (const WordPart* wordPart : wordParts)
		{
			if (wordPart->partText() == L"��������")
			{
				PG_Assert(true);
			}
			// unimodel
			WordSeqKey oneWordKey({ wordPart->id() });
			WordSeqUsage* oneWordSeq = wordUsage_.getOrAddWordSequence(oneWordKey);
			oneWordSeq->UsedCount++;
			seqOneWordCounter_++;

			// bimodel
			if (prevWordPart != nullptr)
			{
				WordSeqKey twoWordsKey({ prevWordPart->id(), wordPart->id() });
				WordSeqUsage* twoWordsSeq = wordUsage_.getOrAddWordSequence(twoWordsKey);
				twoWordsSeq->UsedCount++;
				seqTwoWordsCounter_++;
			}

			prevWordPart = wordPart;
		}
	}

	void UkrainianPhoneticSplitter::doWordPhoneticSplit(const wv::slice<wchar_t>& wordSlice, std::vector<const WordPart*>& wordParts)
	{
		//const std::wstring& word
		int matchedSuffixInd = -1;
		int sepInd = phoneticSplitOfWord(wordSlice, nullptr, &matchedSuffixInd);
		if (sepInd != -1)
			sureSuffixes[matchedSuffixInd].UsedCount++;

		int partsCount;
		std::array<std::wstring, 2> partsStrings;
		std::array<WordPartSide, 2> partsSides;
		if (sepInd == -1)
		{
			// separation position was not found, use the whole word
			partsCount = 1;
			partsStrings[0] = toString(wordSlice);
			partsSides[0] = WordPartSide::WholeWord;
		}
		else
		{
			partsCount = 2;

			wv::slice<wchar_t> prefix = wv::make_view(wordSlice.data(), sepInd);
			wv::slice<wchar_t> suffix = wv::make_view(wordSlice.data() + sepInd, wordSlice.size() - sepInd);

			partsStrings[0] = toString(prefix);
			partsSides[0] = WordPartSide::LeftPart;
			partsStrings[1] = toString(suffix);
			partsSides[1] = WordPartSide::RightPart;
		}

		for (int partInd = 0; partInd < partsCount; ++partInd)
		{
			const std::wstring& partStr = partsStrings[partInd];
			WordPartSide partSide = partsSides[partInd];

			const WordPart* wordPart = wordUsage_.getOrAddWordPart(partStr, partSide);
			wordParts.push_back(wordPart);
		}
	}
}