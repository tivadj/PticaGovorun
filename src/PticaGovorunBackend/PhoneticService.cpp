#include "stdafx.h"
#include "PhoneticService.h"
#include <array>
#include <QFile>
#include <QDir>
#include <QDirIterator>
#include <QDebug>
#include <QXmlStreamReader>
#include <QString>

namespace PticaGovorun
{
	namespace
	{
		const wchar_t Letter_Hyphen = L'-';
		const wchar_t Letter_Apostrophe = L'\'';
		const wchar_t Letter_D = L'д';
		const wchar_t Letter_JI = L'ї';
		const wchar_t Letter_JE = L'є';
		const wchar_t Letter_ZH = L'ж';
		const wchar_t Letter_Z = L'з';
		const wchar_t Letter_SHCH = L'щ';
		const wchar_t Letter_SoftSign = L'ь';
		const wchar_t Letter_JU = L'ю';
		const wchar_t Letter_JA = L'я';
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
		//wchar_t ukAlpha[] = L"абвгґдеєжзиіїйклмнопрстуфхцчшщьюя";
		static std::array<CharToPhone, 35> ukAlphaArray = {
			CharToPhone{ L'а', UkrainianPhoneId::P_A },
			CharToPhone{ L'б', UkrainianPhoneId::P_B },
			CharToPhone{ L'в', UkrainianPhoneId::P_V },
			CharToPhone{ L'г', UkrainianPhoneId::P_H },
			CharToPhone{ L'ґ', UkrainianPhoneId::P_G },
			CharToPhone{ Letter_D, UkrainianPhoneId::Nil },
			CharToPhone{ L'е', UkrainianPhoneId::P_E },
			CharToPhone{ Letter_JE, UkrainianPhoneId::Nil },
			CharToPhone{ Letter_ZH, UkrainianPhoneId::P_ZH },
			CharToPhone{ Letter_Z, UkrainianPhoneId::P_Z },
			CharToPhone{ L'и', UkrainianPhoneId::P_Y },
			CharToPhone{ L'і', UkrainianPhoneId::P_I },
			CharToPhone{ Letter_JI, UkrainianPhoneId::Nil },
			CharToPhone{ L'й', UkrainianPhoneId::P_J },
			CharToPhone{ L'к', UkrainianPhoneId::P_K },
			CharToPhone{ L'л', UkrainianPhoneId::P_L },
			CharToPhone{ L'м', UkrainianPhoneId::P_M },
			CharToPhone{ L'н', UkrainianPhoneId::P_N },
			CharToPhone{ L'о', UkrainianPhoneId::P_O },
			CharToPhone{ L'п', UkrainianPhoneId::P_P },
			CharToPhone{ L'р', UkrainianPhoneId::P_R },
			CharToPhone{ L'с', UkrainianPhoneId::P_S },
			CharToPhone{ L'т', UkrainianPhoneId::P_T },
			CharToPhone{ L'у', UkrainianPhoneId::P_U },
			CharToPhone{ L'ф', UkrainianPhoneId::P_F },
			CharToPhone{ L'х', UkrainianPhoneId::P_KH },
			CharToPhone{ L'ц', UkrainianPhoneId::P_TS },
			CharToPhone{ L'ч', UkrainianPhoneId::P_CH },
			CharToPhone{ L'ш', UkrainianPhoneId::P_SH },
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
		// good: називати існують
		// verb+t1
		// ити
		static std::vector<SuffixEnd> sureSuffixesStatic = {
			{ std::wstring(    L"а"), 1, WordClass::Noun }, // ручк~а noun
			{ std::wstring(    L"а"), 1, WordClass::Numeral }, // багатомільярдн~а ст~а
			{ std::wstring(    L"а"), 1, WordClass::Adjective }, // ближч~а adj
			{ std::wstring(  L"ала"), 2 }, // сказа~ла
			{ std::wstring(L"увала"), 4 }, // існу~вала актуалізу~вала
			{ std::wstring(L"ювала"), 4 }, // базарю~вала
			{ std::wstring(  L"ила"), 2 }, // жи~ла би~ла
			{ std::wstring(  L"іла"), 2 }, // буркоті~ла
			//{ std::wstring(  L"їла"), 2 }, // благоустрої~ла
			{ std::wstring( L"оїла"), 3 }, // благоустро~їла
			{ std::wstring(  L"ола"), 2 }, // боро~ла
			{ std::wstring(  L"ула"), 2 }, // бамкну~ла
			{ std::wstring(  L"ьма"), 2, WordClass::Numeral}, // дев'ять~ма
			{ std::wstring(  L"ома"), 3, WordClass::Numeral}, // дв~ома дев'ять~ома
			{ std::wstring(  L"іша"), 3, WordClass::Adjective }, // абсурдн~іша

			{ std::wstring(  L"ав"), 1 }, // бува~в назива~в ма~в вплива~в
			{ std::wstring(L"ував"), 3 }, // актуалізу~вав існу~вав
			{ std::wstring(L"ював"), 3 }, // базарю~вав
			//{ std::wstring(  L"ев"), 1 }, // ?? NOT дерев~
			{ std::wstring(  L"ив"), 1 }, // безчести~в би~в
			{ std::wstring(  L"ів"), 1 }, // буркоті~в
			{ std::wstring(  L"ів"), 2, WordClass::Noun }, // фургон~ів noun
			{ std::wstring(  L"їв"), 2, WordClass::Noun }, // аграрі~їв одностро~їв noun
			//{ std::wstring(  L"їв"), 1 }, // однострої~в благоустрої~в
			{ std::wstring( L"іїв"), 2 }, // благоустро~їв
			{ std::wstring( L"оїв"), 2 }, // благоустро~їв verb
			{ std::wstring(  L"ов"), 1 }, // боро~в (weird word, =боровся)
			{ std::wstring(  L"ов"), 2, WordClass::Noun }, // церк~ов
			{ std::wstring(  L"ув"), 1 }, // бамкну~в

			{ std::wstring(        L"е"), 1 }, // бер~е бамкн~е
			{ std::wstring(        L"е"), 1, WordClass::Adjective }, // ближч~е близьк~е
			{ std::wstring(        L"е"), 1, WordClass::Noun }, // фургон~е
			{ std::wstring(        L"е"), 1, WordClass::Numeral }, // багатомільярдн~е
			{ std::wstring(      L"име"), 2 }, // брести~ме
			{ std::wstring(    L"атиме"), 4 }, // назива~тиме
			{ std::wstring(    L"ятиме"), 4 }, // розмовля~тиме
			{ std::wstring(  L"уватиме"), 6 }, // існу~ва~тиме актуалізу~ватиме
			{ std::wstring(  L"юватиме"), 6 }, // базарю~ва~тиме
			{ std::wstring(    L"итиме"), 4 }, // роби~тиме би~тиме
			{ std::wstring(    L"ітиме"), 4 }, // буркоті~тиме
			//{ std::wstring(    L"їтиме"), 4 }, // гної~тиме
			{ std::wstring(   L"оїтиме"), 5 }, // гно~їтиме
			{ std::wstring(    L"отиме"), 4 }, // боро~тиме
			{ std::wstring(    L"утиме"), 4 }, // блякну~тиме
			{ std::wstring(     L"айте"), 3 }, // назива~йте вплива~йте
			{ std::wstring(     L"ійте"), 3 }, // бурі~йте
			{ std::wstring(     L"ойте"), 3 }, // благоустро~йте
			{ std::wstring(     L"уйте"), 3 }, // у~йте існу~йте
			{ std::wstring(     L"юйте"), 3 }, // дорівню~йте
			//{ std::wstring(       L"те"), 2 }, // буркоть~те
			{ std::wstring(      L"ете"), 2 }, // бере~те бамкне~те
			//{ std::wstring(     L"мете"), 4 }, // візь~мете
			{ std::wstring(    L"имете"), 4 }, // іти~мете
			{ std::wstring(  L"атимете"), 6 }, // існу~ва~тимете ма~тимете бра-тимете
			{ std::wstring(  L"итимете"), 6 }, // безчести~тимете би~тимете
			{ std::wstring(  L"ітимете"), 6 }, // буркоті~тимете
			//{ std::wstring(  L"їтимете"), 6 }, // гної~тимете
			{ std::wstring(  L"оїтимете"), 7 }, // гно~їтимете
			{ std::wstring(  L"отимете"), 6 }, // боро~тимете
			{ std::wstring(  L"утимете"), 6 }, // блякну~тимете
			{ std::wstring(L"уватимете"), 8 }, // актуалізу~ватимете
			{ std::wstring(L"юватимете"), 8 }, // дорівню~ватимете
			{ std::wstring(    L"ьмете"), 4 }, // візь~мете
			{ std::wstring(     L"аєте"), 3 }, // зна~єте
			{ std::wstring(     L"ієте"), 3 }, // бурі~єте
			{ std::wstring(     L"уєте"), 3 }, // існу~єте
			{ std::wstring(     L"юєте"), 3 }, // дорівню~єте
			{ std::wstring(     L"'єте"), 3 }, // б'~єте
			{ std::wstring(      L"ите"), 2 }, // буркоти~те
			{ std::wstring(      L"іте"), 2 }, // бері~те бамкні~те
			//{ std::wstring(      L"їте"), 2 }, // благоустрої~те
			{ std::wstring(     L"оїте"), 3 }, // благоустро~їте
			{ std::wstring(     L"ийте"), 3 }, // би~йте
			{ std::wstring(      L"ьте"), 2 }, // буркоть~те
			{ std::wstring(      L"іше"), 3, WordClass::Adjective }, // абсурдн~іше
			{ std::wstring(      L"іше"), 3, WordClass::Adverb }, // азартн~іше
			{ std::wstring( L"є"), 1, WordClass::Adjective }, // автодорожн~є
			{ std::wstring(L"ає"), 1 }, // вплива~є
			{ std::wstring(L"еє"), 2, WordClass::Adjective }, // безпосадков~еє багатеньк~еє близьк~еє adj
			{ std::wstring(L"іє"), 1 }, // бурі~є
			{ std::wstring(L"ує"), 1 }, // існу~є
			{ std::wstring(L"ює"), 1 }, // дорівню~є
			{ std::wstring(L"'є"), 1, WordClass::Noun }, // сім'~є

			{ std::wstring(     L"и"), 1, WordClass::Noun }, // ручк~и буркн~и береж~и реклам~и noun
			{ std::wstring(     L"и"), 1, WordClass::Numeral }, // шістдесят~и
			{ std::wstring(   L"али"), 2 }, // бува~ли
			{ std::wstring( L"ували"), 4 }, // актуалізу~вали існу~вали
			{ std::wstring( L"ювали"), 4 }, // базарю~вали (soft U, not J-U)
			{ std::wstring(   L"или"), 2 }, // би~ли
			{ std::wstring(   L"іли"), 2 }, // буркоті~ли
			//{ std::wstring(   L"їли"), 2 }, // благоустрої~ли
			{ std::wstring(  L"оїли"), 3 }, // благоустро~їли
			{ std::wstring(   L"оли"), 2 }, // боро~ли
			{ std::wstring(   L"ули"), 2 }, // бамкну~ли
			{ std::wstring(   L"ами"), 3, WordClass::Numeral }, // чотирмаст~ами
			{ std::wstring(   L"ими"), 2 }, // велики~ми
			{ std::wstring(   L"ими"), 3, WordClass::Numeral }, // багатомільярдн~ими
			//{ std::wstring( L"овими"), 5 }, // adjective
			{ std::wstring(   L"ами"), 3, WordClass::Noun }, // реклам~ами рук~ами ручк~ами фургон~ами
			{ std::wstring(   L"ями"), 3, WordClass::Noun }, // аграрі~ями обслуговуванн~ями
			//{ std::wstring(  L"оями"), 2 }, // и~ли noun
			{ std::wstring(  L"'ями"), 3, WordClass::Noun }, // сім'~ями
			{ std::wstring(   L"ати"), 2 }, // вплива~ти бра~ти
			{ std::wstring( L"ювати"), 4 }, // базарю~вати (soft U)
			{ std::wstring(   L"ити"), 2 }, // жи~ти аркани~ти би~ти
			{ std::wstring(   L"іти"), 2 }, // буркоті~ти
			//{ std::wstring(   L"їти"), 2 }, // благоустрої~ти
			{ std::wstring(  L"оїти"), 3 }, // благоустро~їти
			{ std::wstring(   L"оти"), 2 }, // боро~ти
			{ std::wstring(   L"ути"), 2 }, // бамкну~ти
			{ std::wstring( L"увати"), 4 }, // актуалізу~вати
			{ std::wstring(   L"ачи"), 2, WordClass::VerbalAdverb }, // бурлача~чи вантажа~чи
			{ std::wstring(   L"учи"), 2, WordClass::VerbalAdverb }, // беру~чи бережу~чи
			{ std::wstring(   L"ючи"), 3, WordClass::VerbalAdverb }, // бор~ючи ?
			//{ std::wstring(  L"аючи"), 3, WordClass::VerbalAdverb }, // незважа~ючи бува~ючи
			//{ std::wstring(  L"іючи"), 3, WordClass::VerbalAdverb }, // бурі~ючи байдужі~ючи
			//{ std::wstring(  L"уючи"), 3, WordClass::VerbalAdverb }, // у~ючи існу~ючи
			//{ std::wstring(  L"юючи"), 3, WordClass::VerbalAdverb }, // дорівню~ючи
			//{ std::wstring(  L"'ючи"), 3, WordClass::VerbalAdverb }, // б'~ючи
			{ std::wstring(   L"ячи"), 3, WordClass::VerbalAdverb }, // буркот~ячи виход~ячи буд~ячи
			{ std::wstring(  L"оячи"), 3, WordClass::VerbalAdverb }, // гно~ячи
			{ std::wstring(  L"авши"), 3, WordClass::VerbalAdverb }, // ма~вши зна~вши бра~вши
			{ std::wstring(  L"івши"), 3, WordClass::VerbalAdverb }, // буркоті~вши буботі~вши
			//{ std::wstring(  L"ївши"), 3, WordClass::VerbalAdverb }, // благоустрої~вши
			{ std::wstring( L"оївши"), 4, WordClass::VerbalAdverb }, // благоустро~ївши
			{ std::wstring(  L"увши"), 3, WordClass::VerbalAdverb }, // бамкну~вши бу~вши
			{ std::wstring(  L"ивши"), 3, WordClass::VerbalAdverb }, // безчести~вши би~вши
			{ std::wstring(  L"овши"), 3, WordClass::VerbalAdverb }, // боро~вши
			{ std::wstring(L"ювавши"), 5, WordClass::VerbalAdverb }, // ідорівню~вавши асоцію~вавши
			{ std::wstring(L"увавши"), 5, WordClass::VerbalAdverb }, // існу~вавши актуалізу~вавши бу~вавши

			{ std::wstring(   L"і"), 1, WordClass::Noun }, // так~і україн~і
			{ std::wstring(   L"і"), 1, WordClass::Adjective }, // ближч~і близьк~і
			{ std::wstring(   L"і"), 1, WordClass::Numeral }, // багатомільярдн~і
			{ std::wstring(L"оєві"), 3, WordClass::Noun }, // одностро~єві noun
			{ std::wstring( L"еві"), 3, WordClass::Noun }, // княз~еві
			{ std::wstring( L"єві"), 3, WordClass::Noun }, // аграрі~єві
			{ std::wstring( L"ові"), 3, WordClass::Noun }, // фургон~ові noun
			{ std::wstring( L"іші"), 3, WordClass::Adjective }, // абсурдн~іші adj

			{ std::wstring( L"ї"), 1, WordClass::Adjective }, // білоши~ї
			{ std::wstring( L"ї"), 1, WordClass::Noun }, // одностро~ї
			{ std::wstring(L"еї"), 1, WordClass::Noun }, // музе~ї
			{ std::wstring(L"єї"), 2, WordClass::Pronoun }, // ці~єї
			{ std::wstring(L"иї"), 1, WordClass::Noun }, // коломи~ї
			{ std::wstring(L"ії"), 2, WordClass::Adjective }, // багатеньк~ії безпосадков~ії adj
			{ std::wstring(L"ії"), 1, WordClass::Noun }, // полонiзацi~ї телепортаці~ї ліні~ї
			{ std::wstring(L"ої"), 2, WordClass::Adjective }, // заможн~ої
			{ std::wstring(L"ої"), 1, WordClass::Noun }, // одностро~ї
			{ std::wstring(L"ої"), 1, WordClass::Numeral }, // багатомільярдн~ої
			{ std::wstring(L"уї"), 1, WordClass::Noun }, // буржу~ї
			{ std::wstring(L"юї"), 1 }, // брю~ї
			{ std::wstring(L"яї"), 1, WordClass::Noun }, // хазя~ї

			{ std::wstring(      L"ай"), 1 }, // назива~й вплива~й
			{ std::wstring(      L"ей"), 2, WordClass::Noun }, // сім~ей
			{ std::wstring(      L"ий"), 2, WordClass::Adjective }, // тепл~ий багатеньк~ий adj 
			{ std::wstring(      L"ий"), 2, WordClass::Numeral }, // багатомільярдн~ий
			{ std::wstring(      L"ій"), 2, WordClass::Adjective }, // українськ~ій останн~ій автодорожн~ій adj
			{ std::wstring(      L"ій"), 1, WordClass::Noun }, // однострі~й організаці~й noun
			{ std::wstring(      L"ій"), 1, WordClass::Numeral }, // багатомільярдн~ій
			{ std::wstring(      L"ій"), 2, WordClass::Pronoun }, // їхн~ій
			{ std::wstring(      L"їй"), 2, WordClass::Adjective }, // безкра~їй
			{ std::wstring(      L"ой"), 1 }, // благоустро~й
			{ std::wstring(      L"уй"), 1 }, // існу~й
			{ std::wstring(      L"юй"), 1 }, // дорівню~й
			{ std::wstring(    L"ілий"), 3, WordClass::Participle }, // бурі~лий байдужі~лий ИЙ
			{ std::wstring(    L"аний"), 3, WordClass::Participle }, // а~ний бра~ний ИЙ
			{ std::wstring(  L"ований"), 5, WordClass::Participle }, // актуалізо~ваний анігільо~ваний асоційо~ваний
			{ std::wstring(  L"уваний"), 5, WordClass::Participle }, // арештову~ваний
			{ std::wstring(  L"юваний"), 5, WordClass::Participle }, // підозрю~ваний
			{ std::wstring(    L"ений"), 3, WordClass::Participle }, // безчеще~ний береже~ний вантаже~ний ИЙ
			{ std::wstring(    L"єний"), 4, WordClass::Adjective }, // благоустро~єний гно~єний заспоко~єний ИЙ
			{ std::wstring(   L"оєний"), 4, WordClass::Adjective }, // благоустро~єний гно~єний заспоко~єний
			{ std::wstring(    L"итий"), 3, WordClass::Participle }, // би~тий
			{ std::wstring(    L"отий"), 3, WordClass::Participle }, // боро~тий
			{ std::wstring(    L"утий"), 3, WordClass::Participle }, // бовтну~тий
			{ std::wstring(    L"ачий"), 3, WordClass::Participle }, // бурлача~чий вантажа~чий
			{ std::wstring(    L"учий"), 3, WordClass::Participle}, // беру~чий бамкну~чий бережу~чий буду~чий
			{ std::wstring(    L"ючий"), 4, WordClass::Participle }, // compound бор~ючий
			//{ std::wstring(   L"аючий"), 4, WordClass::Participle }, // вплива~ючий
			//{ std::wstring(   L"іючий"), 4, WordClass::Participle }, // бурі~ючий байдужі~ючий
			//{ std::wstring(   L"уючий"), 4 , WordClass::Participle}, // існу~ючий
			//{ std::wstring(   L"юючий"), 4, WordClass::Participle }, // дорівню~ючий
			//{ std::wstring(   L"'ючий"), 4, WordClass::Participle }, // б'~ючий
			{ std::wstring(    L"ячий"), 4, WordClass::Participle }, // буркот~ячий буд~ячий
			{ std::wstring(   L"оячий"), 4, WordClass::Participle }, // гно~ячий
			{ std::wstring(     L"ший"), 3, WordClass::Adjective }, // багат~ший
			{ std::wstring(   L"авший"), 4, WordClass::Participle }, // бра~вший
			{ std::wstring( L"увавший"), 6, WordClass::Participle }, // існу~вавший актуалізу~вавший бу~вавший
			{ std::wstring( L"ювавший"), 6, WordClass::Participle }, // базарю~вавший асоцію~вавший
			{ std::wstring(   L"ивший"), 4, WordClass::Participle }, // безчести~вший би~вший
			{ std::wstring(   L"івший"), 4, WordClass::Participle }, // буркоті~вший буботі~вший
			//{ std::wstring(   L"ївший"), 4 }, // благоустрої~вший
			{ std::wstring(  L"оївший"), 5, WordClass::Participle }, // благоустро~ївший
			{ std::wstring(   L"овший"), 4, WordClass::Participle }, // боро~вший
			{ std::wstring(   L"увший"), 4, WordClass::Participle }, // бамкну~вший бу~вший
			{ std::wstring(    L"іший"), 4, WordClass::Adjective }, // абсурдн~іший

			{ std::wstring(      L"ам"), 2, WordClass::Noun }, // фургон~ам
			{ std::wstring(      L"ам"), 2, WordClass::Numeral }, // чотирьомст~ам
			{ std::wstring(      L"ем"), 1 }, // бер~ем бамкн~ем
			{ std::wstring(      L"ем"), 2, WordClass::Noun }, // саботаж~ем ERROR: бурозем~ анахтем~(анахтема)
			{ std::wstring(    L"имем"), 3 }, // брести~мем
			{ std::wstring(  L"атимем"), 5 }, // бра~тимем
			{ std::wstring(  L"отимем"), 5 }, // боро~тимем
			{ std::wstring(L"уватимем"), 7 }, // існу~ва~тимем актуалізу~ватимем
			{ std::wstring(L"юватимем"), 7 }, // базарю~ва~тимем
			{ std::wstring(  L"итимем"), 5 }, // роби~тимем би~тимем
			{ std::wstring(  L"ітимем"), 5 }, // терпі~тимем ?
			//{ std::wstring( L"їтимем"), 5 }, // гної~тимем
			{ std::wstring( L"оїтимем"), 6 }, // гно~їтимем
			{ std::wstring(  L"утимем"), 5 }, // блякну~тимем
			{ std::wstring(  L"ятимем"), 5 }, // розмовл~ятимем
			{ std::wstring(      L"єм"), 2, WordClass::Noun }, // composite
			{ std::wstring(      L"єм"), 2 }, // composite
			//{ std::wstring(     L"аєм"), 2 }, // зна~єм
			//{ std::wstring(     L"еєм"), 2 }, // промете~єм noun
			//{ std::wstring(     L"ієм"), 2 }, // критері~єм noun
			//{ std::wstring(     L"оєм"), 2 }, // одностро~єм
			//{ std::wstring(     L"уєм"), 2 }, // чу~єм
			//{ std::wstring(     L"юєм"), 2 }, // базарю~єм
			//{ std::wstring(     L"'єм"), 2 }, // б'~єм
			{ std::wstring(      L"им"), 1 }, // буркоти~м
			{ std::wstring(      L"им"), 2, WordClass::Numeral }, // багатомільярдн~им
			{ std::wstring(      L"ім"), 1 }, // бамкні~м бері~м бережі~м
			{ std::wstring(      L"ім"), 2, WordClass::Numeral }, // багатомільярдн~ім
			//{ std::wstring(      L"їм"), 1 }, // благоустрої~м
			{ std::wstring(     L"оїм"), 2 }, // благоустро~їм
			{ std::wstring(      L"ом"), 2, WordClass::Noun }, // фургон~ом
			{ std::wstring(      L"ом"), 2, WordClass::Numeral }, // дв~ом одинадцять~ом
			{ std::wstring(      L"ям"), 2, WordClass::Noun }, // compound люд~ям обслуговуванн~ям
			//{ std::wstring(     L"оям"), 2, WordClass::Noun }, // одностро~ям
			//{ std::wstring(     L"'ям"), 2, WordClass::Noun }, // сім'~ям

			{ std::wstring(        L"о"), 1 }, // ручк~о
			{ std::wstring(        L"о"), 1, WordClass::Adverb }, // азартн~о adverb
			{ std::wstring(        L"о"), 1, WordClass::Numeral }, // ст~о
			{ std::wstring(      L"ого"), 3, WordClass::Numeral }, // багатомільярдн~ого
			{ std::wstring(     L"ього"), 3, WordClass::Pronoun }, // їхнь~ого
			{ std::wstring(      L"ало"), 2 }, // бракува~ло
			{ std::wstring(    L"увало"), 4 }, // існу~вало актуалізу~вало
			{ std::wstring(    L"ювало"), 4 }, // базарю~вало (soft U)
			{ std::wstring(      L"ило"), 2 }, // жи~ло би~ло
			{ std::wstring(      L"іло"), 2 }, // буркоті~ло
			//{ std::wstring(      L"їло"), 2 }, // благоустрої~ло
			{ std::wstring(     L"оїло"), 3 }, // благоустро~їло
			{ std::wstring(      L"оло"), 2 }, // боро~ло
			{ std::wstring(      L"уло"), 2 }, // бамкну~ло
			//{ std::wstring(       L"мо"), 2 }, // буркоть~мо безчесть~мо
			{ std::wstring(      L"емо"), 2 }, // бере~мо бамкне~мо
			{ std::wstring(      L"имо"), 2 }, // буркоти~мо
			{ std::wstring(  L"атимемо"), 6 }, // існу~ва~тимемо ма~тимемо
			{ std::wstring(  L"итимемо"), 6 }, // жи~тимемо би~тимемо
			{ std::wstring(  L"ітимемо"), 6 }, // буркоті~тимемо
			//{ std::wstring(  L"їтимемо"), 6 }, // гної~тимемо
			{ std::wstring( L"оїтимемо"), 7 }, // гно~їтимемо
			{ std::wstring(  L"отимемо"), 6 }, // боро~тимемо
			{ std::wstring(  L"утимемо"), 6 }, // блякну~тимемо
			{ std::wstring(L"уватимемо"), 8 }, // актуалізу~ватимемо
			{ std::wstring(L"юватимемо"), 8 }, // дорівню~ватимемо
			{ std::wstring(     L"аємо"), 3 }, // зна~ємо
			{ std::wstring(     L"іємо"), 3 }, // бурі~ємо
			{ std::wstring(     L"уємо"), 3 }, // існу~ємо
			{ std::wstring(     L"юємо"), 3 }, // дорівню~ємо
			{ std::wstring(     L"'ємо"), 3 }, // б'~ємо
			{ std::wstring(      L"імо"), 2 }, // бері~мо бамкні~мо бережі~мо
			//{ std::wstring(      L"їмо"), 2 }, // благоустрої~мо
			{ std::wstring(     L"оїмо"), 3 }, // благоустро~їмо
			//{ std::wstring(      L"ймо"), 3 }, // NOT клей*ймо
			{ std::wstring(     L"аймо"), 3 }, // назива~ймо руша~ймо
			//{ std::wstring(     L"еймо"), 3 }, // no words
			{ std::wstring(     L"иймо"), 3 }, // бурі~ймо би~ймо
			{ std::wstring(     L"іймо"), 3 }, // бурі~ймо
			{ std::wstring(     L"оймо"), 3 }, // благоустро~ймо
			{ std::wstring(     L"уймо"), 3 }, // існу~ймо актуалізу~ймо
			{ std::wstring(     L"юймо"), 3 }, // дорівню~ймо
			{ std::wstring(     L"яймо"), 3 }, // порівня~ймо
			{ std::wstring(      L"ьмо"), 2 }, // буркоть~мо безчесть~мо
			{ std::wstring(      L"еро"), 3, WordClass::Numeral }, // дев'ят~еро
			
			{ std::wstring(      L"у"), 1, WordClass::Noun }, // noun, ручк~у бер~у буркоч~у безчещ~у береж~у
			{ std::wstring(      L"у"), 1, WordClass::Numeral }, // багатомільярдн~у
			//{ std::wstring(     L"му"), 2 }, // not usable (фор*м~у)
			//{ std::wstring(    L"аму"), 2 }, // not usable (са*м~у)
			{ std::wstring(    L"єму"), 2, WordClass::Pronoun }, // своє~му
			{ std::wstring(    L"иму"), 2 }, // берег*ти~му
			//{ std::wstring(   L"тиму"), 4 }, // берег*тиму; г*тиму is too rare to use
			{ std::wstring(  L"атиму"), 4 }, // бра~тиму
			{ std::wstring(  L"итиму"), 4 }, // безчести~тиму би~тиму
			{ std::wstring(  L"ітиму"), 4 }, // буркоті~тиму
			//{ std::wstring(  L"їтиму"), 4 }, // гної~тиму
			{ std::wstring( L"оїтиму"), 5 }, // гно~їтиму
			{ std::wstring(  L"отиму"), 4 }, // боро~тиму
			{ std::wstring(  L"стиму"), 4 }, // брес~тиму
			{ std::wstring(  L"утиму"), 4 }, // блякну~тиму
			{ std::wstring(L"уватиму"), 6 }, // існу~ватиму актуалізу~ватиму
			{ std::wstring(L"юватиму"), 6 }, // базарю~ватиму
			{ std::wstring(    L"ому"), 3, WordClass::Adjective }, // бюджетн~ому adj
			{ std::wstring(    L"ому"), 3, WordClass::Numeral }, // багатомільярдн~ому
			//{ std::wstring(    L"уму"), 2 }, // not usable (розу*м~у)

			{ std::wstring(  L"ах"), 2, WordClass::Noun }, // ручк~ах фургон~ах
			{ std::wstring(  L"ах"), 2, WordClass::Numeral }, // чотирьохст~ах
			{ std::wstring(  L"их"), 2, WordClass::Adjective }, // бюджетн~их
			{ std::wstring(  L"их"), 2, WordClass::Numeral }, // багатомільярдн~их
			{ std::wstring(  L"ох"), 2, WordClass::Numeral }, // дев'ять~ох дв~ох
			{ std::wstring(  L"ях"), 2, WordClass::Noun }, // compound
			//{ std::wstring( L"оях"), 2, WordClass::Noun }, // одностро~ях
			//{ std::wstring(  L"ях"), 2, WordClass::Noun }, // обслуговуванн~ях
			//{ std::wstring( L"'ях"), 2, WordClass::Noun }, // сім'~ях

			{ std::wstring(      L"еш"), 2 }, // бер~еш бамкн~еш
			{ std::wstring(    L"имеш"), 3 }, // брести~меш
			{ std::wstring(  L"атимеш"), 5 }, // бра~тимеш назива~тимеш
			{ std::wstring(  L"итимеш"), 5 }, // безчести~тимеш би~тимеш
			{ std::wstring(  L"ітимеш"), 5 }, // буркоті~тимеш
			//{ std::wstring(  L"їтимеш"), 5 }, // гної~тимеш
			{ std::wstring( L"оїтимеш"), 6 }, // гно~їтимеш
			{ std::wstring(  L"отимеш"), 5 }, // боро~тимеш
			{ std::wstring(  L"утимеш"), 5 }, // блякну~тимеш
			//{ std::wstring(L"иватимеш"), 7 }, // no words with such suffix
			{ std::wstring(L"уватимеш"), 7 }, // актуалізу~ватимеш існу~ватимеш
			{ std::wstring(L"юватимеш"), 7 }, // базарю~ватимеш (soft U)
			{ std::wstring(     L"аєш"), 2 }, // а~єш ма~єш
			{ std::wstring(     L"ієш"), 2 }, // бурі~єш
			{ std::wstring(     L"уєш"), 2 }, // існу~єш
			{ std::wstring(     L"юєш"), 2 }, // дорівню~єш
			{ std::wstring(     L"'єш"), 2 }, // б'~єш
			{ std::wstring(      L"иш"), 1 }, // буркот~иш
			{ std::wstring(      L"іш"), 2, WordClass::Adverb }, // азартн~іш
			//{ std::wstring(      L"їш"), 1 }, // благоустрої~ш
			{ std::wstring(     L"оїш"), 2 }, // благоустро~їш

			{ std::wstring(      L"алась"), 4 }, // назива~лась
			{ std::wstring(    L"увалась"), 6 }, // актуалізу~валась
			{ std::wstring(    L"ювалась"), 6 }, // анігілю~валась
			{ std::wstring(      L"илась"), 4 }, // брижи~лась би~лась
			//{ std::wstring(      L"їлась"), 4 }, // благоустрої~лась
			{ std::wstring(     L"оїлась"), 5 }, // благоустро~їлась
			{ std::wstring(      L"олась"), 4 }, // боро~лась
			{ std::wstring(      L"улась"), 4 }, // бехну~лємосяась
			{ std::wstring(     L"оялась"), 5 }, // бо~ялась
			{ std::wstring(       L"авсь"), 3 }, // назива~всь
			{ std::wstring(     L"увавсь"), 5 }, // актуалізу~вавсь
			{ std::wstring(     L"ювавсь"), 5 }, // анігілю~вавсь
			{ std::wstring(       L"ивсь"), 3 }, // безчести~всь би~всь
			//{ std::wstring(       L"ївсь"), 3 }, // благоустрої~всь
			{ std::wstring(      L"оївсь"), 4 }, // благоустро~ївсь
			{ std::wstring(       L"овсь"), 3 }, // боро~всь
			{ std::wstring(       L"увсь"), 3 }, // бехну~всь
			{ std::wstring(       L"явсь"), 4 }, // бо~явсь
			// { std::wstring(        L"есь"), 2 }, // no words
			{ std::wstring(   L"тиметесь"), 8 }, // берег~тиметесь
			{ std::wstring(  L"атиметесь"), 8 }, // назива~тиметесь
			//{ std::wstring(  L"їтиметесь"), 7 }, // гної~тиметесь
			{ std::wstring( L"оїтиметесь"), 9 }, // гно~їтиметесь
			{ std::wstring(  L"отиметесь"), 8 }, // боро~тиметесь
			{ std::wstring(L"уватиметесь"), 10 }, // актуалізу~ватиметесь
			{ std::wstring(L"юватиметесь"), 10 }, // анігілю~ватиметесь
			{ std::wstring(  L"итиметесь"), 8 }, // брижи~тиметесь би~тиметесь
			{ std::wstring(  L"ятиметесь"), 9 }, // бо~ятиметесь
			{ std::wstring(      L"етесь"), 4 }, // бере~тесь
			{ std::wstring(      L"єтесь"), 5 }, // б'~єтесь
			{ std::wstring(     L"уєтесь"), 5 }, // актуалізу~єтесь
			{ std::wstring(      L"итесь"), 4 }, // безчести~тесь
			{ std::wstring(      L"ітесь"), 4 }, // бері~тесь брижі~тесь диві~тесь
			//{ std::wstring(      L"їтесь"), 4 }, // благоустрої~тесь
			{ std::wstring(     L"оїтесь"), 5 }, // благоустро~їтесь бо~їтесь
			{ std::wstring(     L"айтесь"), 5 }, // назива~йтесь
			{ std::wstring(     L"ийтесь"), 5 }, // би~йтесь
			{ std::wstring(     L"ійтесь"), 5 }, // бі~йтесь
			{ std::wstring(     L"ойтесь"), 5 }, // благоустро~йтесь
			{ std::wstring(     L"уйтесь"), 5 }, // актуалізу~йтесь
			{ std::wstring(     L"юйтесь"), 5 }, // анігілю~йтесь
			{ std::wstring(     L"аєтесь"), 5 }, // а~єтесь назива~єтесь
			{ std::wstring(     L"юєтесь"), 5 }, // анігілю~єтесь
			{ std::wstring(      L"ьтесь"), 4 }, // багрянь~тесь
			{ std::wstring(        L"ись"), 2, WordClass::Verb }, // бери~сь
			{ std::wstring(        L"ись"), 2, WordClass::VerbalAdverb }, // берігши~сь
			{ std::wstring(      L"ались"), 4 }, // назива~лись
			{ std::wstring(    L"ювались"), 6 }, // анігілю~вались
			{ std::wstring(      L"ились"), 4 }, // брижи~лись би~лись
			//{ std::wstring(      L"їлись"), 4 }, // благоустрої~лись
			{ std::wstring(     L"оїлись"), 5 }, // благоустро~їлись
			{ std::wstring(      L"олись"), 4 }, // боро~лись
			{ std::wstring(      L"улись"), 4 }, // бехну~лись
			{ std::wstring(    L"увались"), 6 }, // актуалізу~вались
			{ std::wstring(      L"ялись"), 5 }, // бо~ялись
			{ std::wstring(      L"атись"), 4 }, // бра~тись
			{ std::wstring(    L"уватись"), 6 }, // актуалізу~ватись
			{ std::wstring(    L"юватись"), 6 }, // анігілю~ватись
			{ std::wstring(      L"итись"), 4 }, // брижи~тись би~тись
			//{ std::wstring(      L"їтись"), 4 }, // благоустрої~тись
			{ std::wstring(     L"оїтись"), 5 }, // благоустро~їтись
			{ std::wstring(      L"отись"), 4 }, // боро~тись
			{ std::wstring(      L"утись"), 4 }, // бехну~тись
			{ std::wstring(      L"ятись"), 5 }, // бо~ятись
			{ std::wstring(      L"ачись"), 4, WordClass::VerbalAdverb }, // брижа~чись вантажа~чись
			{ std::wstring(      L"учись"), 4, WordClass::VerbalAdverb }, // беру~чись
			{ std::wstring(      L"ючись"), 5, WordClass::VerbalAdverb }, // compound
			//{ std::wstring(     L"аючись"), 5, WordClass::VerbalAdverb }, // намага~ючись
			//{ std::wstring(     L"уючись"), 5, WordClass::VerbalAdverb }, // диву~ючись
			//{ std::wstring(     L"юючись"), 5, WordClass::VerbalAdverb }, // анігілю~ючись
			//{ std::wstring(      L"ючись"), 5, WordClass::VerbalAdverb }, // борю~чись
			//{ std::wstring(     L"'ючись"), 5, WordClass::VerbalAdverb }, // б'~ючись
			{ std::wstring(      L"ячись"), 5, WordClass::VerbalAdverb }, // бо~ячись гно~ячись дивл~ячись буд~ячись
			{ std::wstring(     L"оячись"), 5, WordClass::VerbalAdverb }, // гно~ячись бо~ячись

			{ std::wstring(     L"авшись"), 5, WordClass::VerbalAdverb }, // назива~вшись бра~вшись
			{ std::wstring(     L"овшись"), 5, WordClass::VerbalAdverb }, // боро~вшись
			{ std::wstring(   L"ювавшись"), 7, WordClass::VerbalAdverb }, // анігілю~вавшись асоцію~вавшись
			{ std::wstring(   L"увавшись"), 7, WordClass::VerbalAdverb }, // актуалізу~вавшись
			{ std::wstring(     L"ившись"), 5, WordClass::VerbalAdverb }, // брижи~вшись би~вшись
			//{ std::wstring(     L"ївшись"), 5, WordClass::VerbalAdverb }, // благоустрої~вшись
			{ std::wstring(    L"оївшись"), 6, WordClass::VerbalAdverb }, // благоустро~ївшись
			{ std::wstring(     L"увшись"), 5, WordClass::VerbalAdverb }, // бехну~вшись зверну~вшись
			{ std::wstring(     L"явшись"), 5, WordClass::VerbalAdverb }, // взя~вшись
			{ std::wstring(    L"оявшись"), 6, WordClass::VerbalAdverb }, // бо~явшись
			{ std::wstring(       L"айсь"), 3 }, // зна~йсь назива~йсь
			{ std::wstring(       L"ийсь"), 3 }, // бі~йсь би~йсь
			{ std::wstring(       L"ійсь"), 3 }, // бі~йсь
			{ std::wstring(       L"ойсь"), 3 }, // благоустро~йсь
			{ std::wstring(       L"уйсь"), 3 }, // актуалізу~йсь
			{ std::wstring(       L"юйсь"), 3 }, // анігілю~йсь
			//{ std::wstring(        L"ось"), 2 }, // no actual words (all are LOS or MOS) кого~сь ??
			{ std::wstring(      L"алось"), 5 }, // бра~лось назива~лось ста~лось
			{ std::wstring(    L"ювалось"), 6 }, // анігілю~валось
			{ std::wstring(      L"илось"), 4 }, // брижи~лось би~лось
			//{ std::wstring(      L"їлось"), 4 }, // благоустрої~лось
			{ std::wstring(     L"оїлось"), 5 }, // благоустро~їлось
			{ std::wstring(      L"олось"), 4 }, // боро~лось
			{ std::wstring(      L"улось"), 4 }, // бехну-лось
			{ std::wstring(    L"увалось"), 6 }, // актуалізу~валось
			{ std::wstring(      L"ялось"), 5 }, // бо~ялось
			// { std::wstring(       L"мось"), 4 }, // 
			{ std::wstring(      L"емось"), 4 }, // береже~мось бере~мось
			{ std::wstring(    L"имемось"), 6 }, // берегти~мемось (we can't split берег*тимемось)
			{ std::wstring(  L"атимемось"), 8 }, // бра~тимемось нази~ва~тимемось
			{ std::wstring(  L"итимемось"), 8 }, // брижи~тимемось би~тимемось
			//{ std::wstring(  L"їтимемось"), 8 }, // гної~тимемось
			{ std::wstring(  L"оїтимемось"), 9 }, // гно~їтимемось
			{ std::wstring(  L"отимемось"), 8 }, // боро~тимемось
			{ std::wstring(L"уватимемось"), 10 }, // актуалізу~ватимемось
			{ std::wstring(L"юватимемось"), 10 }, // анігілю~ватимемось
			{ std::wstring(  L"ятимемось"), 9 }, // бо~ятимемось
			{ std::wstring(      L"ємось"), 5 }, // б'~ємось
			//{ std::wstring(     L"аємось"), 5 }, // сподіва~ємось
			//{ std::wstring(     L"иємось"), 5 }, // поши~ємось
			//{ std::wstring(     L"іємось"), 5 }, // наді~ємось
			//{ std::wstring(     L"оємось"), 5 }, // побою~ємось
			//{ std::wstring(     L"уємось"), 5 }, // користу~ємось актуалізу~ємось
			//{ std::wstring(     L"юємось"), 5 }, // висловлю~ємось анігілю~ємось
			//{ std::wstring(     L"яємось"), 5 }, // спиня~ємось
			//{ std::wstring(     L"'ємось"), 5 }, // відіб'~ємось б'~ємось
			{ std::wstring(      L"имось"), 4 }, // безчести~мось
			//{ std::wstring(      L"їмось"), 4 }, // бої~мось благоустрої~мось
			{ std::wstring(      L"імось"), 4 }, // бережі~мось бері~мось диві~мось
			{ std::wstring(      L"їмось"), 5 }, // compound
			//{ std::wstring(     L"иїмось"), 5 }, // чи~їмось
			//{ std::wstring(     L"оїмось"), 5 }, // бо~їмось благоустро~їмось
			{ std::wstring(      L"ймось"), 5 }, // compound
			//{ std::wstring(     L"аймось"), 5 }, // назива~ймось
			//{ std::wstring(     L"иймось"), 5 }, // би~ймось
			//{ std::wstring(     L"іймось"), 5 }, // бі~ймось
			//{ std::wstring(     L"оймось"), 5 }, // благоустро~ймось
			//{ std::wstring(     L"уймось"), 5 }, // актуалізу~ймось
			//{ std::wstring(     L"юймось"), 5 }, // анігілю~ймось
			{ std::wstring(      L"ьмось"), 4 }, // безчесть~мось
			{ std::wstring(        L"усь"), 2 }, // беру~сь безчещу~сь
			{ std::wstring(      L"имусь"), 4 }, // берегти~мусь
			{ std::wstring(    L"атимусь"), 6 }, // назива~тимусь
			{ std::wstring(    L"їтимусь"), 7 }, // compound
			//{ std::wstring(   L"оїтимусь"), 7 }, // гно~їтимусь
			{ std::wstring(    L"отимусь"), 6 }, // боро~тимусь
			{ std::wstring(  L"уватимусь"), 8 }, // актуалізу~ватимусь
			{ std::wstring(  L"юватимусь"), 8 }, // анігілю~ватимусь
			{ std::wstring(    L"итимусь"), 6 }, // брижи~тимусь би~тимусь
			{ std::wstring(    L"ятимусь"), 7 }, // бо~ятимусь

			{ std::wstring(        L"юсь"), 3 }, // дивл~юсь аркан~юсь жур~юсь
			{ std::wstring(       L"аюсь"), 3 }, // назива~юсь
			{ std::wstring(       L"іюсь"), 3 }, // розумі~юсь
			{ std::wstring(       L"июсь"), 3 }, // чи~юсь
			{ std::wstring(       L"оюсь"), 3 }, // бо~юсь(?or zero) благоустро~юсь ERROR: гно~юсь (need to keep letter 'o')
			{ std::wstring(       L"уюсь"), 3 }, // актуалізу~юсь
			{ std::wstring(       L"єюсь"), 4 }, // чи~єюсь
			{ std::wstring(       L"ююсь"), 3 }, // анігілю~юсь хвилю~юсь
			{ std::wstring(       L"яюсь"), 3 }, // помиля~юсь
			{ std::wstring(       L"'юсь"), 3 }, // б'~юсь

			{ std::wstring(       L"ать"), 2 }, // бра~ть бурлача~ть
			{ std::wstring(     L"увать"), 4 }, // існу~вать
			{ std::wstring(     L"ювать"), 4 }, // дорівню~вать
			{ std::wstring(       L"ить"), 2 }, // безчести~ть би~ть
			{ std::wstring(       L"іть"), 3 }, // бер~іть буркот~іть бамкн~іть
			//{ std::wstring(       L"їть"), 3 }, // благоустрої~ть
			{ std::wstring(      L"оїть"), 3 }, // благоустро~їть
			{ std::wstring(       L"оть"), 3 }, // бор~оть пол~оть кол~оть кіг~оть
			{ std::wstring(       L"уть"), 3 }, // бер~уть бамкн~уть
			{ std::wstring(     L"имуть"), 4 }, // брести~муть
			{ std::wstring(   L"атимуть"), 6 }, // а~тимуть існува~тимуть
			{ std::wstring(   L"итимуть"), 6 }, // жи~тимуть би~тимуть
			{ std::wstring(   L"ітимуть"), 6 }, // буркоті~тимуть
			//{ std::wstring(   L"їтимуть"), 6 }, // гної~тимуть
			{ std::wstring(  L"оїтимуть"), 7 }, // гно~їтимуть
			{ std::wstring(   L"отимуть"), 6 }, // боро~тимуть
			{ std::wstring(   L"утимуть"), 6 }, // блякну~тимуть
			{ std::wstring( L"уватимуть"), 8 }, // актуалізу~ватимуть
			{ std::wstring( L"юватимуть"), 8 }, // дорівню~ватимуть
			{ std::wstring(       L"ють"), 3 }, // бор~ють
			{ std::wstring(      L"ають"), 3 }, // вважа~ють вплива~ють
			{ std::wstring(      L"іють"), 3 }, // бурі~ють
			{ std::wstring(      L"ують"), 3 }, // існу~ють
			{ std::wstring(      L"юють"), 3 }, // дорівню~ють
			{ std::wstring(      L"'ють"), 3 }, // дорівню~ють б'~ють
			{ std::wstring(       L"ять"), 3 }, // буркот~ять гно~ять
			{ std::wstring(      L"оять"), 3 }, // благоустро~ять

			{ std::wstring( L"ю"), 1 }, // аркан~ю
			{ std::wstring( L"ю"), 1 , WordClass::Noun}, // беззмінніст~ю
			{ std::wstring(L"аю"), 1 }, // назива~ю
			{ std::wstring(L"єю"), 2, WordClass::Noun }, // сім'~єю
			{ std::wstring(L"ію"), 1 }, // бурі~ю
			{ std::wstring(L"ою"), 1 }, // гно~ю
			{ std::wstring(L"ою"), 2, WordClass::Noun }, // благоустр~ою змін~ою
			{ std::wstring(L"ою"), 2, WordClass::Numeral }, // багатомільярдн~ою
			{ std::wstring(L"ую"), 1 }, // існу~ю актуалізу~ю
			{ std::wstring(L"юю"), 1 }, // доріню~ю
			{ std::wstring(L"'ю"), 1, WordClass::Noun }, // сім'~ю

			{ std::wstring(          L"я"), 1, WordClass::Adjective }, // дорожн~я
			{ std::wstring(          L"я"), 1, WordClass::Noun }, // обслуговуванн~я
			{ std::wstring(         L"ая"), 2, WordClass::Adjective }, // безпосадков~ая близьк~ая adj
			{ std::wstring(         L"яя"), 2, WordClass::Adjective }, // автодорожн~яя
			{ std::wstring(         L"'я"), 1, WordClass::Noun }, // сім'~я
			// { std::wstring(         L"ся"), 2 }, // can't detach unvoiced 'Sya'
			//{ std::wstring(        L"ася"), 2 }, // no words
			{ std::wstring(       L"лася"), 4 }, // берег~лася
			{ std::wstring(      L"алася"), 4 }, // назива~лася
			{ std::wstring(      L"олася"), 4 }, // боро~лася
			{ std::wstring(    L"увалася"), 6 }, // актуалізу~валася
			{ std::wstring(    L"ювалася"), 6 }, // анігілю~валася
			{ std::wstring(      L"илася"), 4 }, // брижи~лася би~лася
			//{ std::wstring(      L"їлася"), 4 }, // благоустрої~лася
			{ std::wstring(     L"оїлася"), 5 }, // благоустро~їлася
			{ std::wstring(      L"улася"), 4 }, // бехну~лася
			{ std::wstring(      L"ялася"), 5 }, // бо~ялася
			{ std::wstring(       L"ався"), 3 }, // назива~вся бра~вся
			{ std::wstring(     L"увався"), 5 }, // актуалізу~вався
			{ std::wstring(     L"ювався"), 5 }, // анігілю~вався
			{ std::wstring(       L"ився"), 3 }, // брижи~вся би~вся
			//{ std::wstring(       L"ївся"), 3 }, // благоустрої~вся
			{ std::wstring(      L"оївся"), 4 }, // благоустро~ївся
			{ std::wstring(       L"овся"), 3 }, // боро~вся
			{ std::wstring(       L"увся"), 3 }, // бехну~вся
			{ std::wstring(       L"явся"), 3 }, // боя~вся
			//{ std::wstring(       L"ігся"), 3 }, // берігс~я (бер is too common)
			{ std::wstring(      L"етеся"), 4 }, // бере~теся береже~теся
			{ std::wstring(    L"иметеся"), 6 }, // берегти~метеся
			{ std::wstring(  L"атиметеся"), 8 }, // назива~тиметеся
			{ std::wstring(L"уватиметеся"), 10 }, // актуалізу~ватиметеся
			{ std::wstring(L"юватиметеся"), 10 }, // анігілю~ватиметеся
			{ std::wstring(  L"итиметеся"), 8 }, // брижи~тиметеся би~тиметеся
			//{ std::wstring(  L"їтиметеся"), 8 }, // гної~тиметеся
			{ std::wstring( L"оїтиметеся"), 9 }, // гно~їтиметеся
			{ std::wstring(  L"отиметеся"), 8 }, // боро~тиметеся
			{ std::wstring(  L"ятиметеся"), 9 }, // бо~ятиметеся
			{ std::wstring(      L"єтеся"), 5 }, // compound
			//{ std::wstring(     L"аєтеся"), 5 }, // забира~єтеся назива~єтеся
			//{ std::wstring(     L"иєтеся"), 5 }, // вши~єтеся
			//{ std::wstring(     L"ієтеся"), 5 }, // розумі~єтеся
			//{ std::wstring(     L"уєтеся"), 5 }, // актуалізу~єтеся
			//{ std::wstring(     L"юєтеся"), 5 }, // анігілю~єтеся
			//{ std::wstring(     L"яєтеся"), 5 }, // помиля~єтеся
			//{ std::wstring(     L"'єтеся"), 5 }, // б'~єтеся
			{ std::wstring(      L"итеся"), 4 }, // брижи~теся
			{ std::wstring(      L"ітеся"), 4 }, // бері~теся диві~теся
			//{ std::wstring(      L"їтеся"), 4 }, // бої~теся благоустрої~теся
			{ std::wstring(     L"оїтеся"), 5 }, // бо~їтеся благоустро~їтеся
			{ std::wstring(      L"йтеся"), 5 }, // compound
			//{ std::wstring(     L"айтеся"), 5 }, // назива~йтеся
			//{ std::wstring(     L"ийтеся"), 5 }, // би~йтеся
			//{ std::wstring(     L"ійтеся"), 5 }, // бі~йтеся
			//{ std::wstring(     L"ойтеся"), 5 }, // благоустро~йтеся
			//{ std::wstring(     L"уйтеся"), 5 }, // актуалізу~йтеся
			//{ std::wstring(     L"юйтеся"), 5 }, // анігілю~йтеся
			{ std::wstring(      L"ьтеся"), 4 }, // аркань~теся
			{ std::wstring(        L"ися"), 3, WordClass::Verb }, // бери~ся диви~ся береж~ися
			{ std::wstring(        L"ися"), 3, WordClass::VerbalAdverb }, // берігш~ися
			{ std::wstring(      L"алися"), 4 }, // назива~лися
			{ std::wstring(    L"увалися"), 6 }, // актуалізу~валися
			{ std::wstring(    L"ювалися"), 6 }, // анігілю~валися
			{ std::wstring(      L"илися"), 4 }, // брижи~лися би~лися
			//{ std::wstring(      L"їлися"), 4 }, // благоустрої~лися
			{ std::wstring(     L"оїлися"), 5 }, // благоустро~їлися
			{ std::wstring(      L"олися"), 5 }, // боро~лися
			{ std::wstring(      L"улися"), 5 }, // бехну~лися
			{ std::wstring(      L"ялися"), 5 }, // боя~лися
			{ std::wstring(      L"атися"), 4 }, // назива~тися
			{ std::wstring(    L"уватися"), 6 }, // актуалізу~ватися
			{ std::wstring(    L"юватися"), 6 }, // анігілю~ватися
			{ std::wstring(      L"итися"), 4 }, // брижи~тися би~тися
			{ std::wstring(      L"отися"), 4 }, // боро~тися
			//{ std::wstring(      L"їтися"), 4 }, // благоустрої~тися
			{ std::wstring(     L"оїтися"), 5 }, // благоустро~їтися
			{ std::wstring(      L"утися"), 4 }, // бехну~тися
			{ std::wstring(      L"ятися"), 4 }, // бо~ятися
			{ std::wstring(     L"авшися"), 5, WordClass::VerbalAdverb }, // назива~вшися бра~вшися
			{ std::wstring(     L"овшися"), 5, WordClass::VerbalAdverb }, // боро~вшися
			{ std::wstring(   L"увавшися"), 7, WordClass::VerbalAdverb }, // актуалізу~вавшися
			{ std::wstring(   L"ювавшися"), 7, WordClass::VerbalAdverb }, // анігілю~вавшися асоцію~вавшися
			{ std::wstring(     L"ившися"), 5, WordClass::VerbalAdverb }, // безчести~вшися би~вшися
			//{ std::wstring(     L"ївшися"), 5, WordClass::VerbalAdverb }, // благоустрої~вшися
			{ std::wstring(    L"оївшися"), 6, WordClass::VerbalAdverb }, // благоустро~ївшися
			{ std::wstring(     L"увшися"), 5, WordClass::VerbalAdverb }, // бовтну~вшися зверну~вшися

			{ std::wstring(        L"йся"), 3, WordClass::Participle }, // compound
			//{ std::wstring(       L"айся"), 3, WordClass::Participle }, // айся назива~йся
			//{ std::wstring(       L"ийся"), 3, WordClass::Participle }, // берігши~йся би~йся
			//{ std::wstring(       L"ійся"), 3, WordClass::Participle }, // бі~йся
			//{ std::wstring(       L"ойся"), 3, WordClass::Participle }, // благоустро~йся
			//{ std::wstring(       L"уйся"), 3, WordClass::Participle }, // актуалізу~йся
			//{ std::wstring(       L"юйся"), 3, WordClass::Participle }, // анігілю~йся
			{ std::wstring(    L"авшийся"), 6, WordClass::Participle }, // бра~вшийся
			{ std::wstring(  L"увавшийся"), 8, WordClass::Participle }, // акуталізу~вавшийся
			{ std::wstring(  L"ювавшийся"), 8, WordClass::Participle }, // анігілю~вавшийся асоцію~вавшийся
			{ std::wstring(    L"ившийся"), 6, WordClass::Participle }, // брижи~вшийся би~вшийся
			//{ std::wstring(    L"ївшийся"), 6 }, // благоустрої~вшийся
			{ std::wstring(   L"оївшийся"), 7, WordClass::Participle  }, // благоустро~ївшийся
			{ std::wstring(    L"овшийся"), 6, WordClass::Participle }, // боро~вшийся
			{ std::wstring(    L"увшийся"), 6, WordClass::Participle }, // бехну~вшийся
			{ std::wstring(    L"явшийся"), 7, WordClass::Participle  }, // бо~явшийся
			{ std::wstring(       L"емся"), 3 }, // бере~мся береже~мся зверне~мся
			{ std::wstring(     L"имемся"), 5 }, // берегти~мемся
			{ std::wstring(       L"ємся"), 4 }, // compound
			//{ std::wstring(      L"аємся"), 4 }, // назива~ємся
			//{ std::wstring(      L"уємся"), 4 }, // актуалізу~ємся
			//{ std::wstring(      L"юємся"), 4 }, // анігілю~ємся
			//{ std::wstring(      L"'ємся"), 4 }, // б'~ємся
			{ std::wstring(   L"атимемся"), 7 }, // бра~тимемся
			{ std::wstring(   L"итимемся"), 7 }, // брижи~тимемся би~тимемся
			//{ std::wstring(   L"їтимемся"), 7 }, // гної~тимемся
			{ std::wstring(  L"оїтимемся"), 8 }, // гно~їтимемся
			{ std::wstring(   L"отимемся"), 7 }, // боро~тимемся
			{ std::wstring( L"уватимемся"), 9 }, // актуалізу~ватимемся
			{ std::wstring( L"юватимемся"), 9 }, // анігілю~ватимемся
			{ std::wstring(   L"ятимемся"), 8 }, // бо~ятимемся
			{ std::wstring(       L"имся"), 3 }, // брижи~мся
			//{ std::wstring(       L"їмся"), 3 }, // благоустрої~мся
			{ std::wstring(      L"оїмся"), 4 }, // благоустро~їмся
			//{ std::wstring(        L"ося"), 2 }, // no words біймо~ся
			{ std::wstring(       L"лося"), 4 }, // берег~лося
			{ std::wstring(      L"алося"), 4 }, // назива~лося
			{ std::wstring(    L"увалося"), 6 }, // актуалізу~валося
			{ std::wstring(    L"ювалося"), 6 }, // анігілю~валося
			{ std::wstring(      L"елося"), 4 }, // дове~лося
			{ std::wstring(      L"илося"), 4 }, // брижи~лося би~лося
			//{ std::wstring(      L"їлося"), 4 }, // благоустрої~лося
			{ std::wstring(     L"оїлося"), 5 }, // благоустро~їлося
			{ std::wstring(      L"олося"), 4 }, // боро~лося
			{ std::wstring(      L"улося"), 4 }, // бехну~лося
			{ std::wstring(      L"ялося"), 5 }, // бо~ялося
			{ std::wstring(       L"мося"), 4 }, // typo:поквап~мося
			{ std::wstring(      L"амося"), 4 }, // пода~мося
			{ std::wstring(      L"емося"), 4 }, // бере~мося поверне~мося
			{ std::wstring(    L"имемося"), 6 }, // берегти~мемося
			{ std::wstring(  L"атимемося"), 8 }, // назива~тимемося
			{ std::wstring(L"уватимемося"), 10 }, // актуалізу~ватимемося
			{ std::wstring(L"юватимемося"), 10 }, // анігілю~ватимемося
			{ std::wstring(  L"итимемося"), 8 }, // брижи~тимемося би~тимемося
			//{ std::wstring(  L"їтимемося"), 8 }, // гної~тимемося
			{ std::wstring( L"оїтимемося"), 9 }, // гно~їтимемося
			{ std::wstring(  L"отимемося"), 8 }, // боро~тимемося
			{ std::wstring(  L"ятимемося"), 8 }, // боя~тимемося
			{ std::wstring(      L"ємося"), 5 }, // compound
			//{ std::wstring(     L"аємося"), 5 }, // дізна~ємося назива~ємося
			//{ std::wstring(     L"уємося"), 5 }, // актуалізу~ємося
			//{ std::wstring(     L"юємося"), 5 }, // анігілю~ємося
			//{ std::wstring(     L"'ємося"), 5 }, // б'~ємося
			{ std::wstring(      L"имося"), 4 }, // брижи~мося обмежи-мося
			{ std::wstring(      L"імося"), 4 }, // бер~імося диві~мося
			//{ std::wstring(      L"їмося"), 4 }, // no words
			//{ std::wstring(     L"аїмося"), 4 }, // too little words (на~їмося)
			{ std::wstring(     L"оїмося"), 5 }, // бо~їмося благоустро~їмося
			{ std::wstring(      L"ймося"), 5 }, // compound
			//{ std::wstring(     L"аймося"), 5 }, // назива~ймося верта~ймося
			//{ std::wstring(     L"иймося"), 5 }, // би~ймося
			//{ std::wstring(     L"іймося"), 5 }, // бі~ймося
			//{ std::wstring(     L"оймося"), 5 }, // благоустро~ймося
			//{ std::wstring(     L"уймося"), 5 }, // актуалізу~ймося
			//{ std::wstring(     L"юймося"), 5 }, // анігілю~ймося
			{ std::wstring(      L"ьмося"), 4 }, // безчесть~мося скинь~мося зосередь~мося
			{ std::wstring(        L"уся"), 3 }, // бер~уся безчещ~уся
			{ std::wstring(      L"имуся"), 4 }, // берегти~муся
			{ std::wstring(    L"атимуся"), 6 }, // бра~тимуся
			{ std::wstring(    L"отимуся"), 6 }, // боро~тимуся
			//{ std::wstring(    L"їтимуся"), 6 }, // гної~тимуся
			{ std::wstring(   L"оїтимуся"), 7 }, // гно~їтимуся
			{ std::wstring(  L"уватимуся"), 8 }, // актуалізу~ватимуся
			{ std::wstring(  L"юватимуся"), 8 }, // анігілю~ватимуся
			{ std::wstring(    L"итимуся"), 6 }, // брижи~тимуся би~тимуся
			{ std::wstring(    L"ятимуся"), 6 }, // бо~ятимуся
			{ std::wstring(       L"ешся"), 4 }, // бер~ешся
			{ std::wstring(     L"имешся"), 5 }, // берегти~мешся
			{ std::wstring(   L"атимешся"), 7 }, // назива~тимешся
			{ std::wstring(   L"отимешся"), 7 }, // боро~тимешся
			{ std::wstring( L"уватимешся"), 9 }, // актуалізу~ватимешся
			{ std::wstring( L"юватимешся"), 9 }, // анігілю~ватимешся
			{ std::wstring(   L"итимешся"), 7 }, // брижи~тимешся би~тимешся
			//{ std::wstring(   L"їтимешся"), 7 }, // гної~тимешся
			{ std::wstring(  L"оїтимешся"), 8 }, // гно~їтимешся
			{ std::wstring(   L"ятимешся"), 8 }, // бо~ятимешся
			{ std::wstring(       L"єшся"), 4 }, // compound
			//{ std::wstring(      L"аєшся"), 4 }, // назива~єшся
			//{ std::wstring(      L"уєшся"), 4 }, // актуалізу~єшся
			//{ std::wstring(      L"юєшся"), 4 }, // анігілю~єшся
			//{ std::wstring(      L"'єшся"), 4 }, // б'~єшся
			{ std::wstring(       L"ишся"), 4 }, // бриж~ишся
			//{ std::wstring(       L"їшся"), 4 }, // бо~їшся благоустро~їшся
			{ std::wstring(      L"оїшся"), 4 }, // бо~їшся благоустро~їшся
			{ std::wstring(        L"ься"), 2 }, // безчесть~ся, do not work without SA
			//{ std::wstring(       L"ться"), 4 }, // Must have vowel (Ut1sa) NOT:безчес*ть~ся
			{ std::wstring(      L"аться"), 4 }, // назива~ться
			{ std::wstring(    L"уваться"), 6 }, // актуалізу~ваться
			{ std::wstring(    L"юваться"), 6 }, // анігілю~ваться
			{ std::wstring(      L"еться"), 4 }, // бере~ться береже~ться
			{ std::wstring(    L"иметься"), 6 }, // берегти~меться
			{ std::wstring(  L"атиметься"), 8 }, // назива~тиметься
			{ std::wstring(L"юватиметься"), 10 }, // анігілю~ватиметься
			{ std::wstring(L"уватиметься"), 10 }, // актуалізу~ватиметься
			{ std::wstring(L"итиметься"), 8 }, // брижи~тиметься би~тиметься
			//{ std::wstring(  L"їтиметься"), 8 }, // гної~тиметься
			{ std::wstring( L"оїтиметься"), 9 }, // гно~їтиметься
			{ std::wstring(  L"отиметься"), 8 }, // боро~тиметься
			{ std::wstring(  L"ятиметься"), 9 }, // бо~ятиметься

			{ std::wstring(      L"ється"), 5 }, // compound
			//{ std::wstring(     L"ається"), 5 }, // назива~ється
			//{ std::wstring(     L"ується"), 5 }, // актуалізу~ється
			//{ std::wstring(     L"юється"), 5 }, // анігілю~ється
			//{ std::wstring(     L"'ється"), 5 }, // б'~ється
			{ std::wstring(      L"иться"), 4 }, // брижи~ться би~ться
			{ std::wstring(      L"іться"), 4 }, // диві~ться бері~ться бережі~ться
			//{ std::wstring(      L"їться"), 5 }, // благоустро~їться
			{ std::wstring(     L"оїться"), 5 }, // благоустро~їться заспоко~їться
			{ std::wstring(      L"оться"), 4 }, // боро~ться
			{ std::wstring(      L"уться"), 4 }, // бехну~ться беру~ться
			{ std::wstring(    L"имуться"), 6 }, // берегти~муться
			{ std::wstring(  L"атимуться"), 8 }, // назива~тимуться
			{ std::wstring(  L"отимуться"), 8 }, // боро~тимуться
			{ std::wstring(L"уватимуться"), 10 }, // актуалізу~ватимуться
			{ std::wstring(L"юватимуться"), 10 }, // анігілю~ватимуться
			{ std::wstring(  L"итимуться"), 8 }, // брижи~тимуться би~тимуться
			//{ std::wstring(  L"їтимуться"), 9 }, // гно~їтимуться
			{ std::wstring( L"оїтимуться"), 9 }, // гно~їтимуться
			{ std::wstring(  L"ятимуться"), 9 }, // бо~ятимуться
			{ std::wstring(      L"ються"), 5 }, // compound бор~ються
			//{ std::wstring(     L"аються"), 5 }, // назива~ються
			//{ std::wstring(     L"юються"), 5 }, // анігілю~ються
			//{ std::wstring(     L"'ються"), 5 }, // б'~ються
			//{ std::wstring(      L"їться"), 4 }, // заспокої~ться
			{ std::wstring(     L"уються"), 5 }, // актуалізу~ються
			{ std::wstring(      L"яться"), 5}, // безчест~яться дивл~яться
			{ std::wstring(     L"ояться"), 5 }, // заспоко~яться гно~яться бо~яться
			{ std::wstring(        L"юся"), 3 }, // compound аркан~юся дивл~юся
			//{ std::wstring(       L"аюся"), 3 }, // зна~юся назива~юся
			//{ std::wstring(       L"оюся"), 3 }, // бо~юся благоустро~юся гно~юся
			//{ std::wstring(       L"уюся"), 3 }, // актуалізу~юся
			//{ std::wstring(       L"ююся"), 3 }, // анігілю~юся хвилю~юся
			//{ std::wstring(       L"'юся"), 3 }, // б'~юся
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
		if (suffixFirstChar == L'ь')
			return false;

		// WHY?
		// allow suffixes like "jmo"
		//if (suffixFirstChar == L'й' && suffixSize == 1)
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
				if (word[prefixSize] == L'ь' && prefixSize + 1 < word.size())
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
				// боявшийся -> б
				// need prefixes with size>1 to distinguish (би~йся, бі~йся)
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
		bool voiceless = ch == L'ч'; // буркоч
		return voiceless;
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
								wordLastChar == L'ь' ||
								wordLastChar == L'ж' || // бродяж
								voiceless;

							// it is ok to not finding consonant+sa
							if (!ok && endsWith(subWord, (wv::slice<wchar_t>)std::wstring(L"ся")))
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
						//	static std::wstring suffixVa(L"ва");
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
					if (ch == L'ь' || voiceless || ch == L'-')
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

	long UkrainianPhoneticSplitter::totalWordParts() const
	{
		return totalWordParts_;
	}

	void UkrainianPhoneticSplitter::buildLangModel(const wchar_t* textFilesDir, long& totalPreSplitWords, int maxFileToProcess)
	{
		QXmlStreamReader xml;
		totalPreSplitWords = 0;

		std::vector<wv::slice<wchar_t>> words;
		words.reserve(64);
		std::vector<const WordPart*> wordParts;
		wordParts.reserve(1024);
		std::vector<const WordPart*> wordPartsStraight;
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

						wordParts.clear();
						selectWordParts(words, wordParts, totalPreSplitWords);
						if (wordParts.empty())
							continue;

						// select sequantial word parts without separator
						size_t wordPartInd = 0;
						auto takeWordPartsTillNull = [&wordParts,&wordPartInd](std::vector<const WordPart*>& outWordParts) -> bool
						{
							// returns true if result contains data
							while (wordPartInd < wordParts.size())
							{
								for (; wordPartInd < wordParts.size(); ++wordPartInd)
								{
									const WordPart* wordPartPtr = wordParts[wordPartInd];
									if (wordPartPtr == nullptr)
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
						}
					}
				}
			}

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
				wordParts.push_back(nullptr); // word parts separator
				continue;
			}

			std::wstring str = toString(wordSlice);

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
	}

	void UkrainianPhoneticSplitter::calcLangStatistics(const std::vector<const WordPart*>& wordParts)
	{
		for (const WordPart* wordPart : wordParts)
		{
			WordSeqKey wordIds({ wordPart->id() });
			WordSeqUsage* wordSeq = wordUsage_.getOrAddWordSequence(wordIds);
			wordSeq->UsedCount++;
			totalWordParts_++;
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