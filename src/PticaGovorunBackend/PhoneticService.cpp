#include "stdafx.h"
#include "PhoneticService.h"
#include <array>
#include <QDirIterator>
#include <QXmlStreamReader>
#include <QString>
#include "CoreUtils.h"
#include <utility>
#include "assertImpl.h"

namespace PticaGovorun
{
	void toString(SoftHardConsonant value, std::string& result)
	{
		switch (value)
		{
		case SoftHardConsonant::Hard:
			result = "Hard";
			break;
		case SoftHardConsonant::Palatal:
			result = "Palatal";
			break;
		case SoftHardConsonant::Soft:
			result = "Soft";
			break;
		default:
			PG_Assert(false);
		}
	}

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
		PG_Assert2(toStrOp, "Invalid PhoneId:int");
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

		PG_Assert(basicPhoneStrInd >= 0 && basicPhoneStrInd < basicPhones_.size());
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
			{
				PhoneId pid = extendPhoneId(phoneInd + 1);
				phoneIds.push_back(pid);
			}
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
			return boost::none;
		return phoneIdSingle(basicPhId, softHard, isStressed);
	}

	boost::optional<PhoneId> PhoneRegistry::phoneIdSingle(PhoneRegistry::BasicPhoneIdT basicPhoneStrId, boost::optional<SoftHardConsonant> softHardRequested, boost::optional<bool> isStressed) const
	{
		boost::optional<PhoneId> t = boost::none;
		const BasicPhone* basicPh = basicPhone(basicPhoneStrId);
		if (basicPh == nullptr)
			return boost::none;
		static std::vector<PhoneId> candidates;
		candidates.clear();
		findPhonesByBasicPhoneStr(basicPh->Name, candidates);

		if (candidates.empty())
			return boost::none;
		if (candidates.size() == 1)
			return candidates.front();

		boost::optional<SoftHardConsonant> softHardFixed = softHardRequested;
		if (softHardRequested != boost::none)
		{
			if (allowSoftHardConsonant())
			{
				if (softHardRequested == SoftHardConsonant::Palatal)
				{
					     if (palatalSupport() == PalatalSupport::AsHard)
						softHardFixed = SoftHardConsonant::Hard; // B2 -> B, L2 -> L
					else if (palatalSupport() == PalatalSupport::AsSoft)
						softHardFixed = SoftHardConsonant::Soft; // B2 -> B1, L2 -> L1
					else if (palatalSupport() == PalatalSupport::AsPalatal)
						softHardFixed = SoftHardConsonant::Palatal; // B2 -> B2, L2 -> L2
				}
				else if (softHardRequested == SoftHardConsonant::Soft)
				{
					if (usuallyHardBasicPhone(*this, basicPhoneStrId))
					{
						if (palatalSupport() == PalatalSupport::AsHard)
						{
							softHardFixed = SoftHardConsonant::Hard; // B1 -> B
						}
						else if (palatalSupport() == PalatalSupport::AsSoft)
						{
							softHardFixed = SoftHardConsonant::Soft; // B1 -> B1
						}
						else if (palatalSupport() == PalatalSupport::AsPalatal)
						{
							softHardFixed = SoftHardConsonant::Palatal; // B1 -> B2
						}
					}
				}
			}
		}

		std::vector<PhoneId> excactCandidates;
		std::remove_copy_if(candidates.begin(), candidates.end(), std::back_inserter(excactCandidates), [this, softHardFixed, isStressed](PhoneId phoneId)
		{
			const Phone* phone = phoneById(phoneId);
			if (softHardFixed != boost::none && softHardFixed != phone->SoftHard)
				return true;
			if (isStressed != boost::none && isStressed != phone->IsStressed)
				return true;
			return false;
		});

		if (excactCandidates.size() == 1)
			return excactCandidates.front();

		// exact candidates size:
		// = 0 then we may fallback to basic phone candidates; but there is no way to choose the best one
		// > 1 then again the best one can't be chosen
		return boost::none;
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
		return boost::none;
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

		// TODO: silence is not a vowel
		phoneReg.newVowelPhone("SIL", false); // SIL
		//phoneReg.newVowelPhone("SWL", false); // silence with noise
		phoneReg.newVowelPhone("INH", false); // inhale
		//phoneReg.newConsonantPhone("CLK", SoftHardConsonant::Hard); // click sound (such as pressed key)
		phoneReg.newVowelPhone("EEE", false); // filler; gives a speaker the time to think about her answer
		phoneReg.newVowelPhone("YYY", false); // filler; gives a speaker the time to think about her answer

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
		boost::optional<Phone::BasicPhoneIdT> partnerBasicPhoneId = boost::none;
		auto match = [&suc, &partnerBasicPhoneId, &phoneReg](Phone::BasicPhoneIdT expectBasicPhoneId, const std::string& basicStr, const std::string& dualBasicStr) -> void
		{
			if (!suc || partnerBasicPhoneId != boost::none)
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

#ifdef PG_HAS_JULIUS
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
#endif

	void parsePronId(boost::wstring_view pronId, boost::wstring_view& pronName)
	{
		size_t openBraceInd = pronId.find(L'(');
		if (openBraceInd == boost::wstring_view::npos)
		{
			pronName = pronId;
			return;
		}
		pronName = pronId.substr(0, openBraceInd);
	}

	bool isWordStressAssigned(const PhoneRegistry& phoneReg, const std::vector<PhoneId>& phoneIds)
	{
		int numVowels = 0;
		int numStressedVowels = 0;
		for (PhoneId phoneId : phoneIds)
		{
			const Phone* phone = phoneReg.phoneById(phoneId);
			if (phone->IsStressed == true)
				numStressedVowels++;
			const BasicPhone* basicPhone = phoneReg.basicPhone(phone->BasicPhoneId);
			if (basicPhone->DerivedFromChar == CharGroup::Vowel)
				numVowels++;
		}
		return numStressedVowels > 0 || numVowels == 0;
	}

	bool isPronCodeDefinesStress(boost::wstring_view pronCode)
	{
		return pronCode.ends_with(L')');
	}

	bool parsePronCodeNameAndStress(boost::wstring_view pronCode, boost::wstring_view* pronCodeName, boost::wstring_view* pronCodeStressSuffix)
	{
		size_t closeParInd = pronCode.find_last_of(L')');
		if (closeParInd == boost::wstring_view::npos)
		{
			*pronCodeName = pronCode;
			*pronCodeStressSuffix = boost::wstring_view();
			return true;
		}

		size_t openParInd = pronCode.find_last_of(L'(');
		if (openParInd == boost::wstring_view::npos)
			return false; // invalid pronCode format

		const wchar_t* numStr = pronCode.data() + openParInd + 1;
		int stress = -1;
		int numRead = swscanf(numStr, L"%d)", &stress);
		if (numRead != 1) // single digit in parenthesis
			return false; // can't parse number
		
		*pronCodeName = boost::wstring_view(pronCode.data(), openParInd);
		*pronCodeStressSuffix = boost::wstring_view(numStr, closeParInd - openParInd-1);
		return true;
	}

	std::tuple<bool, const char*> loadPhoneticDictionaryPronIdPerLine(const std::basic_string<wchar_t>& vocabFilePathAbs, const PhoneRegistry& phoneReg,
		const QTextCodec& textCodec, std::vector<PhoneticWord>& words, std::vector<std::string>& brokenLines,
		GrowOnlyPinArena<wchar_t>& stringArena)
	{
		// file contains text in Windows-1251 encoding
		QFile file(QString::fromStdWString(vocabFilePathAbs));
		if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
			return std::make_tuple(false, "Can't open file");

		// 
		std::array<char, 1024> lineBuff;
		std::array<wchar_t, 64> wordBuff;
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

			boost::string_view line(lineBuff.data(), readBytes);
			if (line.back() == '\n')
				line.remove_suffix(1);

			const char* DictDelim = " \t\n";
			size_t phoneListInd = line.find_first_of(DictDelim);
			if (phoneListInd == boost::string_view::npos)
				return std::make_tuple(false, "The word is too long (>1024 bytes)");

			int wordSize = phoneListInd;
			if (wordSize == 0) // empty line
				continue;

			boost::string_view phoneListRef;
			auto isBrokenLine = [&]() -> bool
			{
				if (wordSize > wordBuff.size()) // word buff can't fit the word
					return true;

				phoneListRef = line.substr(phoneListInd + 1);
				if (phoneListRef.empty()) // word without the list of phones
					return true;

				// parsing requires phone list to be uppercase
				std::transform(phoneListRef.begin(), phoneListRef.end(), (char*)phoneListRef.begin(), toupper);

				phones.clear();
				bool parseOp = parsePhoneList(phoneReg, phoneListRef, phones);
				if (!parseOp)
					return true;
				return false;
			};

			if (isBrokenLine())
			{
				brokenLines.push_back(lineBuff.data());
				continue;
			}

			QString word = textCodec.toUnicode(line.data(), phoneListInd);
			int copiedSize = word.toWCharArray(wordBuff.data());
			PG_DbgAssert(copiedSize == word.size());

			boost::wstring_view wordRef = boost::wstring_view(wordBuff.data(), word.size());
			boost::wstring_view arenaWordRef;
			if (!registerWord(wordRef, stringArena, arenaWordRef))
			{
				brokenLines.push_back(lineBuff.data());
				continue;
			}

			if (curPronGroup.Word.compare(wordRef) != 0)
			{
				// finish previous pronunciation group
				if (!curPronGroup.Word.empty())
				{
					words.push_back(curPronGroup);
					curPronGroup.Word.clear();
					curPronGroup.Pronunciations.clear();
				}

				// start new group
				PG_DbgAssert2(curPronGroup.Pronunciations.empty(), "Old pronunciation data must be purged");
				curPronGroup.Word = arenaWordRef;
			}

			PronunciationFlavour pron;
			pron.PronCode = arenaWordRef;
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

	boost::optional<PhoneId> parsePhoneStr(const PhoneRegistry& phoneReg, boost::string_view phoneStrRef)
	{
		if (phoneStrRef.empty())
			return boost::none;

		boost::string_view basicPhoneStrRef = phoneStrRef;

		// truncate last digit
		char ch = phoneStrRef.back();
		bool lastIsDigit = ::isdigit(ch);
		if (lastIsDigit)
			basicPhoneStrRef.remove_suffix(1);

		bool basicOp = false;
		std::string basicPhoneStr = std::string(basicPhoneStrRef.data(), basicPhoneStrRef.size());
		PhoneRegistry::BasicPhoneIdT basicPhoneId = phoneReg.basicPhoneId(basicPhoneStr, &basicOp);
		if (!basicOp)
			return boost::none;
		const BasicPhone* basicPhone = phoneReg.basicPhone(basicPhoneId);

		// defaults for phone name without a number suffix
		boost::optional<SoftHardConsonant> softHard = boost::none;
		if (basicPhone->DerivedFromChar == CharGroup::Consonant)
			softHard = phoneReg.defaultSoftHardConsonant();

		boost::optional<bool> isStressed = boost::none;
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
				else return boost::none; // unknown number modifier
			}

			// number on a vowel derived phone means stress
			else if (basicPhone->DerivedFromChar == CharGroup::Vowel)
				isStressed = true;
		}

		boost::optional<PhoneId> result = phoneReg.phoneIdSingle(basicPhoneId, softHard, isStressed);
		return result;
	}

	bool parsePhoneList(const PhoneRegistry& phoneReg, boost::string_view phoneListStr, std::vector<PhoneId>& result)
	{
		boost::string_view curPhonesStr = phoneListStr;
		while (!curPhonesStr.empty())
		{
			size_t sepPos = curPhonesStr.find(' ');
			if (sepPos == boost::string_view::npos)
				sepPos = curPhonesStr.size();

			boost::string_view phoneRef = curPhonesStr.substr(0, sepPos);
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

	std::tuple<bool, const char*> parsePronuncLinesNew(const PhoneRegistry& phoneReg, const std::wstring& prons, std::vector<PronunciationFlavour>& result, GrowOnlyPinArena<wchar_t>& stringArena)
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
			boost::wstring_view arenaPronAsWord;
			if (!registerWord(pronAsWord, stringArena, arenaPronAsWord))
				return std::make_tuple(false, "Can't allocate word");

			//
			QString phonesStr = pronLine.mid(pronAsWordEndInd + 1);

			std::vector<PhoneId> phones;
			bool parseOp = parsePhoneList(phoneReg, phonesStr.toStdString(), phones);
			if (!parseOp)
				return std::make_tuple(false, "Can't parse the list of phones");

			PronunciationFlavour pron;
			pron.PronCode = arenaPronAsWord;
			pron.Phones = std::move(phones);
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

	bool WordPhoneticTranscriber::hasError() const
	{
		return !errString_.empty();
	}

	const std::wstring& WordPhoneticTranscriber::errorString() const
	{
		return errString_;
	}

	void WordPhoneticTranscriber::tryInitStressedVowels()
	{
		isLetterStressed_.resize(word_->size());
		std::fill(isLetterStressed_.begin(), isLetterStressed_.end(), (char)-1);

		int stressedCharInd = -1;
		if (getStressedVowelCharIndAtMostOne(*word_, stressedCharInd) && stressedCharInd != -1)
		{
			isLetterStressed_[stressedCharInd] = (char)true;
			return;
		}

		// fallback to dictionary lookup
		std::vector<int> stressedSyllables;
		if (stressedSyllableIndFun_ != nullptr && stressedSyllableIndFun_(*word_, stressedSyllables))
		{
			// set all vowels unstressed
			for (size_t charInd = 0; charInd < word_->size(); ++charInd)
			{
				if (isUkrainianVowel((*word_)[charInd]))
					isLetterStressed_[charInd] = (char)false;
			}
			for (int stressedSyllable : stressedSyllables)
			{
				int charInd = syllableIndToVowelCharIndUk(*word_, stressedSyllable);
				if (charInd == -1)
				{
					errString_ = L"Invalid stressed syllable was provided";
					return;
				}
				isLetterStressed_[charInd] = (char)true;
			}
		}
	}

	void WordPhoneticTranscriber::transcribe(const PhoneRegistry& phoneReg, const std::wstring& word)
	{
		PG_Assert(&phoneReg != nullptr);
		phoneReg_ = &phoneReg;
		word_ = &word;
		errString_.clear();
		billetPhones_.clear();
		outputPhones_.clear();
		phoneIndToLetterInd_.clear();
		isLetterStressed_.clear();

		tryInitStressedVowels();
		if (hasError())
			return;

		for (letterInd_ = 0; letterInd_ < word.size(); ++letterInd_)
		{
			if (hasError())
				return;
			
			bool anyRuleWasApplied = false;

			auto tryRule = [this, &anyRuleWasApplied](decltype(&WordPhoneticTranscriber::ruleIgnore) ruleFun) -> void
			{
				if (hasError() || anyRuleWasApplied)
					return;
				anyRuleWasApplied = (this->*ruleFun)();
			};
			tryRule(&WordPhoneticTranscriber::ruleIgnore);
			tryRule(&WordPhoneticTranscriber::ruleJi);
			tryRule(&WordPhoneticTranscriber::ruleShCh);
			tryRule(&WordPhoneticTranscriber::ruleDzDzh);
			tryRule(&WordPhoneticTranscriber::ruleZhDzh);
			tryRule(&WordPhoneticTranscriber::ruleNtsk);
			tryRule(&WordPhoneticTranscriber::ruleSShEtc);
			tryRule(&WordPhoneticTranscriber::ruleTsEtc);
			tryRule(&WordPhoneticTranscriber::ruleSoftSign);
			tryRule(&WordPhoneticTranscriber::ruleApostrophe);
			tryRule(&WordPhoneticTranscriber::ruleHardConsonantBeforeE);
			tryRule(&WordPhoneticTranscriber::ruleSoftConsonantBeforeI);
			tryRule(&WordPhoneticTranscriber::ruleDoubleJaJeJu);
			tryRule(&WordPhoneticTranscriber::ruleSoftConsonantBeforeJaJeJu);
			tryRule(&WordPhoneticTranscriber::ruleDampVoicedConsonantBeforeUnvoiced);
			tryRule(&WordPhoneticTranscriber::ruleDefaultSimpleOneToOneMap);

			if (!anyRuleWasApplied && 
				errString_.empty()) // avoid overwriting existing error
			{
				wchar_t letter = word[letterInd_];
				errString_ = std::wstring(L"Unknown letter ") + letter;
				return;
			}
		}
		
		postRulePairOfConsonantsSoftenEachOther();
		postRuleAmplifyUnvoicedConsonantBeforeVoiced();

		buildOutputPhones();
	}

	void WordPhoneticTranscriber::copyOutputPhoneIds(std::vector<PhoneId>& phoneIds) const
	{
		std::copy(outputPhones_.begin(), outputPhones_.end(), std::back_inserter(phoneIds));
	}

	void WordPhoneticTranscriber::setStressedSyllableIndFun(decltype(stressedSyllableIndFun_) stressedSyllableFun)
	{
		stressedSyllableIndFun_ = stressedSyllableFun;
	}

	wchar_t WordPhoneticTranscriber::curLetter() const
	{
		return (*word_)[letterInd_];
	}

	boost::optional<bool> WordPhoneticTranscriber::isCurVowelStressed() const
	{
		wchar_t letter = curLetter();
		PG_DbgAssert(isUkrainianVowel(letter));
		char isStressed = isLetterStressed_[letterInd_];
		return isStressed == (char)-1 ? nullptr : (bool)isStressed;
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
		addPhone(newConsonantPhone("J", SoftHardConsonant::Hard));
		addPhone(newVowelPhone("I", isCurVowelStressed()));
		return true;
	}
	
	bool WordPhoneticTranscriber::ruleShCh()
	{
		bool accept = curLetter() == Letter_SHCH;
		if (!accept)
			return false;

		addPhone(newConsonantPhone("SH", SoftHardConsonant::Hard));
		addPhone(newConsonantPhone("CH", SoftHardConsonant::Hard));
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
			addPhone(newConsonantPhone("DZ", boost::none));
			letterInd_ += 1; // skip next letter
			return true;
		}
		else if (nextLetter == Letter_ZH)
		{
			addPhone(newConsonantPhone("DZH", SoftHardConsonant::Hard));
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
			addPhone(newConsonantPhone("ZH", SoftHardConsonant::Hard));
			addPhone(newConsonantPhone("ZH", SoftHardConsonant::Hard));
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
				addPhone(newConsonantPhone("ZH", SoftHardConsonant::Hard));
				addPhone(newConsonantPhone("DZH", SoftHardConsonant::Hard));
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
				addPhone(newConsonantPhone("N", SoftHardConsonant::Hard));
				addPhone(newConsonantPhone("S", SoftHardConsonant::Hard));
				addPhone(newConsonantPhone("T", SoftHardConsonant::Hard));
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
				addPhone(newConsonantPhone("N", SoftHardConsonant::Hard));
				addPhone(newConsonantPhone("S", SoftHardConsonant::Soft));
				addPhone(newConsonantPhone("K", SoftHardConsonant::Hard));
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
				addPhone(newConsonantPhone("SH", SoftHardConsonant::Hard));
				addPhone(newConsonantPhone("SH", SoftHardConsonant::Hard));
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
				addPhone(newConsonantPhone("Z", SoftHardConsonant::Hard));
				addPhone(newConsonantPhone("D", SoftHardConsonant::Hard));
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
				addPhone(newConsonantPhone("S", SoftHardConsonant::Soft));
				addPhone(newConsonantPhone("K", SoftHardConsonant::Hard));
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
				addPhone(newConsonantPhone("S", SoftHardConsonant::Hard));
				addPhone(newConsonantPhone("S", SoftHardConsonant::Hard));
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
				addPhone(newConsonantPhone("S", SoftHardConsonant::Hard));
				addPhone(newConsonantPhone("TS", SoftHardConsonant::Hard));
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
				addPhone(newConsonantPhone("TS", SoftHardConsonant::Hard));
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
				addPhone(newConsonantPhone("TS", SoftHardConsonant::Soft));
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
				addPhone(newConsonantPhone("TS", SoftHardConsonant::Hard));
				addPhone(newConsonantPhone("TS", SoftHardConsonant::Hard));
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
				addPhone(newConsonantPhone("CH", SoftHardConsonant::Hard));
				addPhone(newConsonantPhone("CH", SoftHardConsonant::Hard));
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
			if (prevValue != boost::none)
			{
				
				bool ok = prevValue == SoftHardConsonant::Hard || prevValue == SoftHardConsonant::Palatal;
				if (!ok)
				{
					// for eg for Russian word "����"
					errString_ = L"hard and usually hard consonants can become hard";
					return false;
				}
			}
			prevPh.SoftHard = SoftHardConsonant::Hard;
			addPhone(newVowelPhone("E", isCurVowelStressed()));
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
			addPhone(newVowelPhone("I", isCurVowelStressed()));
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
		if (!makePhoneFromCurLetterOneToOne(curPh)) // try to create current phone before modifying the collection
			return false;

		addPhone(newConsonantPhone("J", SoftHardConsonant::Hard));
		addPhone(curPh);
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
			if (makePhoneFromCurLetterOneToOne(curPh))
				return false;
			addPhone(curPh);
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
				ph = newConsonantPhone("T", boost::none);
			else if (letter == Letter_ZH)
				ph = newConsonantPhone("SH", SoftHardConsonant::Hard);
			else if (letter == Letter_Z)
				ph = newConsonantPhone("S", boost::none);
			else PG_Assert(false);

			addPhone(ph);
			return true;
		}
		return false;
	}

	bool WordPhoneticTranscriber::ruleDefaultSimpleOneToOneMap()
	{
		// default char->phone mapping
		PhoneBillet ph;
		bool makeOp = makePhoneFromCurLetterOneToOne(ph);
		if (!makeOp)
			return false;
		addPhone(ph);
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
		for (int i = static_cast<int>(billetPhones_.size()) - 1; i > 0; --i)
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
		for (int phInd = static_cast<int>(billetPhones_.size()) - 1; phInd > 0; phInd--)
		{
			PhoneBillet& prevPh = billetPhones_[phInd - 1];
			const PhoneBillet& ph = billetPhones_[phInd];

			if (isNoisyUnvoicedConsonant(*phoneReg_, prevPh.BasicPhoneId) && isNoisyVoicedConsonant(*phoneReg_, ph.BasicPhoneId))
			{
				boost::optional<PhoneRegistry::BasicPhoneIdT> partnerBasicPhoneId = getVoicedUnvoicedConsonantPair(*phoneReg_, prevPh.BasicPhoneId);
				PG_Assert2(partnerBasicPhoneId != boost::none, "Can't find paired consonant");

				const BasicPhone* partnerBasicPhone = phoneReg_->basicPhone(partnerBasicPhoneId.get());

				PhoneBillet newPhone = newConsonantPhone(partnerBasicPhone->Name, prevPh.SoftHard);
				billetPhones_[phInd - 1] = newPhone;
			}
		}
	}

	void WordPhoneticTranscriber::buildOutputPhones()
	{
		// build phoneIds sequnces from phone billets
		for (size_t billetInd = 0; billetInd < billetPhones_.size(); ++billetInd)
		{
			const PhoneBillet& ph = billetPhones_[billetInd];
			boost::optional<PhoneId> phoneId = boost::none;
			if (ph.DerivedFromChar == CharGroup::Consonant)
			{
				boost::optional<SoftHardConsonant> softHard = ph.SoftHard;
				if (softHard == boost::none)
					softHard = phoneReg_->defaultSoftHardConsonant();
				phoneId = phoneReg_->phoneIdSingle(ph.BasicPhoneId, softHard, boost::none);
			}
			else if (ph.DerivedFromChar == CharGroup::Vowel)
			{
				boost::optional<bool> isStressed = ph.IsStressed;
				if (isStressed == boost::none)
					isStressed = phoneReg_->defaultIsVowelStressed();
				phoneId = phoneReg_->phoneIdSingle(ph.BasicPhoneId, boost::none, isStressed);
			}
			if (phoneId == boost::none)
			{
				std::wstring phStr;
				phoneBilletToStr(ph, phStr);
				errString_ = std::wstring(L"Can't map phone billet to phoneId. ") + phStr;
				return;
			}
			outputPhones_.push_back(phoneId.get());
		}
	}

	PhoneBillet WordPhoneticTranscriber::newConsonantPhone(const std::string& basicPhoneStr, boost::optional<SoftHardConsonant> SoftHard) const
	{
		bool success = false;
		BasicPhoneIdT basicId = phoneReg_->basicPhoneId(basicPhoneStr, &success);
		PG_Assert2(success, "Unknown basic phone str");

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
		PG_Assert2(success, "Unknown basic phone str");

		PhoneBillet billet;
		billet.BasicPhoneId = basicId;
		billet.DerivedFromChar = CharGroup::Vowel;
		billet.IsStressed = isStressed;
		return billet;
	}

	void WordPhoneticTranscriber::addPhone(const PhoneBillet& phone)
	{
		if (phone.DerivedFromChar == CharGroup::Vowel)
			phoneIndToLetterInd_[billetPhones_.size()] = letterInd_;
		billetPhones_.push_back(phone);
	}

	void WordPhoneticTranscriber::phoneBilletToStr(const PhoneBillet& phone, std::wstring& result) const
	{
		std::wstringstream buf;
		const BasicPhone* basicPhone = phoneReg_->basicPhone(phone.BasicPhoneId);
		if (basicPhone != nullptr)
			buf << QString::fromStdString(basicPhone->Name).toStdWString();
		if (phone.SoftHard != boost::none)
		{
			std::string softHardStr;
			toString(phone.SoftHard.get(), softHardStr);
			buf << L" soft=" << QString::fromStdString(softHardStr).toStdWString();
		}
		if (phone.IsStressed != boost::none)
			buf << L" isStressed=" << phone.IsStressed.get();
		result = buf.str();
	}

	int WordPhoneticTranscriber::getVowelLetterInd(int vowelPhoneInd) const
	{
		auto it = phoneIndToLetterInd_.find(vowelPhoneInd);
		if (it != phoneIndToLetterInd_.end())
			return it -> second;
		return -1;
	}

	bool mapLetterToBasicPhoneInfo(wchar_t letter, boost::string_view& basicPhoneStr, CharGroup& charGroup)
	{
		auto init = [&basicPhoneStr, &charGroup](const char* basicPhoneCStr, CharGroup group)
		{
			basicPhoneStr = basicPhoneCStr;
			charGroup = group;
		};
		switch (letter)
		{
		case L'�':
			init("A", CharGroup::Vowel); break;
		case L'�':
			init("B", CharGroup::Consonant); break;
		case L'�':
			init("V", CharGroup::Consonant); break;
		case L'�':
			init("H", CharGroup::Consonant); break;
		case L'�':
			init("G", CharGroup::Consonant); break;
		case L'�':
			init("D", CharGroup::Consonant); break;
		case L'�':
		case L'�':
			init("E", CharGroup::Vowel); break;
		case L'�':
			init("ZH", CharGroup::Consonant); break;
		case L'�':
			init("Z", CharGroup::Consonant); break;
		case L'�':
			init("Y", CharGroup::Vowel); break;
		case L'�':
		case L'�':
			init("I", CharGroup::Vowel); break;
		case L'�':
			init("J", CharGroup::Consonant); break;
		case L'�':
			init("K", CharGroup::Consonant); break;
		case L'�':
			init("L", CharGroup::Consonant); break;
		case L'�':
			init("M", CharGroup::Consonant); break;
		case L'�':
			init("N", CharGroup::Consonant); break;
		case L'�':
			init("O", CharGroup::Vowel); break;
		case L'�':
			init("P", CharGroup::Consonant); break;
		case L'�':
			init("R", CharGroup::Consonant); break;
		case L'�':
			init("S", CharGroup::Consonant); break;
		case L'�':
			init("T", CharGroup::Consonant); break;
		case L'�':
			init("U", CharGroup::Vowel); break;
		case L'�':
			init("F", CharGroup::Consonant); break;
		case L'�':
			init("KH", CharGroup::Consonant); break;
		case L'�':
			init("TS", CharGroup::Consonant); break;
		case L'�':
			init("CH", CharGroup::Consonant); break;
		case L'�':
		case L'�':
			init("SH", CharGroup::Consonant); break;
		case L'�':
			init("U", CharGroup::Vowel); break;
		case L'�':
			init("A", CharGroup::Vowel); break;
		default:
			return false;
		}
		return true;
	}

	bool WordPhoneticTranscriber::makePhoneFromCurLetterOneToOne(PhoneBillet& ph) const
	{
		wchar_t letter = curLetter();

		boost::string_view basicPhoneStr;
		CharGroup charGroup;
		if (!mapLetterToBasicPhoneInfo(letter, basicPhoneStr, charGroup))
			return false;

		bool success = false;
		BasicPhoneIdT basicId = phoneReg_->basicPhoneId(std::string(basicPhoneStr.data(), basicPhoneStr.size()), &success);
		if (!success)
			return false;

		boost::optional<SoftHardConsonant> softHard = boost::none;
		if (charGroup == CharGroup::Consonant)
			softHard = SoftHardConsonant::Hard;

		boost::optional<bool> isStressed = boost::none;
		if (charGroup == CharGroup::Vowel)
			isStressed = isCurVowelStressed();

		PhoneBillet billet;
		billet.BasicPhoneId = basicId;
		billet.DerivedFromChar = charGroup;
		billet.SoftHard = softHard;
		billet.IsStressed = isStressed;
		ph = billet;

		return true;
	}

	std::tuple<bool, const char*> spellWordUk(const PhoneRegistry& phoneReg, const std::wstring& word, std::vector<PhoneId>& phones,
		WordPhoneticTranscriber::StressedSyllableIndFunT stressedSyllableIndFun)
	{
		WordPhoneticTranscriber phoneticTranscriber;
		phoneticTranscriber.setStressedSyllableIndFun(stressedSyllableIndFun);
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
					boost::optional<PhoneId> newPhoneId = phoneReg.phoneIdSingle(oldPhone->BasicPhoneId, SoftHardConsonant::Hard, boost::none);
					PG_DbgAssert(newPhoneId != boost::none);
					phonesList[i] = newPhoneId.get();
					continue;
				}
			}
			if (!keepVowelStress && basicPhone->DerivedFromChar == CharGroup::Vowel)
			{
				if (oldPhone->IsStressed == true)
				{
					boost::optional<PhoneId> newPhoneId = phoneReg.phoneIdSingle(oldPhone->BasicPhoneId, boost::none, false);
					PG_DbgAssert(newPhoneId != boost::none);
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
					PG_Assert2(false, "actual suffix size must be <= of possible suffix size");
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
					PG_Assert2(false, "Duplicate suffixs");
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

	void populatePronCodes(const std::vector<PhoneticWord>& phoneticDict, std::map<boost::wstring_view, PronunciationFlavour>& pronCodeToObj, std::vector<boost::wstring_view>& duplicatePronCodes)
	{
		for (const PhoneticWord& word : phoneticDict)
		{
			for (const PronunciationFlavour& pron : word.Pronunciations)
			{
				const auto& code = pron.PronCode;
				auto it = pronCodeToObj.find(code);
				if (it != pronCodeToObj.end())
					duplicatePronCodes.push_back(code);
				else
					pronCodeToObj.insert({ code, pron });
			}
		}
	}

	void mergePhoneticDictOnlyNew(std::map<boost::wstring_view, PhoneticWord>& basePhoneticDict, const std::vector<PhoneticWord>& extraPhoneticDict)
	{
		for (const PhoneticWord& extraWord : extraPhoneticDict)
		{
			boost::wstring_view wordRef = extraWord.Word;
			auto wordIt = basePhoneticDict.find(wordRef);
			if (wordIt == basePhoneticDict.end())
			{
				// new word; add the whole word with all prons
				basePhoneticDict.insert({ wordRef, extraWord });
				continue;
			}

			// existing word; try to integrate new prons into it
			PhoneticWord& baseWord = wordIt->second;
			for (const PronunciationFlavour& extraPron : extraWord.Pronunciations)
			{
				auto matchedPronIt = std::find_if(baseWord.Pronunciations.begin(), baseWord.Pronunciations.end(), [&extraPron](PronunciationFlavour& p)
				{
					return p.PronCode == extraPron.PronCode;
				});
				bool duplicatePron = matchedPronIt != baseWord.Pronunciations.end();
				if (duplicatePron)
					continue; // pron with the same code already exist
				baseWord.Pronunciations.push_back(extraPron);
			}
		}
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

			if (wordClass != boost::none)
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

	boost::wstring_view fillerSilence()
	{
		return L"<sil>";
	}

	boost::wstring_view fillerStartSilence()
	{
		return L"<s>";
	}

	boost::wstring_view fillerEndSilence()
	{
		return L"</s>";
	}

	boost::wstring_view fillerShortPause()
	{
		return L"[sp]";
	}

	boost::wstring_view fillerInhale()
	{
		return L"[inh]";
	}

	boost::wstring_view fillerEee()
	{
		return L"[eee]";
	}

	boost::wstring_view fillerYyy()
	{
		return L"[yyy]";
	}

	boost::wstring_view fillerClick()
	{
		return L"[clk]";
	}

	boost::wstring_view fillerGlottal()
	{
		return L"[glt]";
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
				PG_DbgAssert2(s1.size() <= s2.size(), "Strings are ordered in ascending order");

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
		PG_Assert(wasAdded);
		sentEndWordPart_ = wordUsage_.getOrAddWordPart(L"</s>", WordPartSide::WholeWord, &wasAdded);
		PG_Assert(wasAdded);
	}

	void UkrainianPhoneticSplitter::bootstrapFromDeclinedWords(const std::unordered_map<std::wstring, std::unique_ptr<WordDeclensionGroup>>& words, const std::wstring& targetWord, const std::unordered_set<std::wstring>& processedWords)
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
						if (wordForm.WordClass != boost::none)
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

	bool UkrainianPhoneticSplitter::allowPhoneticWordSplit() const
	{
		return allowPhoneticWordSplit_;
	}

	void UkrainianPhoneticSplitter::setSentParser(std::shared_ptr<SentenceParser> sentParser)
	{
		sentParser_ = sentParser;
	}

	void UkrainianPhoneticSplitter::gatherWordPartsSequenceUsage(const wchar_t* textFilesDir, long& totalPreSplitWords, int maxFileToProcess)
	{
		QFile corpusFile;
		QTextStream corpusStream;
		if (outputCorpus_)
		{
			corpusFile.setFileName(toQString(corpusFilePath_.wstring()));
			if (!corpusFile.open(QIODevice::WriteOnly | QIODevice::Text))
				return;
			corpusStream.setDevice(&corpusFile);
			corpusStream.setCodec("UTF-8");
			log_ = &corpusStream;
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
				std::wcout << L"SKIPPED " << txtPath.toStdWString() <<std::endl;
				continue;
			}
			std::wcout << txtPath.toStdWString() <<std::endl;

			//
			QFile file(txtPath);
			if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
			{
				std::wcerr << L"Can't open file " << txtPath.toStdWString() <<std::endl;
				return;
			}

			// parse file
			xml.setDevice(&file);

			bool reachedBody = false;
			while (!xml.atEnd())
			{
				QXmlStreamReader::TokenType token = xml.readNext();

				auto nodeName = xml.name();

				if (xml.isStartElement() && xml.name() == "body")
				{
					reachedBody = true;
					continue;
				}

				//if (reachedBody && xml.isCharacters())
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

						std::vector<RawTextLexeme> lexemes;
						lexemes.reserve(words.size());
						analyzeSentence(words, lexemes);

						AbbreviationExpanderUkr abbrExp;
						abbrExp.expandInplace(0, lexemes);

						bool isUkr = checkGoodSentenceUkr(lexemes);
						if (!isUkr)
						{
							if (outputCorpus_) // output sentence with bad words
							{
								corpusStream << "BAD: ";
								for (wv::slice<wchar_t> word : words)
								{
									corpusStream << toQString(boost::wstring_view(word.data(), word.size()));
									corpusStream << " ";
								}
								//for (const WordPart* wp : wordPartsStraight)
								//{
								//	printWordPart(wp, corpusStream);
								//	corpusStream << " ";
								//}
								corpusStream << "\n";
							}
							continue;
						}
						else
						{
							if (outputCorpus_)
							{
								for (const RawTextLexeme& lex : lexemes)
								{
									corpusStream << toQString(lex.ValueStr);
									corpusStream << " ";
								}
								corpusStream << "\n";
							}
						}

						{
							// augment the sentence with start/end terminators
							wordParts.push_back(sentStartWordPart_);

							selectWordParts(lexemes, wordParts, totalPreSplitWords);

							// augment the sentence with start/end terminators
							wordParts.push_back(sentEndWordPart_);
						}

						// calculate statistic only if enough word parts is accumulated
						size_t calcStatWordPartsCount = 1000000;
						if (wordParts.size() > calcStatWordPartsCount)
						{
							calcNGramStatisticsOnWordPartsBatch(wordParts);
						}
					}
				}
			}
			
			// flush the word parts buffer
			calcNGramStatisticsOnWordPartsBatch(wordParts);

			if (xml.hasError())
			{
				std::wcerr <<"XmlError: " << xml.errorString().toStdWString() << std::endl;
			}

			++processedFiles;
		}
	}

	/// Reads text from FB2 documents. FB2 is an xml format.
	class Fb2TextBlockReader : public TextBlockReader
	{
		QXmlStreamReader& xml_;
		bool skippedHeader = false;
	public:

		Fb2TextBlockReader(QXmlStreamReader& xml): xml_(xml)
		{
		}

		bool nextBlock(std::vector<wchar_t>& buff) override
		{
			if (!skippedHeader)
			{
				while (!xml_.atEnd())
				{
					QXmlStreamReader::TokenType token = xml_.readNext();
					auto nodeName = xml_.name();
					if (xml_.isStartElement() && nodeName == "body")
						break;
				}
				skippedHeader = true;
			}
			while (!xml_.atEnd())
			{
				QXmlStreamReader::TokenType token = xml_.readNext();
				auto nodeName = xml_.name();

				if (xml_.isCharacters())
				{
					// BUG: if the lines inside the text are separated using LF only on windows machines
					//      the function below will concatenate the lines. For now, fix LF->CRLF for such files externally
					QStringRef elementText = xml_.text();

					QString elementStr = elementText.toString();
					
					buff.resize(elementStr.size());
					int copyCount = elementStr.toWCharArray(buff.data());
					PG_DbgAssert2(copyCount == elementStr.size(), "Not all chars are copied");
					return true;
				}
			}
			return false;
		}

		bool hasError() const { return xml_.hasError(); }
		std::wstring errorStdWString() const { return xml_.errorString().toStdWString(); }
	};

	void UkrainianPhoneticSplitter::gatherWordPartsSequenceUsageFullSent(const wchar_t* textFilesDir, long& totalPreSplitWords, int maxFileToProcess)
	{
		QFile corpusFile;
		QTextStream corpusStream;
		if (outputCorpus_)
		{
			corpusFile.setFileName(toQString(corpusFilePath_.wstring()));
			if (!corpusFile.open(QIODevice::WriteOnly | QIODevice::Text))
				return;
			corpusStream.setDevice(&corpusFile);
			corpusStream.setCodec("UTF-8");
			log_ = &corpusStream;
		}
		QFile normalizDebugFile;
		QTextStream normalizDebugStream;
		if (outputCorpusNormaliz_)
		{
			normalizDebugFile.setFileName(toQString(corpusNormalizFilePath_.wstring()));
			if (!normalizDebugFile.open(QIODevice::WriteOnly | QIODevice::Text))
				return;
			normalizDebugStream.setDevice(&normalizDebugFile);
			normalizDebugStream.setCodec("UTF-8");
		}

		QXmlStreamReader xml;
		totalPreSplitWords = 0;

		std::vector<wv::slice<wchar_t>> words;
		words.reserve(64);
		std::vector<const WordPart*> wordParts;
		wordParts.reserve(1024*1024);

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
				std::wcout << L"SKIPPED " << txtPath.toStdWString() <<std::endl;
				continue;
			}
			std::wcout << txtPath.toStdWString() <<std::endl;

			//
			QFile file(txtPath);
			if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
			{
				std::wcerr << L"Can't open file " << txtPath.toStdWString() <<std::endl;
				return;
			}
			xml.setDevice(&file);

			// parse file
			Fb2TextBlockReader fb2Reader(xml);

			auto onSent = [&](gsl::span<const RawTextLexeme>& sent)
			{
				gsl::span<const RawTextRun> runs = sentParser_->curSentRuns();

				// expansion is required for numbers (arabic and roman)
				auto needExpansionBefore = [&]() -> bool {
					return std::any_of(std::begin(runs), std::end(runs), [](const RawTextRun& x)
					{
						return x.Type == TextRunType::Digit ||
							x.Str == L"�" ||
							std::all_of(std::begin(x.Str), std::end(x.Str), isRomanNumeral);
					});
				};

				std::vector<RawTextLexeme> oneSent(sent.begin(), sent.end());
				removeWhitespaceLexemes(oneSent);

				auto needExpansionAfter = [&]() -> bool {
					return std::any_of(std::begin(oneSent), std::end(oneSent), [](const RawTextLexeme& x)
					{
						return x.Class == WordClass::Numeral || // arabic or roman
							x.ValueStr == L"�";
					});
				};

				//if (corpusNormalizDebug_ && needExpansionAfter())
				if (outputCorpusNormaliz_)
				{
					normalizDebugStream << "-- ";
					for (const RawTextRun& run : runs)
					{
						normalizDebugStream << toQString(run.Str);
					}
					normalizDebugStream << "\n";
					normalizDebugStream << "++ ";
					for (const RawTextLexeme& lex : oneSent)
					{
						normalizDebugStream << toQString(lex.ValueStr);
						normalizDebugStream << " ";
					}
					normalizDebugStream << "\n";
				}
				
				//
				if (needExpansionAfter())
					return; // keep language model clean

				// remove unnecessary lexemes
				{
					auto newEnd = std::remove_if(std::begin(oneSent), std::end(oneSent),
						[](auto& x)
					{
						return
							x.RunType == TextRunType::Whitespace ||
							x.RunType == TextRunType::Punctuation ||
							x.RunType == TextRunType::PunctuationStopSentence;
					});
					oneSent.erase(newEnd, std::end(oneSent));
				}

				if (outputCorpus_)
				{
					corpusStream << toQString(sentStartWordPart_->partText()) << " ";
					for (const RawTextLexeme& lex : oneSent)
					{
						corpusStream << toQString(lex.ValueStr);
						corpusStream << " ";
					}
					corpusStream << toQString(sentEndWordPart_->partText());
					corpusStream << "\n";
				}

				// augment the sentence with start/end terminators
				wordParts.push_back(sentStartWordPart_);

				selectWordParts(oneSent, wordParts, totalPreSplitWords);

				// augment the sentence with start/end terminators
				wordParts.push_back(sentEndWordPart_);
			};

			sentParser_->setTextBlockReader(&fb2Reader);
			sentParser_->setOnNextSentence(onSent);
			sentParser_->run();

			// collect statistics on all word parts from a file
			calcNGramStatisticsOnWordPartsBatch(wordParts);
			PG_DbgAssert(wordParts.empty())

			if (fb2Reader.hasError())
			{
				std::wcerr <<"XmlError: " << fb2Reader.errorStdWString() << std::endl;
			}

			++processedFiles;
		}
	}

	void UkrainianPhoneticSplitter::analyzeSentence(const std::vector<wv::slice<wchar_t>>& words, std::vector<RawTextLexeme>& lexemes) const
	{
		for (int i = 0; i < words.size(); ++i)
		{
			const wv::slice<wchar_t>& wordSlice = words[i];
			PG_Assert(!wordSlice.empty());

			int digitsCount = 0;
			int romanChCount = 0;
			int engCount = 0;
			int exclEngCount = 0;
			int rusCount = 0;
			int exclRusCount = 0;
			int hyphenCount = 0;

			auto wordCharUsage = [&](wv::slice<wchar_t> word)
			{
				for (size_t charInd = 0; charInd < word.size(); ++charInd)
				{
					wchar_t ch = word[charInd];
					bool isDigit = isDigitChar(ch);
					if (isDigit)
						digitsCount += 1;
					bool isRomanCh = isRomanNumeral(ch);
					if (isRomanCh)
						romanChCount += 1;
					bool isEng = isEnglishChar(ch);
					if (isEng)
						engCount++;
					bool isExclEng = isExclusiveEnglishChar(ch);
					if (isExclEng)
						exclEngCount++;
					bool isRus = isRussianChar(ch);
					if (isRus)
						rusCount++;
					bool isExclRus = isExclusiveRussianChar(ch);
					if (isExclRus)
						exclRusCount++;
					if (ch == L'-' || ch == L'\'')
						hyphenCount++;
				}
			};
			wordCharUsage(wordSlice);

			RawTextLexeme lex;
			lex.ValueStr = boost::wstring_view(wordSlice.data(), wordSlice.size());

			// output non-leterate words with digits, cryptic symbols, etc.
			if (digitsCount == wordSlice.size()) // Arabic number
			{
				lex.Class = WordClass::Numeral;
				lex.NumeralLexView = NumeralLexicalView::Arabic;
			}
			else if (romanChCount == wordSlice.size()) // Roman number
			{
				lex.Class = WordClass::Numeral;
				lex.NumeralLexView = NumeralLexicalView::Roman;
			}
			else if (exclEngCount > 0 && (engCount + hyphenCount) == wordSlice.size()) // english word
			{
				lex.Lang = TextLanguage::English;
			}
			else if (exclRusCount > 0 && (rusCount + hyphenCount) == wordSlice.size()) // russian word
			{
				lex.Lang = TextLanguage::Russian;
			}
			else if (digitsCount > 0 || romanChCount > 0 || exclEngCount > 0 || exclRusCount > 0)
				lex.NonLiterate = true;

			lexemes.push_back(lex);
		}
	}

	bool UkrainianPhoneticSplitter::checkGoodSentenceUkr(const std::vector<RawTextLexeme>& lexemes) const
	{
		int validWords = 0;
		for (const auto& lex : lexemes)
		{
			if (lex.NonLiterate)
			{
				if (log_ != nullptr) *log_ << QString("SKIPPED: %1, because: non-literate\n").arg(toQString(lex.ValueStr));
				continue;
			}
			if (lex.Lang == TextLanguage::Russian || lex.Lang == TextLanguage::English)
			{
				if (log_ != nullptr) *log_ << QString("SKIPPED: %1, because: lang is %2\n")
					.arg(toQString(lex.ValueStr)
					.arg(toQString(toString(lex.Lang.value()))));
				continue;
			}
			validWords += 1;
		}
		return validWords == static_cast<int>(lexemes.size());
	}

	void UkrainianPhoneticSplitter::selectWordParts(const std::vector<RawTextLexeme>& lexemes, std::vector<const WordPart*>& wordParts, long& preSplitWords)
	{
		for (const RawTextLexeme& lexeme : lexemes)
		{
			std::wstring str = lexeme.ValueStr.to_string();

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

	void UkrainianPhoneticSplitter::calcNGramStatisticsOnWordPartsBatch(std::vector<const WordPart*>& wordParts)
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
		int sepInd = phoneticSplitOfWord(wordSlice, boost::none, &matchedSuffixInd);
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