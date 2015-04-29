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
		const wchar_t Letter_Space = L' ';
		const wchar_t Letter_Hyphen = L'-';
		const wchar_t Letter_Apostrophe = L'\'';
		const wchar_t Letter_A = L'�';
		const wchar_t Letter_B = L'�';
		const wchar_t Letter_H = L'�';
		const wchar_t Letter_D = L'�';
		const wchar_t Letter_E = L'�';
		const wchar_t Letter_JE = L'�';
		const wchar_t Letter_ZH = L'�';
		const wchar_t Letter_Z = L'�';
		const wchar_t Letter_I = L'�';
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
		if (a.Phones.size() < b.Phones.size())
			return true;
		else if (a.Phones.size() > b.Phones.size())
			return false;
		return false;
	}

	PhoneId PhoneRegistry::extendPhoneId(int validPhoneId) const
	{
		PhoneId result;
		result.Id = validPhoneId;
#if PG_DEBUG
		static std::string phoneStr;
		bool toStrOp = phoneToStr(*this, validPhoneId, phoneStr);
		PG_DbgAssert(toStrOp && "Invalid PhoneId:int");
		result.fillStr(phoneStr);
#endif
		return result;
	}

	PhoneRegistry::BasicPhoneIdT PhoneRegistry::extendBasicPhoneId(int basicPhoneId) const
	{
		BasicPhoneIdT result;
		result.Id = basicPhoneId;
#ifdef PG_DEBUG
		int basicPhoneStrInd = basicPhoneId - 1;

		PG_DbgAssert(basicPhoneStrInd >= 0 && basicPhoneStrInd < basicPhones_.size());
		const BasicPhone& basicPhone = basicPhones_[basicPhoneStrInd];

		result.fillStr(basicPhone.Name);
#endif
		return result;
	}

	PhoneRegistry::BasicPhoneIdT PhoneRegistry::getOrCreateBasicPhone(const std::string& basicPhoneStr, CharGroup charGroup)
	{
		auto it = basicPhonesStrToId_.find(basicPhoneStr);
		if (it != basicPhonesStrToId_.end())
			return extendBasicPhoneId(it->second);

		int newId = basicPhones_.size() + 1;

		BasicPhone basicPhone;
		basicPhone.Id = newId;
		basicPhone.Name = basicPhoneStr;
		basicPhone.DerivedFromChar = charGroup;
		basicPhones_.push_back(basicPhone);
		basicPhonesStrToId_[basicPhoneStr] = newId;
		return extendBasicPhoneId(newId);
	}

	PhoneRegistry::BasicPhoneIdT PhoneRegistry::basicPhoneId(const std::string& basicPhoneStr, bool* success) const
	{
		*success = false;

		auto it = basicPhonesStrToId_.find(basicPhoneStr);
		if (it != basicPhonesStrToId_.end())
		{
			*success = true;
			return extendBasicPhoneId(it->second);
		}
		return PhoneRegistry::BasicPhoneIdT();
	}

	const BasicPhone* PhoneRegistry::basicPhone(BasicPhoneIdT basicPhoneId) const
	{
		int basicPhoneInd = basicPhoneId.Id - 1;
		if (basicPhoneInd < 0 || basicPhoneInd >= basicPhones_.size())
			return nullptr;
		return &basicPhones_[basicPhoneInd];
	}

	PhoneId PhoneRegistry::newVowelPhone(const std::string& basicPhoneStr, bool isStressed)
	{
		int phoneId = nextPhoneId_++;

		Phone phone;
		phone.Id = phoneId;
		phone.BasicPhoneId = getOrCreateBasicPhone(basicPhoneStr, CharGroup::Vowel);
		phone.IsStressed = isStressed;
		phoneReg_.push_back(phone);
		return extendPhoneId(phoneId);
	}

	PhoneId PhoneRegistry::newConsonantPhone(const std::string& basicPhoneStr, boost::optional<SoftHardConsonant> softHard)
	{
		int phoneId = nextPhoneId_++;

		Phone phone;
		phone.Id = phoneId;
		phone.BasicPhoneId = getOrCreateBasicPhone(basicPhoneStr, CharGroup::Consonant);
		phone.SoftHard = softHard;
		phoneReg_.push_back(phone);
		return extendPhoneId(phoneId);
	}

	void PhoneRegistry::findPhonesByBasicPhoneStr(const std::string& basicPhoneStr, std::vector<PhoneId>& phoneIds) const
	{
		auto basicPhoneIt = basicPhonesStrToId_.find(basicPhoneStr);
		if (basicPhoneIt == basicPhonesStrToId_.end())
			return;
		int basicPhoneId = basicPhoneIt->second;

		for (size_t phoneInd = 0; phoneInd < phoneReg_.size(); ++phoneInd)
		{
			const Phone& ph = phoneReg_[phoneInd];
			if (ph.BasicPhoneId.Id == basicPhoneId)
				phoneIds.push_back(extendPhoneId(phoneInd + 1));
		}
	}

	int PhoneRegistry::phonesCount() const
	{
		assumeSequentialPhoneIdsWithoutGaps();
		return nextPhoneId_ - 1;
	}

	const Phone* PhoneRegistry::phoneById(int phoneId) const
	{
		int phoneInd = phoneId - 1;
		if (phoneInd < 0 || phoneInd >= phoneReg_.size())
			return nullptr;
		return &phoneReg_[phoneInd];
	}

	const Phone* PhoneRegistry::phoneById(PhoneId phoneId) const
	{
		return phoneById(phoneId.Id);
	}

	boost::optional<PhoneId> PhoneRegistry::phoneIdSingle(const std::string& basicPhoneStr, boost::optional<SoftHardConsonant> softHard, boost::optional<bool> isStressed) const
	{
		bool suc = false;
		auto basicPhId = basicPhoneId(basicPhoneStr, &suc);
		if (!suc)
			return nullptr;
		return phoneIdSingle(basicPhId, softHard, isStressed);
	}

	boost::optional<PhoneId> PhoneRegistry::phoneIdSingle(PhoneRegistry::BasicPhoneIdT basicPhoneStrId, boost::optional<SoftHardConsonant> softHardRequested, boost::optional<bool> isStressed) const
	{
		const BasicPhone* basicPh = basicPhone(basicPhoneStrId);
		if (basicPh == nullptr)
			return nullptr;
		std::vector<PhoneId> candidates;
		findPhonesByBasicPhoneStr(basicPh->Name, candidates);

		if (candidates.empty())
			return nullptr;
		if (candidates.size() == 1)
			return candidates.front();

		boost::optional<SoftHardConsonant> softHardFixed = softHardRequested;
		if (softHardRequested != nullptr)
		{
			if (allowSoftHardConsonant())
			{
				if (softHardRequested == SoftHardConsonant::Palatal)
				{
					     if (palatalSupport() == PalatalSupport::AsHard)
						softHardFixed = SoftHardConsonant::Hard;
					else if (palatalSupport() == PalatalSupport::AsSoft)
						softHardFixed = SoftHardConsonant::Soft;
					else if (palatalSupport() == PalatalSupport::AsPalatal)
						softHardFixed = SoftHardConsonant::Palatal;
				}
			}
		}

		std::vector<PhoneId> excactCandidates;
		std::remove_copy_if(candidates.begin(), candidates.end(), std::back_inserter(excactCandidates), [this, softHardFixed, isStressed](PhoneId phoneId)
		{
			const Phone* phone = phoneById(phoneId);
			if (softHardFixed != nullptr && softHardFixed != phone->SoftHard)
				return true;
			if (isStressed != nullptr && isStressed != phone->IsStressed)
				return true;
			return false;
		});

		if (excactCandidates.size() == 1)
			return excactCandidates.front();

		// exact candidates size:
		// = 0 then we may fallback to basic phone candidates; but there is no way to choose the best one
		// > 1 then again the best one can't be chosen
		return nullptr;
	}

	bool PhoneRegistry::allowSoftHardConsonant() const
	{
		return allowSoftConsonant_;
	}

	bool PhoneRegistry::allowVowelStress() const
	{
		return allowVowelStress_;
	}

	PalatalSupport PhoneRegistry::palatalSupport() const
	{
		return palatalSupport_;
	}

	void PhoneRegistry::setPalatalSupport(PalatalSupport value)
	{
		palatalSupport_ = value;
	}

	boost::optional<SoftHardConsonant> PhoneRegistry::defaultSoftHardConsonant() const
	{
		if (allowSoftHardConsonant())
			return SoftHardConsonant::Hard;
		return nullptr;
	}

	boost::optional<bool> PhoneRegistry::defaultIsVowelStressed() const
	{
		if (allowVowelStress())
			return false;
		return boost::none;
	}

	void initPhoneRegistryUk(PhoneRegistry& phoneReg, bool allowSoftConsonant, bool allowVowelStress)
	{
		phoneReg.allowSoftConsonant_ = allowSoftConsonant;
		phoneReg.allowVowelStress_ = allowVowelStress;
		bool allowPalatalizedConsonant = false;
		SoftHardConsonant palatalProxy = SoftHardConsonant::Hard;
		if (phoneReg.palatalSupport() == PalatalSupport::AsPalatal)
		{
			allowPalatalizedConsonant = true;
			palatalProxy = SoftHardConsonant::Palatal;
		}
		else if (phoneReg.palatalSupport() == PalatalSupport::AsHard)
		{
			allowPalatalizedConsonant = false;
		}
		else if (phoneReg.palatalSupport() == PalatalSupport::AsSoft)
		{
			allowPalatalizedConsonant = true;
			palatalProxy = SoftHardConsonant::Soft;
		}

		phoneReg.newVowelPhone("A", false); // A
		if (allowVowelStress)
			phoneReg.newVowelPhone("A", true); // A1
		phoneReg.newConsonantPhone("B", SoftHardConsonant::Hard); // B, hard (���), palatalized (����)
		if (allowPalatalizedConsonant)
			phoneReg.newConsonantPhone("B", palatalProxy); // B''
		phoneReg.newConsonantPhone("V", SoftHardConsonant::Hard); // V, hard (����), palatalized (����)
		if (allowPalatalizedConsonant)
			phoneReg.newConsonantPhone("V", palatalProxy); // V''
		phoneReg.newConsonantPhone("G", SoftHardConsonant::Hard); // G, hard (gudzyk, ����), palatalized
		if (allowPalatalizedConsonant)
			phoneReg.newConsonantPhone("G", palatalProxy); // G''
		phoneReg.newConsonantPhone("H", SoftHardConsonant::Hard); // H, hard (gluhi, ����), palatalized (����)
		if (allowPalatalizedConsonant)
			phoneReg.newConsonantPhone("H", palatalProxy); // H''
		phoneReg.newConsonantPhone("D", SoftHardConsonant::Hard); // D
		if (allowSoftConsonant)
			phoneReg.newConsonantPhone("D", SoftHardConsonant::Soft); // D1
		phoneReg.newConsonantPhone("DZ", SoftHardConsonant::Hard); // DZ
		if (allowSoftConsonant)
			phoneReg.newConsonantPhone("DZ", SoftHardConsonant::Soft); // DZ1
		phoneReg.newConsonantPhone("DZH", SoftHardConsonant::Hard); // DZH hard (����), palatalized
		if (allowPalatalizedConsonant)
			phoneReg.newConsonantPhone("DZH", palatalProxy); // DZH''
		phoneReg.newVowelPhone("E", false); // E
		if (allowVowelStress)
			phoneReg.newVowelPhone("E", true); // E1
		phoneReg.newConsonantPhone("ZH", SoftHardConsonant::Hard); // ZH, hard (����), palatalized (���)
		if (allowPalatalizedConsonant)
			phoneReg.newConsonantPhone("ZH", palatalProxy); // ZH''
		phoneReg.newConsonantPhone("Z", SoftHardConsonant::Hard); // Z
		if (allowSoftConsonant)
			phoneReg.newConsonantPhone("Z", SoftHardConsonant::Soft); // Z1
		phoneReg.newVowelPhone("Y", false); // Y
		if (allowVowelStress)
			phoneReg.newVowelPhone("Y", true); // Y1
		phoneReg.newVowelPhone("I", false); // I
		if (allowVowelStress)
			phoneReg.newVowelPhone("I", true); // I1
		phoneReg.newConsonantPhone("J", SoftHardConsonant::Hard); // J, hard only (�)
		phoneReg.newConsonantPhone("K", SoftHardConsonant::Hard); // K, hard (�����), palatalized (�����)
		if (allowPalatalizedConsonant)
			phoneReg.newConsonantPhone("K", palatalProxy); // K''
		phoneReg.newConsonantPhone("L", SoftHardConsonant::Hard); // L
		if (allowSoftConsonant)
			phoneReg.newConsonantPhone("L", SoftHardConsonant::Soft); // L1
		phoneReg.newConsonantPhone("M", SoftHardConsonant::Hard); // M, hard (��), palatalized (��)
		if (allowPalatalizedConsonant)
			phoneReg.newConsonantPhone("M", palatalProxy); // M''
		phoneReg.newConsonantPhone("N", SoftHardConsonant::Hard); // N
		if (allowSoftConsonant)
			phoneReg.newConsonantPhone("N", SoftHardConsonant::Soft); // N1
		phoneReg.newVowelPhone("O", false); // O
		if (allowVowelStress)
			phoneReg.newVowelPhone("O", true); // O1
		phoneReg.newConsonantPhone("P", SoftHardConsonant::Hard); // P, hard (�'���), palatalized (��)
		if (allowPalatalizedConsonant)
			phoneReg.newConsonantPhone("P", palatalProxy); // P''
		phoneReg.newConsonantPhone("R", SoftHardConsonant::Hard); // R
		if (allowSoftConsonant)
			phoneReg.newConsonantPhone("R", SoftHardConsonant::Soft); // R1
		phoneReg.newConsonantPhone("S", SoftHardConsonant::Hard); // S
		if (allowSoftConsonant)
			phoneReg.newConsonantPhone("S", SoftHardConsonant::Soft); // S1
		phoneReg.newConsonantPhone("T", SoftHardConsonant::Hard); // T
		if (allowSoftConsonant)
			phoneReg.newConsonantPhone("T", SoftHardConsonant::Soft); // T1
		phoneReg.newVowelPhone("U", false); // U
		if (allowVowelStress)
			phoneReg.newVowelPhone("U", true); // U1
		phoneReg.newConsonantPhone("F", SoftHardConsonant::Hard); // F, hard (����), palatalized (������)
		if (allowPalatalizedConsonant)
			phoneReg.newConsonantPhone("F", palatalProxy); // F''
		phoneReg.newConsonantPhone("KH", SoftHardConsonant::Hard); // KH, hard (�'�), palatalized (����)
		if (allowPalatalizedConsonant)
			phoneReg.newConsonantPhone("KH", palatalProxy); // KH''
		phoneReg.newConsonantPhone("TS", SoftHardConsonant::Hard); // TS
		if (allowSoftConsonant)
			phoneReg.newConsonantPhone("TS", SoftHardConsonant::Soft); // TS1
		phoneReg.newConsonantPhone("CH", SoftHardConsonant::Hard); // CH, hard (���), palatalized (���)
		if (allowPalatalizedConsonant)
			phoneReg.newConsonantPhone("CH", palatalProxy); // CH''
		phoneReg.newConsonantPhone("SH", SoftHardConsonant::Hard); // SH, hard (����, �'�) , palatalized (��������)
		if (allowPalatalizedConsonant)
			phoneReg.newConsonantPhone("SH", palatalProxy); // SH''
	}

	bool usuallyHardBasicPhone(const PhoneRegistry& phoneReg, Phone::BasicPhoneIdT basicPhoneId)
	{
		bool suc = true;
		bool foundMatch = false; // whether got the match
		auto usuallyHardPhone = [&suc, &foundMatch, &phoneReg](Phone::BasicPhoneIdT expectBasicPhoneId, const std::string& queryBasicPhoneStr) -> void
		{
			if (!suc || foundMatch)
				return;
			Phone::BasicPhoneIdT basicId = phoneReg.basicPhoneId(queryBasicPhoneStr, &suc);
			if (suc && expectBasicPhoneId == basicId)
				foundMatch = true;
		};
		usuallyHardPhone(basicPhoneId, "B");
		usuallyHardPhone(basicPhoneId, "P");
		usuallyHardPhone(basicPhoneId, "V");
		usuallyHardPhone(basicPhoneId, "M");
		usuallyHardPhone(basicPhoneId, "F");
		usuallyHardPhone(basicPhoneId, "H");
		usuallyHardPhone(basicPhoneId, "K");
		usuallyHardPhone(basicPhoneId, "KH");
		usuallyHardPhone(basicPhoneId, "G");
		usuallyHardPhone(basicPhoneId, "CH");
		usuallyHardPhone(basicPhoneId, "DZH");
		usuallyHardPhone(basicPhoneId, "SH");
		usuallyHardPhone(basicPhoneId, "ZH");
		PG_DbgAssert(suc);
		return foundMatch;
	}

	bool easilySoftBasicPhone(const PhoneRegistry& phoneReg, Phone::BasicPhoneIdT basicPhoneId)
	{
		bool suc = true;
		bool foundMatch = false; // whether got the match
		auto usuallyHardPhone = [&suc, &foundMatch, &phoneReg](Phone::BasicPhoneIdT expectBasicPhoneId, const std::string& queryBasicPhoneStr) -> void
		{
			if (!suc || foundMatch)
				return;
			Phone::BasicPhoneIdT basicId = phoneReg.basicPhoneId(queryBasicPhoneStr, &suc);
			if (suc && expectBasicPhoneId == basicId)
				foundMatch = true;
		};
		usuallyHardPhone(basicPhoneId, "D");
		usuallyHardPhone(basicPhoneId, "DZ");
		usuallyHardPhone(basicPhoneId, "T");
		usuallyHardPhone(basicPhoneId, "TS");
		usuallyHardPhone(basicPhoneId, "Z");
		usuallyHardPhone(basicPhoneId, "S");
		usuallyHardPhone(basicPhoneId, "N");
		usuallyHardPhone(basicPhoneId, "L");
		usuallyHardPhone(basicPhoneId, "R");
		PG_DbgAssert(suc);
		return foundMatch;
	}

	bool isNoisyVoicedConsonant(const PhoneRegistry& phoneReg, Phone::BasicPhoneIdT basicPhoneId)
	{
		bool suc = true;
		bool foundMatch = false; // whether got the match
		auto match = [&suc, &foundMatch, &phoneReg](Phone::BasicPhoneIdT expectBasicPhoneId, const std::string& queryBasicPhoneStr) -> void
		{
			if (!suc || foundMatch)
				return;
			Phone::BasicPhoneIdT basicId = phoneReg.basicPhoneId(queryBasicPhoneStr, &suc);
			if (suc && expectBasicPhoneId == basicId)
				foundMatch = true;
		};
		match(basicPhoneId, "B");
		match(basicPhoneId, "H");
		match(basicPhoneId, "G");
		match(basicPhoneId, "D");
		match(basicPhoneId, "DZ");
		match(basicPhoneId, "DZH");
		match(basicPhoneId, "Z");
		match(basicPhoneId, "ZH");
		PG_DbgAssert(suc);
		return foundMatch;
	}

	bool isNoisyUnvoicedConsonant(const PhoneRegistry& phoneReg, Phone::BasicPhoneIdT basicPhoneId)
	{
		bool suc = true;
		bool foundMatch = false; // whether got the match
		auto match = [&suc, &foundMatch, &phoneReg](Phone::BasicPhoneIdT expectBasicPhoneId, const std::string& queryBasicPhoneStr) -> void
		{
			if (!suc || foundMatch)
				return;
			Phone::BasicPhoneIdT basicId = phoneReg.basicPhoneId(queryBasicPhoneStr, &suc);
			if (suc && expectBasicPhoneId == basicId)
				foundMatch = true;
		};
		match(basicPhoneId, "P");
		match(basicPhoneId, "KH");
		match(basicPhoneId, "K");
		match(basicPhoneId, "T");
		match(basicPhoneId, "TS");
		match(basicPhoneId, "CH");
		match(basicPhoneId, "S");
		match(basicPhoneId, "SH");
		PG_DbgAssert(suc);
		return foundMatch;
	}

	boost::optional<Phone::BasicPhoneIdT> getVoicedUnvoicedConsonantPair(const PhoneRegistry& phoneReg, Phone::BasicPhoneIdT basicPhoneId)
	{
		bool suc = true;
		boost::optional<Phone::BasicPhoneIdT> partnerBasicPhoneId = nullptr;
		auto match = [&suc, &partnerBasicPhoneId, &phoneReg](Phone::BasicPhoneIdT expectBasicPhoneId, const std::string& basicStr, const std::string& dualBasicStr) -> void
		{
			if (!suc || partnerBasicPhoneId != nullptr)
				return;
			Phone::BasicPhoneIdT basicId1 = phoneReg.basicPhoneId(basicStr, &suc);
			if (!suc)
				return;
			Phone::BasicPhoneIdT basicId2 = phoneReg.basicPhoneId(dualBasicStr, &suc);
			if (!suc)
				return;
			if (expectBasicPhoneId == basicId1)
				partnerBasicPhoneId = basicId2;
			else if (expectBasicPhoneId == basicId2)
				partnerBasicPhoneId = basicId1;
		};
		match(basicPhoneId, "B", "P");
		match(basicPhoneId, "H", "KH");
		match(basicPhoneId, "G", "K");
		match(basicPhoneId, "D", "T");
		match(basicPhoneId, "DZ", "TS");
		match(basicPhoneId, "DZH", "CH");
		match(basicPhoneId, "Z", "S");
		match(basicPhoneId, "ZH", "SH");
		return partnerBasicPhoneId;
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

	std::tuple<bool, const char*> loadPhoneticDictionaryPronIdPerLine(const std::basic_string<wchar_t>& vocabFilePathAbs, const PhoneRegistry& phoneReg, const QTextCodec& textCodec, std::vector<PhoneticWord>& words, std::vector<std::basic_string<char>>& brokenLines)
	{
		// file contains text in Windows-1251 encoding
		QFile file(QString::fromStdWString(vocabFilePathAbs));
		if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
			return std::make_tuple(false, "Can't open file");

		// 
		std::array<char, 1024> lineBuff;
		PhoneticWord curPronGroup;
		std::vector<PhoneId> phones;

		// each line has a format:
		// sure\tsh u e\n
		while (true) {
			auto readBytes = file.readLine(lineBuff.data(), lineBuff.size());
			if (readBytes == -1) // EOF
				break;

			if (readBytes < 3) // 3=min length of Word->PhoneList
				continue;

			boost::string_ref line(lineBuff.data(), readBytes);
			if (line.back() == '\n')
				line.remove_suffix(1);

			const char* DictDelim = " \t\n";
			size_t phoneListInd = line.find_first_of(DictDelim);
			if (phoneListInd == boost::string_ref::npos)
				return std::make_tuple(false, "The word is too long (>1024 bytes)");

			boost::string_ref wordRef(line.data(), phoneListInd);
			if (wordRef.empty()) // empty line
				continue;

			boost::string_ref phoneListRef = line.substr(phoneListInd + 1);
			if (phoneListRef.empty()) // word without the list of phones
			{
				brokenLines.push_back(lineBuff.data());
				continue;
			}
			std::transform(phoneListRef.begin(), phoneListRef.end(), (char*)phoneListRef.begin(), toupper);

			phones.clear();
			bool parseOp = parsePhoneList(phoneReg, phoneListRef, phones);
			if (!parseOp)
			{
				brokenLines.push_back(line.data());
				continue;
			}

			QString word = textCodec.toUnicode(line.data(), phoneListInd);
			std::wstring wordW = word.toStdWString();

			if (curPronGroup.Word != wordW)
			{
				// finish previous pronunciation group
				if (!curPronGroup.Word.empty())
				{
					words.push_back(curPronGroup);
					curPronGroup.Word.clear();
					curPronGroup.Pronunciations.clear();
				}

				// start new group
				PG_DbgAssert(curPronGroup.Pronunciations.empty() && "Old pronunciation data must be purged");
				curPronGroup.Word = wordW;
			}

			PronunciationFlavour pron;
			pron.PronAsWord = wordW;
			pron.Phones = phones;
			curPronGroup.Pronunciations.push_back(pron);
		}

		if (!curPronGroup.Word.empty())
		{
			words.push_back(curPronGroup);
			curPronGroup.Word.clear();
			curPronGroup.Pronunciations.clear();
		}
		return std::make_tuple(true, nullptr);
	}

	void trimPhoneStrExtraInfos(const std::string& phoneStdStr, std::string& phoneStrTrimmed, bool toUpper, bool trimNumbers)
	{
		QString phoneStr = QString::fromStdString(phoneStdStr);
		if (trimNumbers)
		{
			if (phoneStr[phoneStr.size() - 1].isDigit())
			{
				phoneStr = phoneStr.left(phoneStr.size() - 1); // remove last digit
			}
		}
		if (toUpper)
			phoneStr = phoneStr.toUpper();
		phoneStrTrimmed = phoneStr.toStdString();
	}

	void normalizePronunciationVocabulary(std::map<std::wstring, std::vector<Pronunc>>& wordToPhoneList, bool toUpper, bool trimNumbers)
	{
		std::string phoneStrTrimmed;
		for (auto& pair : wordToPhoneList)
		{
			std::vector<Pronunc>& prons = pair.second;
			for (auto& pron : prons)
			{
				for (auto& phone : pron.Phones)
				{
					trimPhoneStrExtraInfos(phone, phoneStrTrimmed, toUpper, trimNumbers);
					phone = phoneStrTrimmed;
				}
			}

			std::sort(std::begin(prons), std::end(prons));
			auto it = std::unique(std::begin(prons), std::end(prons));
			size_t newSize = std::distance(std::begin(prons), it);
			prons.resize(newSize);
		}
	}

	boost::optional<PhoneId> parsePhoneStr(const PhoneRegistry& phoneReg, boost::string_ref phoneStrRef)
	{
		if (phoneStrRef.empty())
			return nullptr;

		boost::string_ref basicPhoneStrRef = phoneStrRef;

		// truncate last digit
		char ch = phoneStrRef.back();
		bool lastIsDigit = ::isdigit(ch);
		if (lastIsDigit)
			basicPhoneStrRef.remove_suffix(1);

		bool basicOp = false;
		std::string basicPhoneStr = std::string(basicPhoneStrRef.data(), basicPhoneStrRef.size());
		PhoneRegistry::BasicPhoneIdT basicPhoneId = phoneReg.basicPhoneId(basicPhoneStr, &basicOp);
		if (!basicOp)
			return nullptr;
		const BasicPhone* basicPhone = phoneReg.basicPhone(basicPhoneId);

		// defaults for phone name without a number suffix
		boost::optional<SoftHardConsonant> softHard = nullptr;
		if (basicPhone->DerivedFromChar == CharGroup::Consonant)
			softHard = phoneReg.defaultSoftHardConsonant();

		boost::optional<bool> isStressed = nullptr;
		if (basicPhone->DerivedFromChar == CharGroup::Vowel)
			isStressed = phoneReg.defaultIsVowelStressed();

		if (lastIsDigit)
		{
			// number on a consonant derived phone means half-softness for usually hard consonants and softness for other consonants
			if (basicPhone->DerivedFromChar == CharGroup::Consonant)
			{
				std::array<char, 2> digitBuf = { ch, 0 };
				int lastDigit = std::atoi(digitBuf.data());

				if (lastDigit == 1)
					softHard = SoftHardConsonant::Soft;
				else if (lastDigit == 2)
					softHard = SoftHardConsonant::Palatal;
				else return nullptr; // unknown number modifier
			}

			// number on a vowel derived phone means stress
			else if (basicPhone->DerivedFromChar == CharGroup::Vowel)
				isStressed = true;
		}

		boost::optional<PhoneId> result = phoneReg.phoneIdSingle(basicPhoneId, softHard, isStressed);
		return result;
	}

	bool parsePhoneList(const PhoneRegistry& phoneReg, boost::string_ref phoneListStr, std::vector<PhoneId>& result)
	{
		boost::string_ref curPhonesStr = phoneListStr;
		while (!curPhonesStr.empty())
		{
			size_t sepPos = curPhonesStr.find(' ');
			if (sepPos == boost::string_ref::npos)
				sepPos = curPhonesStr.size();

			boost::string_ref phoneRef = curPhonesStr.substr(0, sepPos);
			if (phoneRef.empty()) // a phone can't be an empty string
				continue;

			boost::optional<PhoneId> phoneId = parsePhoneStr(phoneReg, phoneRef);
			if (!phoneId)
				return false;

			result.push_back(phoneId.get());

			curPhonesStr.remove_prefix(sepPos + 1);
		}
		return true;
	}

	std::tuple<bool, const char*> parsePronuncLines(const PhoneRegistry& phoneReg, const std::basic_string<wchar_t>& prons, std::vector<PronunciationFlavour>& result)
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
			QString phonesStr = pronLine.mid(pronAsWordEndInd + 1);

			std::vector<PhoneId> phones;
			bool parseOp = parsePhoneList(phoneReg, phonesStr.toStdString(), phones);
			if (!parseOp)
				return std::make_tuple(false, "Can't parse the list of phones");

			PronunciationFlavour pron;
			pron.PronAsWord = pronAsWord.toStdWString();
			pron.Phones = phones;
			result.push_back(pron);
		}
		return std::make_tuple(true, nullptr);
	}

	bool phoneToStr(const PhoneRegistry& phoneReg, int phoneId, std::string& result)
	{
		const Phone* phone = phoneReg.phoneById(phoneId);
		if (phone == nullptr)
			return false;
		const BasicPhone* basicPh = phoneReg.basicPhone(phone->BasicPhoneId);
		result = basicPh->Name;

		const char SoftDecorator = '1';
		const char PalatalDecorator = '2';

		if (phoneReg.allowSoftHardConsonant()) // is soft consonant
		{
			     if (phone->SoftHard == SoftHardConsonant::Hard) { }
			else if (phone->SoftHard == SoftHardConsonant::Soft)
				result.push_back(SoftDecorator);
			else if (phone->SoftHard == SoftHardConsonant::Palatal)
			{
				     if (phoneReg.palatalSupport() == PalatalSupport::AsHard) { }
				else if (phoneReg.palatalSupport() == PalatalSupport::AsSoft)
					result.push_back(SoftDecorator);
				else if (phoneReg.palatalSupport() == PalatalSupport::AsPalatal)
					result.push_back(PalatalDecorator);
			}
		}
		if (phoneReg.allowVowelStress() && phone->IsStressed == true)
			result.push_back('1');

		return true;
	}

	bool phoneToStr(const PhoneRegistry& phoneReg, PhoneId phoneId, std::string& result)
	{
		return phoneToStr(phoneReg, phoneId.Id, result);
	}

	bool phoneListToStr(const PhoneRegistry& phoneReg, wv::slice<PhoneId> phones, std::string& result)
	{
		if (phones.empty())
			return true;

		std::string phStr;
		if (!phoneToStr(phoneReg, phones.front(), phStr))
			return false;
		result += phStr;

		for (int i = 1; i < phones.size(); ++i)
		{
			result.push_back(' ');

			if (!phoneToStr(phoneReg, phones[i], phStr))
				return false;
			result += phStr;
		}
		return true;
	}

	// Half made phone.
	// The incomplete phone's data, so that complete PhoneId can't be queried from phone registry.
	typedef decltype(static_cast<Phone*>(nullptr)->BasicPhoneId) BasicPhoneIdT;
	struct PhoneBillet
	{
		BasicPhoneIdT BasicPhoneId;
		boost::optional<CharGroup> DerivedFromChar = boost::none;
		boost::optional<SoftHardConsonant> SoftHard = boost::none; // valid for consonants only
		boost::optional<bool> IsStressed = boost::none; // valid for for vowels only
	};

	// Transforms text into sequence of phones.
	class WordPhoneticTranscriber
	{
		const PhoneRegistry* phoneReg_;
		const std::wstring* word_;
		size_t letterInd_;
		std::wstring errMsg_;
		std::vector<PhoneBillet> billetPhones_;
		std::vector<PhoneId> outputPhones_;
	public:
		void transcribe(const PhoneRegistry& phoneReg, const std::wstring& word);
		bool hasError() const;
		const std::wstring& errorMessage() const;
		void copyOutputPhoneIds(std::vector<PhoneId>& phoneIds) const;
	private:
		// The current processed character of the word.
		inline wchar_t curLetter() const;
		inline wchar_t offsetLetter(int offset) const;
		inline bool isFirstLetter() const;
		inline bool isLastLetter() const;

		PhoneBillet newConsonantPhone(const std::string& basicPhoneStr, boost::optional<SoftHardConsonant> SoftHard) const;
		PhoneBillet newVowelPhone(const std::string& basicPhoneStr, boost::optional<bool> isStressed) const;

		// Maps letter to one phone when there is a reasonable phone candidate. More complicated cases are handled via rules.
		bool makePhoneOneToOne(wchar_t letter, PhoneBillet& ph) const;

		bool ruleIgnore(); // do not require neighbourhood info
		bool ruleJi(); // do not require neighbourhood info
		bool ruleShCh(); // do not require neighbourhood info
		bool ruleDzDzh(); // progressive
		bool ruleZhDzh(); // progressive
		bool ruleNtsk(); // progressive
		bool ruleSShEtc(); // progressive
		bool ruleTsEtc(); // progressive
		bool ruleSoftSign(); // regressive
		bool ruleApostrophe(); // regressive
		bool ruleHardConsonantBeforeE(); // regressive
		bool ruleSoftConsonantBeforeI(); // regressive
		bool ruleDoubleJaJeJu(); // regressive
		bool ruleSoftConsonantBeforeJaJeJu(); // regressive
		bool ruleDampVoicedConsonantBeforeUnvoiced(); // progressive
		bool ruleDefaultSimpleOneToOneMap(); // do not require neighbourhood info

		// Checks whether the given phone is of the kind, which mutually softens each other.
		bool isMutuallySoftConsonant(Phone::BasicPhoneIdT basicPhoneId) const;
		// Rule: checks that if there are two consequent phones of specific type, and the second is soft, then the first becomes soft too.
		void postRulePairOfConsonantsSoftenEachOther();

		void postRuleAmplifyUnvoicedConsonantBeforeVoiced();

		bool tryRecoverStressedVowel();
		void buildOutputPhones();
	};

	bool WordPhoneticTranscriber::hasError() const
	{
		return !errMsg_.empty();
	}

	const std::wstring& WordPhoneticTranscriber::errorMessage() const
	{
		return errMsg_;
	}

	void WordPhoneticTranscriber::transcribe(const PhoneRegistry& phoneReg, const std::wstring& word)
	{
		phoneReg_ = &phoneReg;
		word_ = &word;
		errMsg_.clear();
		billetPhones_.clear();
		outputPhones_.clear();

		for (letterInd_ = 0; letterInd_ < word.size(); ++letterInd_)
		{
			if (hasError())
				return;

			wchar_t letter = word[letterInd_];
			
			if (ruleIgnore()) continue;
			if (ruleJi()) continue;
			if (ruleShCh()) continue;
			if (ruleDzDzh()) continue;
			if (ruleZhDzh()) continue;
			if (ruleNtsk()) continue;
			if (ruleSShEtc()) continue;
			if (ruleTsEtc()) continue;
			if (ruleSoftSign()) continue;
			if (ruleApostrophe()) continue;
			if (ruleHardConsonantBeforeE()) continue;
			if (ruleSoftConsonantBeforeI()) continue;
			if (ruleDoubleJaJeJu()) continue;
			if (ruleSoftConsonantBeforeJaJeJu()) continue;
			if (ruleDampVoicedConsonantBeforeUnvoiced()) continue;
			if (!ruleDefaultSimpleOneToOneMap())
			{
				errMsg_ = std::wstring(L"Unknown letter ") + letter;
				return;
			}
		}
		
		postRulePairOfConsonantsSoftenEachOther();
		postRuleAmplifyUnvoicedConsonantBeforeVoiced();

		tryRecoverStressedVowel();
		buildOutputPhones();
	}

	void WordPhoneticTranscriber::copyOutputPhoneIds(std::vector<PhoneId>& phoneIds) const
	{
		std::copy(outputPhones_.begin(), outputPhones_.end(), std::back_inserter(phoneIds));
	}

	wchar_t WordPhoneticTranscriber::curLetter() const
	{
		return (*word_)[letterInd_];
	}

	wchar_t WordPhoneticTranscriber::offsetLetter(int offset) const
	{
		int newIndex = static_cast<int>(letterInd_) + offset;
		PG_DbgAssert(newIndex >= 0 && newIndex < word_->size());
		return (*word_)[newIndex];
	}

	bool WordPhoneticTranscriber::isFirstLetter() const
	{
		PG_Assert(!word_->empty());
		return letterInd_ == 0;
	}

	bool WordPhoneticTranscriber::isLastLetter() const
	{
		PG_Assert(!word_->empty());
		return letterInd_ == word_->size() - 1;
	}
	
	bool WordPhoneticTranscriber::ruleIgnore()
	{
		wchar_t letter = curLetter();
		return letter == Letter_Hyphen || letter == Letter_Space;
	}

	bool WordPhoneticTranscriber::ruleJi()
	{
		bool accept = curLetter() == Letter_JI;
		if (!accept)
			return false;

		// Rule: letter JI always converts as J and I
		billetPhones_.push_back(newConsonantPhone("J", SoftHardConsonant::Hard));
		billetPhones_.push_back(newVowelPhone("I", nullptr));
		return true;
	}
	
	bool WordPhoneticTranscriber::ruleShCh()
	{
		bool accept = curLetter() == Letter_SHCH;
		if (!accept)
			return false;

		billetPhones_.push_back(newConsonantPhone("SH", SoftHardConsonant::Hard));
		billetPhones_.push_back(newConsonantPhone("CH", SoftHardConsonant::Hard));
		return true;
	}

	bool WordPhoneticTranscriber::ruleDzDzh()
	{
		bool accept = curLetter() == Letter_D && !isLastLetter();
		if (!accept)
			return false;

		wchar_t nextLetter = offsetLetter(1);
		if (nextLetter == Letter_Z)
		{
			billetPhones_.push_back(newConsonantPhone("DZ", nullptr));
			letterInd_ += 1; // skip next letter
			return true;
		}
		else if (nextLetter == Letter_ZH)
		{
			billetPhones_.push_back(newConsonantPhone("DZH", SoftHardConsonant::Hard));
			letterInd_ += 1; // skip next letter
			return true;
		}
		return false;
	}

	bool WordPhoneticTranscriber::ruleZhDzh()
	{
		bool accept = curLetter() == Letter_Z && !isLastLetter();
		if (!accept)
			return false;

		wchar_t nextLetter = offsetLetter(1);

		// Z ZH -> ZH ZH
		// ���� [ZH ZH E R]
		if (nextLetter == Letter_ZH)
		{
			// skip first T
			billetPhones_.push_back(newConsonantPhone("ZH", SoftHardConsonant::Hard));
			billetPhones_.push_back(newConsonantPhone("ZH", SoftHardConsonant::Hard));
			letterInd_ += 1;
			return true;
		}

		if (letterInd_ + 2 < word_->size()) // size(D ZH)=2
		{
			// Z D ZH -> ZH DZH
			// �'�������� [Z J I ZH DZH A J U T]
			if (nextLetter == Letter_D && offsetLetter(2) == Letter_ZH)
			{
				// skip first T
				billetPhones_.push_back(newConsonantPhone("ZH", SoftHardConsonant::Hard));
				billetPhones_.push_back(newConsonantPhone("DZH", SoftHardConsonant::Hard));
				letterInd_ += 2;
				return true;
			}
		}
		return false;
	}

	bool WordPhoneticTranscriber::ruleNtsk()
	{
		bool accept = curLetter() == Letter_N;
		if (!accept)
			return false;
		if (letterInd_ + 3 < word_->size()) // size(T S T)=3
		{
			// N T S T -> N S T
			if (offsetLetter(1) == Letter_T && offsetLetter(2) == Letter_S && offsetLetter(3) == Letter_T)
			{
				// skip first T
				billetPhones_.push_back(newConsonantPhone("N", SoftHardConsonant::Hard));
				billetPhones_.push_back(newConsonantPhone("S", SoftHardConsonant::Hard));
				billetPhones_.push_back(newConsonantPhone("T", SoftHardConsonant::Hard));
				letterInd_ += 3;
				return true;
			}
		}
		if (letterInd_ + 4 < word_->size()) // size(T S 1 K)=4
		{
			// N T S 1 K -> N S1 K
			if (offsetLetter(1) == Letter_T && offsetLetter(2) == Letter_S && offsetLetter(3) == Letter_SoftSign && offsetLetter(4) == Letter_K)
			{
				// skip first T
				billetPhones_.push_back(newConsonantPhone("N", SoftHardConsonant::Hard));
				billetPhones_.push_back(newConsonantPhone("S", SoftHardConsonant::Soft));
				billetPhones_.push_back(newConsonantPhone("K", SoftHardConsonant::Hard));
				letterInd_ += 4;
				return true;
			}
		}
		return false;
	}

	bool WordPhoneticTranscriber::ruleSShEtc()
	{
		bool accept = curLetter() == Letter_S;
		if (!accept)
			return false;
		if (letterInd_ + 1 < word_->size()) // size(SH)=1
		{
			// S SH -> SH SH
			// ������ [D O N I SH SH Y]
			if (offsetLetter(1) == Letter_SH)
			{
				billetPhones_.push_back(newConsonantPhone("SH", SoftHardConsonant::Hard));
				billetPhones_.push_back(newConsonantPhone("SH", SoftHardConsonant::Hard));
				letterInd_ += 1;
				return true;
			}
		}
		if (letterInd_ + 2 < word_->size()) // size(T D)=2
		{
			// S T D -> Z D
			// ��������� [SH I Z D E S A T]
			if (offsetLetter(1) == Letter_T && offsetLetter(2) == Letter_D)
			{
				billetPhones_.push_back(newConsonantPhone("Z", SoftHardConsonant::Hard));
				billetPhones_.push_back(newConsonantPhone("D", SoftHardConsonant::Hard));
				letterInd_ += 2;
				return true;
			}
		}
		// check STS1K group before STS group, because latter is inside the former
		if (letterInd_ + 4 < word_->size()) // size(T S 1 K)=4
		{
			// S T S 1 K -> S1 K
			// ���������� [N A TS Y S1 K O J I]
			if (offsetLetter(1) == Letter_T && offsetLetter(2) == Letter_S && offsetLetter(3) == Letter_SoftSign && offsetLetter(4) == Letter_K)
			{
				billetPhones_.push_back(newConsonantPhone("S", SoftHardConsonant::Soft));
				billetPhones_.push_back(newConsonantPhone("K", SoftHardConsonant::Hard));
				letterInd_ += 4;
				return true;
			}
		}
		if (letterInd_ + 2 < word_->size()) // size(T S)=2
		{
			// S T S -> S S
			// ������� [SH I S S O T]
			if (offsetLetter(1) == Letter_T && offsetLetter(2) == Letter_S)
			{
				billetPhones_.push_back(newConsonantPhone("S", SoftHardConsonant::Hard));
				billetPhones_.push_back(newConsonantPhone("S", SoftHardConsonant::Hard));
				letterInd_ += 2;
				return true;
			}
		}
		if (letterInd_ + 2 < word_->size()) // size(T TS)=2
		{
			// S T TS -> S TS
			// �������� [V I D P U S TS I]
			if (offsetLetter(1) == Letter_T && offsetLetter(2) == Letter_TS)
			{
				billetPhones_.push_back(newConsonantPhone("S", SoftHardConsonant::Hard));
				billetPhones_.push_back(newConsonantPhone("TS", SoftHardConsonant::Hard));
				letterInd_ += 2;
				return true;
			}
		}
		return false;
	}

	bool WordPhoneticTranscriber::ruleTsEtc()
	{
		bool accept = curLetter() == Letter_T && !isLastLetter();
		if (!accept)
			return false;

		if (letterInd_ + 1 < word_->size()) // size(S)=1
		{
			// T S -> TS
			// �'����� [P J A TS O T]
			if (offsetLetter(1) == Letter_S)
			{
				billetPhones_.push_back(newConsonantPhone("TS", SoftHardConsonant::Hard));
				letterInd_ += 1;
				return true;
			}
		}
		if (letterInd_ + 2 < word_->size()) // size(1 S)=2
		{
			// T 1 S -> TS
			// ������������� [T R Y M A T Y M E TS1 A]
			if (offsetLetter(1) == Letter_SoftSign && offsetLetter(2) == Letter_S)
			{
				// if �� -> TS TS
				billetPhones_.push_back(newConsonantPhone("TS", SoftHardConsonant::Soft));
				letterInd_ += 2;
				return true;
			}
		}
		if (letterInd_ + 1 < word_->size()) // size(TS)=1
		{
			// T TS -> TS TS
			// ����� [K L I TS TS I]
			if (offsetLetter(1) == Letter_TS)
			{
				billetPhones_.push_back(newConsonantPhone("TS", SoftHardConsonant::Hard));
				billetPhones_.push_back(newConsonantPhone("TS", SoftHardConsonant::Hard));
				letterInd_ += 1;
				return true;
			}
		}
		if (letterInd_ + 1 < word_->size()) // size(CH)=1
		{
			// T CH -> CH CH
			// ���� [O CH CH E]
			if (offsetLetter(1) == Letter_CH)
			{
				billetPhones_.push_back(newConsonantPhone("CH", SoftHardConsonant::Hard));
				billetPhones_.push_back(newConsonantPhone("CH", SoftHardConsonant::Hard));
				letterInd_ += 1;
				return true;
			}
		}
		return false;
	}

	bool WordPhoneticTranscriber::ruleSoftSign()
	{
		bool accept = curLetter() == Letter_SoftSign;
		if (!accept)
			return false;

		// soften the previous char
		if (!billetPhones_.empty())
		{
			PhoneBillet& ph = billetPhones_.back();
			bool ok = ph.DerivedFromChar == CharGroup::Consonant;
			if (!ok)
				return false; // Only consonant can be softened
			ph.SoftHard = SoftHardConsonant::Soft;
			return true;
		}
		return false;
	}

	bool WordPhoneticTranscriber::ruleApostrophe()
	{
		bool accept = curLetter() == Letter_Apostrophe;
		if (!accept)
			return false;

		// soften the previous char
		if (!billetPhones_.empty())
		{
			PhoneBillet& ph = billetPhones_.back();
			bool hardingConsonant = ph.DerivedFromChar == CharGroup::Consonant;
			if (hardingConsonant)
				ph.SoftHard = SoftHardConsonant::Hard;
			else
			{
				// apostrophe after vowel may be used in uk->en transliteration eg. "he's -> ��'�"
				// accept it; do no action
			}
			return true;
		}
		return false;
	}

	bool WordPhoneticTranscriber::ruleHardConsonantBeforeE()
	{
		bool accept = curLetter() == Letter_E && !billetPhones_.empty();
		if (!accept)
			return false;
		
		// Rule: the vowel E dictates that the previous consonant is always hard
		PhoneBillet& prevPh = billetPhones_.back();
		if (prevPh.DerivedFromChar == CharGroup::Consonant)
		{
			boost::optional<SoftHardConsonant> prevValue = prevPh.SoftHard;
			if (prevValue != nullptr)
			{
				bool ok = prevValue == SoftHardConsonant::Hard || prevValue == SoftHardConsonant::Palatal;
				PG_DbgAssert(ok && "hard and usually hard consonants can become hard");
			}
			prevPh.SoftHard = SoftHardConsonant::Hard;
			billetPhones_.push_back(newVowelPhone("E", nullptr));
			return true;
		}
		return false;
	}

	bool WordPhoneticTranscriber::ruleSoftConsonantBeforeI()
	{
		bool accept = curLetter() == Letter_I && !billetPhones_.empty();
		if (!accept)
			return false;
		
		// Rule: the vowel E dictates that the previous consonant is always hard
		PhoneBillet& prevPh = billetPhones_.back();
		if (prevPh.DerivedFromChar == CharGroup::Consonant)
		{
			if (usuallyHardBasicPhone(*phoneReg_, prevPh.BasicPhoneId))
			{
				// Consonants (B,P,V,M,F), (H,K,KH,G), (CH,DZH,SH,ZH) become palatilized (half-softened) before sound I.
				prevPh.SoftHard = SoftHardConsonant::Palatal;
			}
			else
			{
				// Rule: the vowel I dictates that the previous consonant is always soft
				prevPh.SoftHard = SoftHardConsonant::Soft;
			}
			billetPhones_.push_back(newVowelPhone("I", nullptr));
			return true;
		}
		return false;
	}
	

	bool WordPhoneticTranscriber::ruleDoubleJaJeJu()
	{
		wchar_t letter = curLetter();
		bool accept = letter == Letter_JE || letter == Letter_JU || letter == Letter_JA;
		if (!accept)
			return false;
		
		// Rule: JA -> J A when it is the first letter
		// ����� [J E V R E J]
		// ���� [J U N A K]
		// ������ [J A B L U K O]
		// Rule: JA -> J A when the previous letter is a vowel
		// ������ [V Z A J E M N O]
		// ������ [N A S T O J U]
		// ����� [A B Y J A K]
		// Rule: JA -> J A when the previous letter is the soft sign
		// �������[K O N1 J A K]
		// ����� [M O S1 J E]
		// ��� [N1 J U]
		// "����-���" [B U D1 J A K A]
		// Rule: JA -> J A when the previous letter is the apostrophe
		// ���'�� [B U R J A N]
		// ���'�� [K A R J E R]
		// ����'���� [K O M P J U T E R]
		auto dualPhone = [this]() ->bool {
			if (isFirstLetter())
				return true;

			// the first letter of a compound word
			wchar_t prevLetter = offsetLetter(-1);
			if (prevLetter == Letter_Hyphen)
				return true;

			//
			bool prevVowel = isUkrainianVowel(prevLetter);
			if (prevVowel)
				return true;
			
			bool prevSoftSign = prevLetter == Letter_SoftSign;
			if (prevSoftSign)
				return true;

			bool prevApostrophe = prevLetter == Letter_Apostrophe;
			if (prevApostrophe)
				return true;

			return false;
		};
		bool doublePhone = dualPhone();
		if (!doublePhone)
			return false;

		PhoneBillet curPh;
		if (!makePhoneOneToOne(letter, curPh)) // try to create current phone before modifying the collection
			return false;

		billetPhones_.push_back(newConsonantPhone("J", SoftHardConsonant::Hard));
		billetPhones_.push_back(curPh);
		return true;
	}

	bool WordPhoneticTranscriber::ruleSoftConsonantBeforeJaJeJu()
	{
		wchar_t letter = curLetter();
		bool accept = (letter == Letter_JU || letter == Letter_JA || letter == Letter_JE) && !billetPhones_.empty();
		if (!accept)
			return false;
		// ���� [L1 U D Y]
		// ������� [O L E K S1 U K]
		// ���� [B U R1 U]
		// ������ [L1 A K A T Y]
		// ���� [B U R1 A]
		// ������� [Z O R1 A N Y J]
		PhoneBillet& prevPh = billetPhones_.back();
		if (prevPh.DerivedFromChar == CharGroup::Consonant)
		{
			SoftHardConsonant softDegree;
			if (usuallyHardBasicPhone(*phoneReg_, prevPh.BasicPhoneId))
			{
				softDegree = SoftHardConsonant::Palatal;
			}
			else
			{
				// easily soft consonant
				softDegree = SoftHardConsonant::Soft;
			}
			prevPh.SoftHard = softDegree;

			// �� [L1 L1 E1]
			// ������ [S U T T E V O]
			// ������� [M O D E L1 L1 U]
			// ���� [I L1 L1 A]
			int prevPrevInd = (int)billetPhones_.size() - 2;
			if (prevPrevInd >= 0)
			{
				PhoneBillet& prevPrevPh = billetPhones_[prevPrevInd];
				if (prevPrevPh.BasicPhoneId == prevPh.BasicPhoneId)
					prevPrevPh.SoftHard = softDegree;
			}

			PhoneBillet curPh;
			if (makePhoneOneToOne(letter, curPh))
				return false;
			billetPhones_.push_back(curPh);
			return true;
		}
		return false;
	}

	bool WordPhoneticTranscriber::ruleDampVoicedConsonantBeforeUnvoiced()
	{
		wchar_t letter = curLetter();
		bool accept = (letter == Letter_B || letter == Letter_H || letter == Letter_D || letter == Letter_ZH || letter == Letter_Z) && !isLastLetter();
		if (!accept)
			return false;

		// B->P, H->KH, D->T, ZH->SH, Z->S before unvoiced sound
		// B->P ��������� [N E O P KH I D N O]
		// H->KH ��������� [D O P O M O KH T Y]
		// D->T ������ [SH V Y T K O]
		// ZH->SH ����� [D U SH CH E]
		// Z->S ������� [B E S P E K A]
		bool beforeUnvoiced = false;
		{
			wchar_t nextLetter = offsetLetter(1);
			if (isUnvoicedCharUk(nextLetter))
				beforeUnvoiced = true;
			else
			{
				// check if the next letter is a soft sign and only then is the unvoiced consonant
				if (nextLetter == Letter_SoftSign)
				{
					if (letterInd_ + 2 < word_->size())
					{
						wchar_t nextNextLetter = offsetLetter(2);
						if (isUnvoicedCharUk(nextNextLetter))
							beforeUnvoiced = true;
					}
				}
			}
		}
		if (beforeUnvoiced)
		{
			PhoneBillet ph;
			if (letter == Letter_B)
				ph = newConsonantPhone("P", SoftHardConsonant::Hard);
			else if (letter == Letter_H)
				ph = newConsonantPhone("KH", SoftHardConsonant::Hard);
			else if (letter == Letter_D)
				ph = newConsonantPhone("T", nullptr);
			else if (letter == Letter_ZH)
				ph = newConsonantPhone("SH", SoftHardConsonant::Hard);
			else if (letter == Letter_Z)
				ph = newConsonantPhone("S", nullptr);
			else PG_Assert(false);

			billetPhones_.push_back(ph);
			return true;
		}
		return false;
	}

	bool WordPhoneticTranscriber::ruleDefaultSimpleOneToOneMap()
	{
		// default char->phone mapping
		PhoneBillet ph;
		bool makeOp = makePhoneOneToOne(curLetter(), ph);
		if (!makeOp)
			return false;
		billetPhones_.push_back(ph);
		return true;
	}

	bool WordPhoneticTranscriber::isMutuallySoftConsonant(Phone::BasicPhoneIdT basicPhoneId) const
	{
		bool suc = true;
		bool foundMatch = false; // whether got the match
		auto matchPhone = [this, &suc, &foundMatch](Phone::BasicPhoneIdT expectBasicPhoneId, const std::string& queryBasicPhoneStr) -> void
		{
			if (!suc || foundMatch)
				return;
			Phone::BasicPhoneIdT basicId = phoneReg_->basicPhoneId(queryBasicPhoneStr, &suc);
			if (suc && expectBasicPhoneId == basicId)
				foundMatch = true;
		};
		matchPhone(basicPhoneId, "D");
		matchPhone(basicPhoneId, "DZ");
		matchPhone(basicPhoneId, "T");
		matchPhone(basicPhoneId, "TS");
		matchPhone(basicPhoneId, "Z");
		matchPhone(basicPhoneId, "S");
		matchPhone(basicPhoneId, "N");
		matchPhone(basicPhoneId, "L");
		PG_DbgAssert(suc);
		return foundMatch;
	}

	void WordPhoneticTranscriber::postRulePairOfConsonantsSoftenEachOther()
	{
		for (size_t i = billetPhones_.size() - 1; i > 0; --i)
		{
			const PhoneBillet& right = billetPhones_[i];
			PhoneBillet& left = billetPhones_[i - 1];
			if (left.DerivedFromChar == CharGroup::Consonant &&
				right.DerivedFromChar == CharGroup::Consonant &&
				right.SoftHard == SoftHardConsonant::Soft &&
				isMutuallySoftConsonant(left.BasicPhoneId) &&
				isMutuallySoftConsonant(right.BasicPhoneId))
			{
				left.SoftHard = SoftHardConsonant::Soft;
			}
		}
	}

	void WordPhoneticTranscriber::postRuleAmplifyUnvoicedConsonantBeforeVoiced()
	{
		// Rule: unvoiced consonant becomes voiced before another voiced consonant
		for (size_t phInd = billetPhones_.size() - 1; phInd > 0; phInd--)
		{
			PhoneBillet& prevPh = billetPhones_[phInd - 1];
			const PhoneBillet& ph = billetPhones_[phInd];

			if (isNoisyUnvoicedConsonant(*phoneReg_, prevPh.BasicPhoneId) && isNoisyVoicedConsonant(*phoneReg_, ph.BasicPhoneId))
			{
				boost::optional<PhoneRegistry::BasicPhoneIdT> partnerBasicPhoneId = getVoicedUnvoicedConsonantPair(*phoneReg_, prevPh.BasicPhoneId);
				PG_Assert(partnerBasicPhoneId != nullptr && "Can't find paired consonant");

				const BasicPhone* partnerBasicPhone = phoneReg_->basicPhone(partnerBasicPhoneId.get());

				PhoneBillet newPhone = newConsonantPhone(partnerBasicPhone->Name, prevPh.SoftHard);
				billetPhones_[phInd - 1] = newPhone;
			}
		}
	}

	bool WordPhoneticTranscriber::tryRecoverStressedVowel()
	{
		// rule: one vowel in a word is always stressed (not works with abbreviation eg USA)
		int numVowels = 0;
		PhoneBillet* lastPhone = nullptr;
		for (PhoneBillet& ph : billetPhones_)
		{
			if (ph.DerivedFromChar == CharGroup::Vowel)
			{
				numVowels++;
				lastPhone = &ph;
			}
		}
		if (numVowels == 1)
		{
			lastPhone->IsStressed = true;
			return true;
		}
		return false;
	}

	void WordPhoneticTranscriber::buildOutputPhones()
	{
		// build phoneIds sequnces from phone billets
		std::transform(billetPhones_.cbegin(), billetPhones_.cend(), std::back_inserter(outputPhones_), [this](const PhoneBillet& ph) -> PhoneId
		{
			boost::optional<PhoneId> phoneId = nullptr;
			if (ph.DerivedFromChar == CharGroup::Consonant)
			{
				boost::optional<SoftHardConsonant> softHard = ph.SoftHard;
				if (softHard == nullptr)
					softHard = phoneReg_->defaultSoftHardConsonant();
				phoneId = phoneReg_->phoneIdSingle(ph.BasicPhoneId, softHard, nullptr);
			}
			else if (ph.DerivedFromChar == CharGroup::Vowel)
			{
				boost::optional<bool> isStressed = ph.IsStressed;
				if (isStressed == nullptr)
					isStressed = phoneReg_->defaultIsVowelStressed();
				phoneId = phoneReg_->phoneIdSingle(ph.BasicPhoneId, nullptr, isStressed);
			}
			PG_Assert(phoneId != nullptr && "Can't map phone billet to phoneId");
			return phoneId.get();
		});
	}

	PhoneBillet WordPhoneticTranscriber::newConsonantPhone(const std::string& basicPhoneStr, boost::optional<SoftHardConsonant> SoftHard) const
	{
		bool success = false;
		BasicPhoneIdT basicId = phoneReg_->basicPhoneId(basicPhoneStr, &success);
		PG_Assert(success && "Unknown basic phone str");

		PhoneBillet billet;
		billet.BasicPhoneId = basicId;
		billet.DerivedFromChar = CharGroup::Consonant;
		billet.SoftHard = SoftHard;
		return billet;
	}

	PhoneBillet WordPhoneticTranscriber::newVowelPhone(const std::string& basicPhoneStr, boost::optional<bool> isStressed) const
	{
		bool success = false;
		BasicPhoneIdT basicId = phoneReg_->basicPhoneId(basicPhoneStr, &success);
		PG_Assert(success && "Unknown basic phone str");

		PhoneBillet billet;
		billet.BasicPhoneId = basicId;
		billet.DerivedFromChar = CharGroup::Vowel;
		billet.IsStressed = isStressed;
		return billet;
	}

	bool WordPhoneticTranscriber::makePhoneOneToOne(wchar_t letter, PhoneBillet& ph) const
	{
		switch (letter)
		{
		case L'�':
			ph = newVowelPhone("A", nullptr);
			break;
		case L'�':
			ph = newConsonantPhone("B", SoftHardConsonant::Hard);
			break;
		case L'�':
			ph = newConsonantPhone("V", SoftHardConsonant::Hard);
			break;
		case L'�':
			ph = newConsonantPhone("H", SoftHardConsonant::Hard);
			break;
		case L'�':
			ph = newConsonantPhone("G", SoftHardConsonant::Hard);
			break;
		case L'�':
			ph = newConsonantPhone("D", nullptr);
			break;
		case L'�':
		case L'�':
			ph=  newVowelPhone("E", nullptr);
			break;
		case L'�':
			ph = newConsonantPhone("ZH", SoftHardConsonant::Hard);
			break;
		case L'�':
			ph = newConsonantPhone("Z", nullptr);
			break;
		case L'�':
			ph = newVowelPhone("Y", nullptr);
			break;
		case L'�':
		case L'�':
			ph = newVowelPhone("I", nullptr);
			break;
		case L'�':
			ph = newConsonantPhone("J", SoftHardConsonant::Hard);
			break;
		case L'�':
			ph = newConsonantPhone("K", SoftHardConsonant::Hard);
			break;
		case L'�':
			ph = newConsonantPhone("L", nullptr);
			break;
		case L'�':
			ph = newConsonantPhone("M", SoftHardConsonant::Hard);
			break;
		case L'�':
			ph = newConsonantPhone("N", SoftHardConsonant::Hard);
			break;
		case L'�':
			ph = newVowelPhone("O", nullptr);
			break;
		case L'�':
			ph = newConsonantPhone("P", SoftHardConsonant::Hard);
			break;
		case L'�':
			ph = newConsonantPhone("R", nullptr);
			break;
		case L'�':
			ph = newConsonantPhone("S", nullptr);
			break;
		case L'�':
			ph = newConsonantPhone("T", nullptr);
			break;
		case L'�':
			ph = newVowelPhone("U", nullptr);
			break;
		case L'�':
			ph = newConsonantPhone("F", SoftHardConsonant::Hard);
			break;
		case L'�':
			ph = newConsonantPhone("KH", SoftHardConsonant::Hard);
			break;
		case L'�':
			ph = newConsonantPhone("TS", nullptr);
			break;
		case L'�':
			ph = newConsonantPhone("CH", SoftHardConsonant::Hard);
			break;
		case L'�':
		case L'�':
			ph = newConsonantPhone("SH", SoftHardConsonant::Hard);
			break;
		case L'�':
			ph = newVowelPhone("U", nullptr);
			break;
		case L'�':
			ph = newVowelPhone("A", nullptr);
			break;
		default:
			return false;
		}
		return true;
	}

	std::tuple<bool, const char*> spellWordUk(const PhoneRegistry& phoneReg, const std::wstring& word, std::vector<PhoneId>& phones)
	{
		WordPhoneticTranscriber phoneticTranscriber;
		phoneticTranscriber.transcribe(phoneReg, word);
		if (phoneticTranscriber.hasError())
			return std::make_tuple(false, "Can't transcribe word");
		phoneticTranscriber.copyOutputPhoneIds(phones);
		return std::make_tuple(true, nullptr);
	}

	void updatePhoneModifiers(const PhoneRegistry& phoneReg, bool keepConsonantSoftness, bool keepVowelStress, std::vector<PhoneId>& phonesList)
	{
		for (size_t i = 0; i < phonesList.size(); ++i)
		{
			PhoneId phoneId = phonesList[i];

			const Phone* oldPhone = phoneReg.phoneById(phoneId);
			const BasicPhone* basicPhone = phoneReg.basicPhone(oldPhone->BasicPhoneId);

			if (!keepConsonantSoftness && basicPhone->DerivedFromChar == CharGroup::Consonant)
			{
				if (oldPhone->SoftHard == SoftHardConsonant::Soft)
				{
					boost::optional<PhoneId> newPhoneId = phoneReg.phoneIdSingle(oldPhone->BasicPhoneId, SoftHardConsonant::Hard, nullptr);
					PG_DbgAssert(newPhoneId != nullptr);
					phonesList[i] = newPhoneId.get();
					continue;
				}
			}
			if (!keepVowelStress && basicPhone->DerivedFromChar == CharGroup::Vowel)
			{
				if (oldPhone->IsStressed == true)
				{
					boost::optional<PhoneId> newPhoneId = phoneReg.phoneIdSingle(oldPhone->BasicPhoneId, nullptr, false);
					PG_DbgAssert(newPhoneId != nullptr);
					phonesList[i] = newPhoneId.get();
					continue;
				}
			}
		}
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

	void UkrainianPhoneticSplitter::bootstrap(const std::unordered_map<std::wstring, std::unique_ptr<WordDeclensionGroup>>& words, const std::wstring& targetWord, const std::unordered_set<std::wstring>& processedWords)
	{
		for (const auto& pair : words)
		{
			const WordDeclensionGroup& wordGroup = *pair.second;
			bool contains = processedWords.find(wordGroup.Name) != processedWords.end();
			if (false && !contains)
				continue;

			if (!targetWord.empty() && wordGroup.Name != targetWord)
				continue;

			if (!allowPhoneticWordSplit_)
			{
				// put all word declination forms as whole word parts
				for (const WordDeclensionForm& declWord : wordGroup.Forms)
				{
					if (declWord.isNotAvailable()) continue;
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

	void UkrainianPhoneticSplitter::gatherWordPartsSequenceUsage(const wchar_t* textFilesDir, long& totalPreSplitWords, int maxFileToProcess, bool outputCorpus)
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