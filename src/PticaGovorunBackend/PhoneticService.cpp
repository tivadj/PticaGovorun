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
		const wchar_t Letter_A = L'а';
		const wchar_t Letter_B = L'б';
		const wchar_t Letter_H = L'г';
		const wchar_t Letter_D = L'д';
		const wchar_t Letter_E = L'е';
		const wchar_t Letter_JE = L'є';
		const wchar_t Letter_ZH = L'ж';
		const wchar_t Letter_Z = L'з';
		const wchar_t Letter_I = L'і';
		const wchar_t Letter_JI = L'ї';
		const wchar_t Letter_K = L'к';
		const wchar_t Letter_N = L'н';
		const wchar_t Letter_S = L'с';
		const wchar_t Letter_T = L'т';
		const wchar_t Letter_U = L'у';
		const wchar_t Letter_TS = L'ц';
		const wchar_t Letter_CH = L'ч';
		const wchar_t Letter_SH = L'ш';
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
		phoneReg.newConsonantPhone("B", SoftHardConsonant::Hard); // B, hard (дріб), palatalized (бюро)
		if (allowPalatalizedConsonant)
			phoneReg.newConsonantPhone("B", palatalProxy); // B''
		phoneReg.newConsonantPhone("V", SoftHardConsonant::Hard); // V, hard (верф), palatalized (цвях)
		if (allowPalatalizedConsonant)
			phoneReg.newConsonantPhone("V", palatalProxy); // V''
		phoneReg.newConsonantPhone("G", SoftHardConsonant::Hard); // G, hard (gudzyk, ґедз), palatalized
		if (allowPalatalizedConsonant)
			phoneReg.newConsonantPhone("G", palatalProxy); // G''
		phoneReg.newConsonantPhone("H", SoftHardConsonant::Hard); // H, hard (gluhi, геній), palatalized (гірко)
		if (allowPalatalizedConsonant)
			phoneReg.newConsonantPhone("H", palatalProxy); // H''
		phoneReg.newConsonantPhone("D", SoftHardConsonant::Hard); // D
		if (allowSoftConsonant)
			phoneReg.newConsonantPhone("D", SoftHardConsonant::Soft); // D1
		phoneReg.newConsonantPhone("DZ", SoftHardConsonant::Hard); // DZ
		if (allowSoftConsonant)
			phoneReg.newConsonantPhone("DZ", SoftHardConsonant::Soft); // DZ1
		phoneReg.newConsonantPhone("DZH", SoftHardConsonant::Hard); // DZH hard (джем), palatalized
		if (allowPalatalizedConsonant)
			phoneReg.newConsonantPhone("DZH", palatalProxy); // DZH''
		phoneReg.newVowelPhone("E", false); // E
		if (allowVowelStress)
			phoneReg.newVowelPhone("E", true); // E1
		phoneReg.newConsonantPhone("ZH", SoftHardConsonant::Hard); // ZH, hard (межа), palatalized (ножі)
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
		phoneReg.newConsonantPhone("J", SoftHardConsonant::Hard); // J, hard only (й)
		phoneReg.newConsonantPhone("K", SoftHardConsonant::Hard); // K, hard (килим), palatalized (кінець)
		if (allowPalatalizedConsonant)
			phoneReg.newConsonantPhone("K", palatalProxy); // K''
		phoneReg.newConsonantPhone("L", SoftHardConsonant::Hard); // L
		if (allowSoftConsonant)
			phoneReg.newConsonantPhone("L", SoftHardConsonant::Soft); // L1
		phoneReg.newConsonantPhone("M", SoftHardConsonant::Hard); // M, hard (сім), palatalized (мій)
		if (allowPalatalizedConsonant)
			phoneReg.newConsonantPhone("M", palatalProxy); // M''
		phoneReg.newConsonantPhone("N", SoftHardConsonant::Hard); // N
		if (allowSoftConsonant)
			phoneReg.newConsonantPhone("N", SoftHardConsonant::Soft); // N1
		phoneReg.newVowelPhone("O", false); // O
		if (allowVowelStress)
			phoneReg.newVowelPhone("O", true); // O1
		phoneReg.newConsonantPhone("P", SoftHardConsonant::Hard); // P, hard (п'ять), palatalized (піл)
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
		phoneReg.newConsonantPhone("F", SoftHardConsonant::Hard); // F, hard (верф), palatalized (фігура)
		if (allowPalatalizedConsonant)
			phoneReg.newConsonantPhone("F", palatalProxy); // F''
		phoneReg.newConsonantPhone("KH", SoftHardConsonant::Hard); // KH, hard (х'ю), palatalized (хіба)
		if (allowPalatalizedConsonant)
			phoneReg.newConsonantPhone("KH", palatalProxy); // KH''
		phoneReg.newConsonantPhone("TS", SoftHardConsonant::Hard); // TS
		if (allowSoftConsonant)
			phoneReg.newConsonantPhone("TS", SoftHardConsonant::Soft); // TS1
		phoneReg.newConsonantPhone("CH", SoftHardConsonant::Hard); // CH, hard (час), palatalized (очі)
		if (allowPalatalizedConsonant)
			phoneReg.newConsonantPhone("CH", palatalProxy); // CH''
		phoneReg.newConsonantPhone("SH", SoftHardConsonant::Hard); // SH, hard (лоша, ш'є) , palatalized (товариші)
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
		// кпстфхшцч
		// КПСТФХШЦЧ
		return
			ch == L'к' || ch == L'К' ||
			ch == L'п' || ch == L'П' ||
			ch == L'с' || ch == L'С' ||
			ch == L'т' || ch == L'Т' ||
			ch == L'ф' || ch == L'Ф' ||
			ch == L'х' || ch == L'Х' ||
			ch == L'ш' || ch == L'Ш' ||
			ch == L'ц' || ch == L'Ц' ||
			ch == L'ч' || ch == L'Ч';
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
		return isStressed == (char)-1 ? boost::none : boost::optional<bool>((bool)isStressed);
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
		// зжер [ZH ZH E R]
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
			// з'їзджають [Z J I ZH DZH A J U T]
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
			// донісши [D O N I SH SH Y]
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
			// шістдесят [SH I Z D E S A T]
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
			// нацистської [N A TS Y S1 K O J I]
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
			// шістсот [SH I S S O T]
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
			// відпустці [V I D P U S TS I]
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
			// п'ятсот [P J A TS O T]
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
			// триматиметься [T R Y M A T Y M E TS1 A]
			if (offsetLetter(1) == Letter_SoftSign && offsetLetter(2) == Letter_S)
			{
				// if тц -> TS TS
				addPhone(newConsonantPhone("TS", SoftHardConsonant::Soft));
				letterInd_ += 2;
				return true;
			}
		}
		if (letterInd_ + 1 < word_->size()) // size(TS)=1
		{
			// T TS -> TS TS
			// клітці [K L I TS TS I]
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
			// отче [O CH CH E]
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
				// apostrophe after vowel may be used in uk->en transliteration eg. "he's -> хі'з"
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
					// for eg for Russian word "чьей"
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
		// єврей [J E V R E J]
		// юнак [J U N A K]
		// яблуко [J A B L U K O]
		// Rule: JA -> J A when the previous letter is a vowel
		// взаємно [V Z A J E M N O]
		// настою [N A S T O J U]
		// абияк [A B Y J A K]
		// Rule: JA -> J A when the previous letter is the soft sign
		// коньяку[K O N1 J A K]
		// мосьє [M O S1 J E]
		// нью [N1 J U]
		// "будь-яка" [B U D1 J A K A]
		// Rule: JA -> J A when the previous letter is the apostrophe
		// бур'ян [B U R J A N]
		// кар'єр [K A R J E R]
		// комп'ютер [K O M P J U T E R]
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
		// люди [L1 U D Y]
		// олексюк [O L E K S1 U K]
		// бурю [B U R1 U]
		// лякати [L1 A K A T Y]
		// буря [B U R1 A]
		// зоряний [Z O R1 A N Y J]
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

			// ллє [L1 L1 E1]
			// суттєво [S U T T E V O]
			// моделлю [M O D E L1 L1 U]
			// ілля [I L1 L1 A]
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
		// B->P необхідно [N E O P KH I D N O]
		// H->KH допомогти [D O P O M O KH T Y]
		// D->T швидко [SH V Y T K O]
		// ZH->SH дужче [D U SH CH E]
		// Z->S безпеки [B E S P E K A]
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
		case L'а':
			init("A", CharGroup::Vowel); break;
		case L'б':
			init("B", CharGroup::Consonant); break;
		case L'в':
			init("V", CharGroup::Consonant); break;
		case L'г':
			init("H", CharGroup::Consonant); break;
		case L'ґ':
			init("G", CharGroup::Consonant); break;
		case L'д':
			init("D", CharGroup::Consonant); break;
		case L'е':
		case L'є':
			init("E", CharGroup::Vowel); break;
		case L'ж':
			init("ZH", CharGroup::Consonant); break;
		case L'з':
			init("Z", CharGroup::Consonant); break;
		case L'и':
			init("Y", CharGroup::Vowel); break;
		case L'і':
		case L'ї':
			init("I", CharGroup::Vowel); break;
		case L'й':
			init("J", CharGroup::Consonant); break;
		case L'к':
			init("K", CharGroup::Consonant); break;
		case L'л':
			init("L", CharGroup::Consonant); break;
		case L'м':
			init("M", CharGroup::Consonant); break;
		case L'н':
			init("N", CharGroup::Consonant); break;
		case L'о':
			init("O", CharGroup::Vowel); break;
		case L'п':
			init("P", CharGroup::Consonant); break;
		case L'р':
			init("R", CharGroup::Consonant); break;
		case L'с':
			init("S", CharGroup::Consonant); break;
		case L'т':
			init("T", CharGroup::Consonant); break;
		case L'у':
			init("U", CharGroup::Vowel); break;
		case L'ф':
			init("F", CharGroup::Consonant); break;
		case L'х':
			init("KH", CharGroup::Consonant); break;
		case L'ц':
			init("TS", CharGroup::Consonant); break;
		case L'ч':
			init("CH", CharGroup::Consonant); break;
		case L'ш':
		case L'щ':
			init("SH", CharGroup::Consonant); break;
		case L'ю':
			init("U", CharGroup::Vowel); break;
		case L'я':
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
		PartOfSpeech WordClass = PartOfSpeech::Verb;
		int UsedCount = 0;

		SuffixEnd(const std::wstring& matchSuffix, int takeCharsCount)
			: MatchSuffix(matchSuffix),
			TakeCharsCount(takeCharsCount) {}
		SuffixEnd(const std::wstring& matchSuffix, int takeCharsCount, PticaGovorun::PartOfSpeech wordClass)
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
			{ std::wstring(    L"а"), 1, PartOfSpeech::Noun }, // ручк~а noun
			{ std::wstring(    L"а"), 1, PartOfSpeech::Numeral }, // багатомільярдн~а ст~а
			{ std::wstring(    L"а"), 1, PartOfSpeech::Adjective }, // ближч~а adj
			{ std::wstring(  L"ала"), 2 }, // сказа~ла
			{ std::wstring(L"увала"), 4 }, // існу~вала актуалізу~вала
			{ std::wstring(L"ювала"), 4 }, // базарю~вала
			{ std::wstring(  L"ила"), 2 }, // жи~ла би~ла
			{ std::wstring(  L"іла"), 2 }, // буркоті~ла
			//{ std::wstring(  L"їла"), 2 }, // благоустрої~ла
			{ std::wstring( L"оїла"), 3 }, // благоустро~їла
			{ std::wstring(  L"ола"), 2 }, // боро~ла
			{ std::wstring(  L"ула"), 2 }, // бамкну~ла
			{ std::wstring(  L"ьма"), 2, PartOfSpeech::Numeral}, // дев'ять~ма
			{ std::wstring(  L"ома"), 3, PartOfSpeech::Numeral}, // дв~ома дев'ять~ома
			{ std::wstring(  L"іша"), 3, PartOfSpeech::Adjective }, // абсурдн~іша

			{ std::wstring(  L"ав"), 1 }, // бува~в назива~в ма~в вплива~в
			{ std::wstring(L"ував"), 3 }, // актуалізу~вав існу~вав
			{ std::wstring(L"ював"), 3 }, // базарю~вав
			//{ std::wstring(  L"ев"), 1 }, // ?? NOT дерев~
			{ std::wstring(  L"ив"), 1 }, // безчести~в би~в
			{ std::wstring(  L"ів"), 1 }, // буркоті~в
			{ std::wstring(  L"ів"), 2, PartOfSpeech::Noun }, // фургон~ів noun
			{ std::wstring(  L"їв"), 2, PartOfSpeech::Noun }, // аграрі~їв одностро~їв noun
			//{ std::wstring(  L"їв"), 1 }, // однострої~в благоустрої~в
			{ std::wstring( L"іїв"), 2 }, // благоустро~їв
			{ std::wstring( L"оїв"), 2 }, // благоустро~їв verb
			{ std::wstring(  L"ов"), 1 }, // боро~в (weird word, =боровся)
			{ std::wstring(  L"ов"), 2, PartOfSpeech::Noun }, // церк~ов
			{ std::wstring(  L"ув"), 1 }, // бамкну~в

			{ std::wstring(        L"е"), 1 }, // бер~е бамкн~е
			{ std::wstring(        L"е"), 1, PartOfSpeech::Adjective }, // ближч~е близьк~е
			{ std::wstring(        L"е"), 1, PartOfSpeech::Noun }, // фургон~е
			{ std::wstring(        L"е"), 1, PartOfSpeech::Numeral }, // багатомільярдн~е
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
			{ std::wstring(      L"іше"), 3, PartOfSpeech::Adjective }, // абсурдн~іше
			{ std::wstring(      L"іше"), 3, PartOfSpeech::Adverb }, // азартн~іше
			{ std::wstring( L"є"), 1, PartOfSpeech::Adjective }, // автодорожн~є
			{ std::wstring(L"ає"), 1 }, // вплива~є
			{ std::wstring(L"еє"), 2, PartOfSpeech::Adjective }, // безпосадков~еє багатеньк~еє близьк~еє adj
			{ std::wstring(L"іє"), 1 }, // бурі~є
			{ std::wstring(L"ує"), 1 }, // існу~є
			{ std::wstring(L"ює"), 1 }, // дорівню~є
			{ std::wstring(L"'є"), 1, PartOfSpeech::Noun }, // сім'~є

			{ std::wstring(     L"и"), 1, PartOfSpeech::Noun }, // ручк~и буркн~и береж~и реклам~и noun
			{ std::wstring(     L"и"), 1, PartOfSpeech::Numeral }, // шістдесят~и
			{ std::wstring(   L"али"), 2 }, // бува~ли
			{ std::wstring( L"ували"), 4 }, // актуалізу~вали існу~вали
			{ std::wstring( L"ювали"), 4 }, // базарю~вали (soft U, not J-U)
			{ std::wstring(   L"или"), 2 }, // би~ли
			{ std::wstring(   L"іли"), 2 }, // буркоті~ли
			//{ std::wstring(   L"їли"), 2 }, // благоустрої~ли
			{ std::wstring(  L"оїли"), 3 }, // благоустро~їли
			{ std::wstring(   L"оли"), 2 }, // боро~ли
			{ std::wstring(   L"ули"), 2 }, // бамкну~ли
			{ std::wstring(   L"ами"), 3, PartOfSpeech::Numeral }, // чотирмаст~ами
			{ std::wstring(   L"ими"), 2 }, // велики~ми
			{ std::wstring(   L"ими"), 3, PartOfSpeech::Numeral }, // багатомільярдн~ими
			//{ std::wstring( L"овими"), 5 }, // adjective
			{ std::wstring(   L"ами"), 3, PartOfSpeech::Noun }, // реклам~ами рук~ами ручк~ами фургон~ами
			{ std::wstring(   L"ями"), 3, PartOfSpeech::Noun }, // аграрі~ями обслуговуванн~ями
			//{ std::wstring(  L"оями"), 2 }, // и~ли noun
			{ std::wstring(  L"'ями"), 3, PartOfSpeech::Noun }, // сім'~ями
			{ std::wstring(   L"ати"), 2 }, // вплива~ти бра~ти
			{ std::wstring( L"ювати"), 4 }, // базарю~вати (soft U)
			{ std::wstring(   L"ити"), 2 }, // жи~ти аркани~ти би~ти
			{ std::wstring(   L"іти"), 2 }, // буркоті~ти
			//{ std::wstring(   L"їти"), 2 }, // благоустрої~ти
			{ std::wstring(  L"оїти"), 3 }, // благоустро~їти
			{ std::wstring(   L"оти"), 2 }, // боро~ти
			{ std::wstring(   L"ути"), 2 }, // бамкну~ти
			{ std::wstring( L"увати"), 4 }, // актуалізу~вати
			{ std::wstring(   L"ачи"), 2, PartOfSpeech::VerbalAdverb }, // бурлача~чи вантажа~чи
			{ std::wstring(   L"учи"), 2, PartOfSpeech::VerbalAdverb }, // беру~чи бережу~чи
			{ std::wstring(   L"ючи"), 3, PartOfSpeech::VerbalAdverb }, // бор~ючи ?
			//{ std::wstring(  L"аючи"), 3, PartOfSpeech::VerbalAdverb }, // незважа~ючи бува~ючи
			//{ std::wstring(  L"іючи"), 3, PartOfSpeech::VerbalAdverb }, // бурі~ючи байдужі~ючи
			//{ std::wstring(  L"уючи"), 3, PartOfSpeech::VerbalAdverb }, // у~ючи існу~ючи
			//{ std::wstring(  L"юючи"), 3, PartOfSpeech::VerbalAdverb }, // дорівню~ючи
			//{ std::wstring(  L"'ючи"), 3, PartOfSpeech::VerbalAdverb }, // б'~ючи
			{ std::wstring(   L"ячи"), 3, PartOfSpeech::VerbalAdverb }, // буркот~ячи виход~ячи буд~ячи
			{ std::wstring(  L"оячи"), 3, PartOfSpeech::VerbalAdverb }, // гно~ячи
			{ std::wstring(  L"авши"), 3, PartOfSpeech::VerbalAdverb }, // ма~вши зна~вши бра~вши
			{ std::wstring(  L"івши"), 3, PartOfSpeech::VerbalAdverb }, // буркоті~вши буботі~вши
			//{ std::wstring(  L"ївши"), 3, PartOfSpeech::VerbalAdverb }, // благоустрої~вши
			{ std::wstring( L"оївши"), 4, PartOfSpeech::VerbalAdverb }, // благоустро~ївши
			{ std::wstring(  L"увши"), 3, PartOfSpeech::VerbalAdverb }, // бамкну~вши бу~вши
			{ std::wstring(  L"ивши"), 3, PartOfSpeech::VerbalAdverb }, // безчести~вши би~вши
			{ std::wstring(  L"овши"), 3, PartOfSpeech::VerbalAdverb }, // боро~вши
			{ std::wstring(L"ювавши"), 5, PartOfSpeech::VerbalAdverb }, // ідорівню~вавши асоцію~вавши
			{ std::wstring(L"увавши"), 5, PartOfSpeech::VerbalAdverb }, // існу~вавши актуалізу~вавши бу~вавши

			{ std::wstring(   L"і"), 1, PartOfSpeech::Noun }, // так~і україн~і
			{ std::wstring(   L"і"), 1, PartOfSpeech::Adjective }, // ближч~і близьк~і
			{ std::wstring(   L"і"), 1, PartOfSpeech::Numeral }, // багатомільярдн~і
			{ std::wstring(L"оєві"), 3, PartOfSpeech::Noun }, // одностро~єві noun
			{ std::wstring( L"еві"), 3, PartOfSpeech::Noun }, // княз~еві
			{ std::wstring( L"єві"), 3, PartOfSpeech::Noun }, // аграрі~єві
			{ std::wstring( L"ові"), 3, PartOfSpeech::Noun }, // фургон~ові noun
			{ std::wstring( L"іші"), 3, PartOfSpeech::Adjective }, // абсурдн~іші adj

			{ std::wstring( L"ї"), 1, PartOfSpeech::Adjective }, // білоши~ї
			{ std::wstring( L"ї"), 1, PartOfSpeech::Noun }, // одностро~ї
			{ std::wstring(L"еї"), 1, PartOfSpeech::Noun }, // музе~ї
			{ std::wstring(L"єї"), 2, PartOfSpeech::Pronoun }, // ці~єї
			{ std::wstring(L"иї"), 1, PartOfSpeech::Noun }, // коломи~ї
			{ std::wstring(L"ії"), 2, PartOfSpeech::Adjective }, // багатеньк~ії безпосадков~ії adj
			{ std::wstring(L"ії"), 1, PartOfSpeech::Noun }, // полонiзацi~ї телепортаці~ї ліні~ї
			{ std::wstring(L"ої"), 2, PartOfSpeech::Adjective }, // заможн~ої
			{ std::wstring(L"ої"), 1, PartOfSpeech::Noun }, // одностро~ї
			{ std::wstring(L"ої"), 1, PartOfSpeech::Numeral }, // багатомільярдн~ої
			{ std::wstring(L"уї"), 1, PartOfSpeech::Noun }, // буржу~ї
			{ std::wstring(L"юї"), 1 }, // брю~ї
			{ std::wstring(L"яї"), 1, PartOfSpeech::Noun }, // хазя~ї

			{ std::wstring(      L"ай"), 1 }, // назива~й вплива~й
			{ std::wstring(      L"ей"), 2, PartOfSpeech::Noun }, // сім~ей
			{ std::wstring(      L"ий"), 2, PartOfSpeech::Adjective }, // тепл~ий багатеньк~ий adj 
			{ std::wstring(      L"ий"), 2, PartOfSpeech::Numeral }, // багатомільярдн~ий
			{ std::wstring(      L"ій"), 2, PartOfSpeech::Adjective }, // українськ~ій останн~ій автодорожн~ій adj
			{ std::wstring(      L"ій"), 1, PartOfSpeech::Noun }, // однострі~й організаці~й noun
			{ std::wstring(      L"ій"), 1, PartOfSpeech::Numeral }, // багатомільярдн~ій
			{ std::wstring(      L"ій"), 2, PartOfSpeech::Pronoun }, // їхн~ій
			{ std::wstring(      L"їй"), 2, PartOfSpeech::Adjective }, // безкра~їй
			{ std::wstring(      L"ой"), 1 }, // благоустро~й
			{ std::wstring(      L"уй"), 1 }, // існу~й
			{ std::wstring(      L"юй"), 1 }, // дорівню~й
			{ std::wstring(    L"ілий"), 3, PartOfSpeech::Participle }, // бурі~лий байдужі~лий ИЙ
			{ std::wstring(    L"аний"), 3, PartOfSpeech::Participle }, // а~ний бра~ний ИЙ
			{ std::wstring(  L"ований"), 5, PartOfSpeech::Participle }, // актуалізо~ваний анігільо~ваний асоційо~ваний
			{ std::wstring(  L"уваний"), 5, PartOfSpeech::Participle }, // арештову~ваний
			{ std::wstring(  L"юваний"), 5, PartOfSpeech::Participle }, // підозрю~ваний
			{ std::wstring(    L"ений"), 3, PartOfSpeech::Participle }, // безчеще~ний береже~ний вантаже~ний ИЙ
			{ std::wstring(    L"єний"), 4, PartOfSpeech::Adjective }, // благоустро~єний гно~єний заспоко~єний ИЙ
			{ std::wstring(   L"оєний"), 4, PartOfSpeech::Adjective }, // благоустро~єний гно~єний заспоко~єний
			{ std::wstring(    L"итий"), 3, PartOfSpeech::Participle }, // би~тий
			{ std::wstring(    L"отий"), 3, PartOfSpeech::Participle }, // боро~тий
			{ std::wstring(    L"утий"), 3, PartOfSpeech::Participle }, // бовтну~тий
			{ std::wstring(    L"ачий"), 3, PartOfSpeech::Participle }, // бурлача~чий вантажа~чий
			{ std::wstring(    L"учий"), 3, PartOfSpeech::Participle}, // беру~чий бамкну~чий бережу~чий буду~чий
			{ std::wstring(    L"ючий"), 4, PartOfSpeech::Participle }, // compound бор~ючий
			//{ std::wstring(   L"аючий"), 4, PartOfSpeech::Participle }, // вплива~ючий
			//{ std::wstring(   L"іючий"), 4, PartOfSpeech::Participle }, // бурі~ючий байдужі~ючий
			//{ std::wstring(   L"уючий"), 4 , PartOfSpeech::Participle}, // існу~ючий
			//{ std::wstring(   L"юючий"), 4, PartOfSpeech::Participle }, // дорівню~ючий
			//{ std::wstring(   L"'ючий"), 4, PartOfSpeech::Participle }, // б'~ючий
			{ std::wstring(    L"ячий"), 4, PartOfSpeech::Participle }, // буркот~ячий буд~ячий
			{ std::wstring(   L"оячий"), 4, PartOfSpeech::Participle }, // гно~ячий
			{ std::wstring(     L"ший"), 3, PartOfSpeech::Adjective }, // багат~ший
			{ std::wstring(   L"авший"), 4, PartOfSpeech::Participle }, // бра~вший
			{ std::wstring( L"увавший"), 6, PartOfSpeech::Participle }, // існу~вавший актуалізу~вавший бу~вавший
			{ std::wstring( L"ювавший"), 6, PartOfSpeech::Participle }, // базарю~вавший асоцію~вавший
			{ std::wstring(   L"ивший"), 4, PartOfSpeech::Participle }, // безчести~вший би~вший
			{ std::wstring(   L"івший"), 4, PartOfSpeech::Participle }, // буркоті~вший буботі~вший
			//{ std::wstring(   L"ївший"), 4 }, // благоустрої~вший
			{ std::wstring(  L"оївший"), 5, PartOfSpeech::Participle }, // благоустро~ївший
			{ std::wstring(   L"овший"), 4, PartOfSpeech::Participle }, // боро~вший
			{ std::wstring(   L"увший"), 4, PartOfSpeech::Participle }, // бамкну~вший бу~вший
			{ std::wstring(    L"іший"), 4, PartOfSpeech::Adjective }, // абсурдн~іший

			{ std::wstring(      L"ам"), 2, PartOfSpeech::Noun }, // фургон~ам
			{ std::wstring(      L"ам"), 2, PartOfSpeech::Numeral }, // чотирьомст~ам
			{ std::wstring(      L"ем"), 1 }, // бер~ем бамкн~ем
			{ std::wstring(      L"ем"), 2, PartOfSpeech::Noun }, // саботаж~ем ERROR: бурозем~ анахтем~(анахтема)
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
			{ std::wstring(      L"єм"), 2, PartOfSpeech::Noun }, // composite
			{ std::wstring(      L"єм"), 2 }, // composite
			//{ std::wstring(     L"аєм"), 2 }, // зна~єм
			//{ std::wstring(     L"еєм"), 2 }, // промете~єм noun
			//{ std::wstring(     L"ієм"), 2 }, // критері~єм noun
			//{ std::wstring(     L"оєм"), 2 }, // одностро~єм
			//{ std::wstring(     L"уєм"), 2 }, // чу~єм
			//{ std::wstring(     L"юєм"), 2 }, // базарю~єм
			//{ std::wstring(     L"'єм"), 2 }, // б'~єм
			{ std::wstring(      L"им"), 1 }, // буркоти~м
			{ std::wstring(      L"им"), 2, PartOfSpeech::Numeral }, // багатомільярдн~им
			{ std::wstring(      L"ім"), 1 }, // бамкні~м бері~м бережі~м
			{ std::wstring(      L"ім"), 2, PartOfSpeech::Numeral }, // багатомільярдн~ім
			//{ std::wstring(      L"їм"), 1 }, // благоустрої~м
			{ std::wstring(     L"оїм"), 2 }, // благоустро~їм
			{ std::wstring(      L"ом"), 2, PartOfSpeech::Noun }, // фургон~ом
			{ std::wstring(      L"ом"), 2, PartOfSpeech::Numeral }, // дв~ом одинадцять~ом
			{ std::wstring(      L"ям"), 2, PartOfSpeech::Noun }, // compound люд~ям обслуговуванн~ям
			//{ std::wstring(     L"оям"), 2, PartOfSpeech::Noun }, // одностро~ям
			//{ std::wstring(     L"'ям"), 2, PartOfSpeech::Noun }, // сім'~ям

			{ std::wstring(        L"о"), 1 }, // ручк~о
			{ std::wstring(        L"о"), 1, PartOfSpeech::Adverb }, // азартн~о adverb
			{ std::wstring(        L"о"), 1, PartOfSpeech::Numeral }, // ст~о
			{ std::wstring(      L"ого"), 3, PartOfSpeech::Numeral }, // багатомільярдн~ого
			{ std::wstring(     L"ього"), 3, PartOfSpeech::Pronoun }, // їхнь~ого
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
			{ std::wstring(      L"еро"), 3, PartOfSpeech::Numeral }, // дев'ят~еро
			
			{ std::wstring(      L"у"), 1, PartOfSpeech::Noun }, // noun, ручк~у бер~у буркоч~у безчещ~у береж~у
			{ std::wstring(      L"у"), 1, PartOfSpeech::Numeral }, // багатомільярдн~у
			//{ std::wstring(     L"му"), 2 }, // not usable (фор*м~у)
			//{ std::wstring(    L"аму"), 2 }, // not usable (са*м~у)
			{ std::wstring(    L"єму"), 2, PartOfSpeech::Pronoun }, // своє~му
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
			{ std::wstring(    L"ому"), 3, PartOfSpeech::Adjective }, // бюджетн~ому adj
			{ std::wstring(    L"ому"), 3, PartOfSpeech::Numeral }, // багатомільярдн~ому
			//{ std::wstring(    L"уму"), 2 }, // not usable (розу*м~у)

			{ std::wstring(  L"ах"), 2, PartOfSpeech::Noun }, // ручк~ах фургон~ах
			{ std::wstring(  L"ах"), 2, PartOfSpeech::Numeral }, // чотирьохст~ах
			{ std::wstring(  L"их"), 2, PartOfSpeech::Adjective }, // бюджетн~их
			{ std::wstring(  L"их"), 2, PartOfSpeech::Numeral }, // багатомільярдн~их
			{ std::wstring(  L"ох"), 2, PartOfSpeech::Numeral }, // дев'ять~ох дв~ох
			{ std::wstring(  L"ях"), 2, PartOfSpeech::Noun }, // compound
			//{ std::wstring( L"оях"), 2, PartOfSpeech::Noun }, // одностро~ях
			//{ std::wstring(  L"ях"), 2, PartOfSpeech::Noun }, // обслуговуванн~ях
			//{ std::wstring( L"'ях"), 2, PartOfSpeech::Noun }, // сім'~ях

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
			{ std::wstring(      L"іш"), 2, PartOfSpeech::Adverb }, // азартн~іш
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
			{ std::wstring(        L"ись"), 2, PartOfSpeech::Verb }, // бери~сь
			{ std::wstring(        L"ись"), 2, PartOfSpeech::VerbalAdverb }, // берігши~сь
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
			{ std::wstring(      L"ачись"), 4, PartOfSpeech::VerbalAdverb }, // брижа~чись вантажа~чись
			{ std::wstring(      L"учись"), 4, PartOfSpeech::VerbalAdverb }, // беру~чись
			{ std::wstring(      L"ючись"), 5, PartOfSpeech::VerbalAdverb }, // compound
			//{ std::wstring(     L"аючись"), 5, PartOfSpeech::VerbalAdverb }, // намага~ючись
			//{ std::wstring(     L"уючись"), 5, PartOfSpeech::VerbalAdverb }, // диву~ючись
			//{ std::wstring(     L"юючись"), 5, PartOfSpeech::VerbalAdverb }, // анігілю~ючись
			//{ std::wstring(      L"ючись"), 5, PartOfSpeech::VerbalAdverb }, // борю~чись
			//{ std::wstring(     L"'ючись"), 5, PartOfSpeech::VerbalAdverb }, // б'~ючись
			{ std::wstring(      L"ячись"), 5, PartOfSpeech::VerbalAdverb }, // бо~ячись гно~ячись дивл~ячись буд~ячись
			{ std::wstring(     L"оячись"), 5, PartOfSpeech::VerbalAdverb }, // гно~ячись бо~ячись

			{ std::wstring(     L"авшись"), 5, PartOfSpeech::VerbalAdverb }, // назива~вшись бра~вшись
			{ std::wstring(     L"овшись"), 5, PartOfSpeech::VerbalAdverb }, // боро~вшись
			{ std::wstring(   L"ювавшись"), 7, PartOfSpeech::VerbalAdverb }, // анігілю~вавшись асоцію~вавшись
			{ std::wstring(   L"увавшись"), 7, PartOfSpeech::VerbalAdverb }, // актуалізу~вавшись
			{ std::wstring(     L"ившись"), 5, PartOfSpeech::VerbalAdverb }, // брижи~вшись би~вшись
			//{ std::wstring(     L"ївшись"), 5, PartOfSpeech::VerbalAdverb }, // благоустрої~вшись
			{ std::wstring(    L"оївшись"), 6, PartOfSpeech::VerbalAdverb }, // благоустро~ївшись
			{ std::wstring(     L"увшись"), 5, PartOfSpeech::VerbalAdverb }, // бехну~вшись зверну~вшись
			{ std::wstring(     L"явшись"), 5, PartOfSpeech::VerbalAdverb }, // взя~вшись
			{ std::wstring(    L"оявшись"), 6, PartOfSpeech::VerbalAdverb }, // бо~явшись
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
			{ std::wstring( L"ю"), 1 , PartOfSpeech::Noun}, // беззмінніст~ю
			{ std::wstring(L"аю"), 1 }, // назива~ю
			{ std::wstring(L"єю"), 2, PartOfSpeech::Noun }, // сім'~єю
			{ std::wstring(L"ію"), 1 }, // бурі~ю
			{ std::wstring(L"ою"), 1 }, // гно~ю
			{ std::wstring(L"ою"), 2, PartOfSpeech::Noun }, // благоустр~ою змін~ою
			{ std::wstring(L"ою"), 2, PartOfSpeech::Numeral }, // багатомільярдн~ою
			{ std::wstring(L"ую"), 1 }, // існу~ю актуалізу~ю
			{ std::wstring(L"юю"), 1 }, // доріню~ю
			{ std::wstring(L"'ю"), 1, PartOfSpeech::Noun }, // сім'~ю

			{ std::wstring(          L"я"), 1, PartOfSpeech::Adjective }, // дорожн~я
			{ std::wstring(          L"я"), 1, PartOfSpeech::Noun }, // обслуговуванн~я
			{ std::wstring(         L"ая"), 2, PartOfSpeech::Adjective }, // безпосадков~ая близьк~ая adj
			{ std::wstring(         L"яя"), 2, PartOfSpeech::Adjective }, // автодорожн~яя
			{ std::wstring(         L"'я"), 1, PartOfSpeech::Noun }, // сім'~я
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
			{ std::wstring(        L"ися"), 3, PartOfSpeech::Verb }, // бери~ся диви~ся береж~ися
			{ std::wstring(        L"ися"), 3, PartOfSpeech::VerbalAdverb }, // берігш~ися
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
			{ std::wstring(     L"авшися"), 5, PartOfSpeech::VerbalAdverb }, // назива~вшися бра~вшися
			{ std::wstring(     L"овшися"), 5, PartOfSpeech::VerbalAdverb }, // боро~вшися
			{ std::wstring(   L"увавшися"), 7, PartOfSpeech::VerbalAdverb }, // актуалізу~вавшися
			{ std::wstring(   L"ювавшися"), 7, PartOfSpeech::VerbalAdverb }, // анігілю~вавшися асоцію~вавшися
			{ std::wstring(     L"ившися"), 5, PartOfSpeech::VerbalAdverb }, // безчести~вшися би~вшися
			//{ std::wstring(     L"ївшися"), 5, PartOfSpeech::VerbalAdverb }, // благоустрої~вшися
			{ std::wstring(    L"оївшися"), 6, PartOfSpeech::VerbalAdverb }, // благоустро~ївшися
			{ std::wstring(     L"увшися"), 5, PartOfSpeech::VerbalAdverb }, // бовтну~вшися зверну~вшися

			{ std::wstring(        L"йся"), 3, PartOfSpeech::Participle }, // compound
			//{ std::wstring(       L"айся"), 3, PartOfSpeech::Participle }, // айся назива~йся
			//{ std::wstring(       L"ийся"), 3, PartOfSpeech::Participle }, // берігши~йся би~йся
			//{ std::wstring(       L"ійся"), 3, PartOfSpeech::Participle }, // бі~йся
			//{ std::wstring(       L"ойся"), 3, PartOfSpeech::Participle }, // благоустро~йся
			//{ std::wstring(       L"уйся"), 3, PartOfSpeech::Participle }, // актуалізу~йся
			//{ std::wstring(       L"юйся"), 3, PartOfSpeech::Participle }, // анігілю~йся
			{ std::wstring(    L"авшийся"), 6, PartOfSpeech::Participle }, // бра~вшийся
			{ std::wstring(  L"увавшийся"), 8, PartOfSpeech::Participle }, // акуталізу~вавшийся
			{ std::wstring(  L"ювавшийся"), 8, PartOfSpeech::Participle }, // анігілю~вавшийся асоцію~вавшийся
			{ std::wstring(    L"ившийся"), 6, PartOfSpeech::Participle }, // брижи~вшийся би~вшийся
			//{ std::wstring(    L"ївшийся"), 6 }, // благоустрої~вшийся
			{ std::wstring(   L"оївшийся"), 7, PartOfSpeech::Participle  }, // благоустро~ївшийся
			{ std::wstring(    L"овшийся"), 6, PartOfSpeech::Participle }, // боро~вшийся
			{ std::wstring(    L"увшийся"), 6, PartOfSpeech::Participle }, // бехну~вшийся
			{ std::wstring(    L"явшийся"), 7, PartOfSpeech::Participle  }, // бо~явшийся
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
	int phoneticSplitOfWord(wv::slice<wchar_t> word, boost::optional<PartOfSpeech> wordClass, int* pMatchedSuffixInd)
	{
		ensureSureSuffixesInitialized();

		for (size_t suffixInd = 0; suffixInd < sureSuffixes.size(); ++suffixInd)
		{
			const SuffixEnd& suffixEnd = sureSuffixes[suffixInd];
			PartOfSpeech suffixClass = suffixEnd.WordClass;

			//PartOfSpeech suffixClass = suffixEnd.PartOfSpeech;
			//if (suffixClass == PartOfSpeech::Participle || suffixClass == PartOfSpeech::VerbalAdverb)
			//	suffixClass = PartOfSpeech::Verb;

			//PartOfSpeech wordClassTmp = wordClass.get();
			//if (wordClassTmp == PartOfSpeech::Participle || wordClassTmp == PartOfSpeech::VerbalAdverb)
			//	wordClassTmp = PartOfSpeech::Verb;

			if (wordClass != boost::none)
			{
				// match word class and suffix (word) class
				PartOfSpeech wordClassTmp = wordClass.get();
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

				if (wordClass == PartOfSpeech::Participle && suffixEnd.WordClass != PartOfSpeech::Participle)
				{
					participleSuffixToWord[suffixEnd.MatchSuffix] = std::wstring(word.data(), word.size());
				}
				if (wordClass == PartOfSpeech::VerbalAdverb && suffixEnd.WordClass != PartOfSpeech::VerbalAdverb)
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

	boost::wstring_view fillerSingleSpace()
	{
		return L"_s";
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
		bool voiceless = ch == L'ч'; // буркоч
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

			if (wordGroup.WordClass == PartOfSpeech::Irremovable ||
				wordGroup.WordClass == PartOfSpeech::Preposition ||
				wordGroup.WordClass == PartOfSpeech::Pronoun ||
				wordGroup.WordClass == PartOfSpeech::Conjunction ||
				wordGroup.WordClass == PartOfSpeech::Interjection ||
				wordGroup.WordClass == PartOfSpeech::Particle ||
				wordGroup.WordClass == PartOfSpeech::Irremovable)
			{
				// keep the word intact
				continue;
			}
			else if (
				wordGroup.WordClass == PartOfSpeech::Adjective ||
				wordGroup.WordClass == PartOfSpeech::Adverb ||
				wordGroup.WordClass == PartOfSpeech::Noun ||
				wordGroup.WordClass == PartOfSpeech::Numeral ||
				wordGroup.WordClass == PartOfSpeech::Verb ||
				wordGroup.WordClass == PartOfSpeech::VerbalAdverb ||
				wordGroup.WordClass == PartOfSpeech::Participle)
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

						wv::slice<const wchar_t> subWord = wv::make_view(word.data() + offset, newOffset - offset);

						int matchedSuffixInd = -1;
						PartOfSpeech curWordClass = wordGroup.WordClass.get();
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
								wordLastChar == L'ь' ||
								wordLastChar == L'ж' || // бродяж
								voiceless;

							// it is ok to not finding consonant+sa
                            std::wstring sya(L"ся");
                            if (!ok && endsWith(subWord, wv::slice<const wchar_t>(sya.data(), sya.data()+sya.size())))
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

							wv::slice<const wchar_t> prefix = wv::make_view(subWord.data(), sepInd);
							wv::slice<const wchar_t> suffix = wv::make_view(subWord.data() + sepInd, subWord.size() - sepInd);

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
				PG_Assert(false); // unknown word class
		}
		const auto& m1 = participleSuffixToWord;
		const auto& m2 = participleSuffixToWord2;
		std::wcout << L"PARTICIPLE\n";
		for (const auto& pair : participleSuffixToWord)
		{
			std::wcout << pair.first.c_str();
			std::wcout << L"\t";
			std::wcout << pair.second.c_str();
			std::wcout << L"\n";
		}
		std::wcout << L"VERBALADVERB\n";
		for (const auto& pair : participleSuffixToWord2)
		{
			std::wcout << pair.first.c_str();
			std::wcout <<  L"\t";
			std::wcout << pair.second.c_str();
			std::wcout << L"\n";
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
		std::wcout << L"Suffix usage statistics:\n";
		for (const auto& suffixEnd : sureSuffixesCopy)
		{
			std::wcout << suffixEnd.MatchSuffix.data();
			std::wcout << L"\t";
			std::wcout << QString::number(suffixEnd.UsedCount).toStdWString().c_str();

			wchar_t* classStr = L"";
			if (suffixEnd.WordClass == PartOfSpeech::Verb)
				classStr = L"v";
			else if (suffixEnd.WordClass == PartOfSpeech::Noun)
				classStr = L"n";
			else if (suffixEnd.WordClass == PartOfSpeech::Adjective)
				classStr = L"adj";
			else if (suffixEnd.WordClass == PartOfSpeech::Adverb)
				classStr = L"adverb";
			std::wcout << L" ";
			std::wcout << classStr;
			std::wcout << L"\n";
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
							x.Str == L"¬" ||
							std::all_of(std::begin(x.Str), std::end(x.Str), isRomanNumeral);
					});
				};

				std::vector<RawTextLexeme> oneSent(sent.begin(), sent.end());
				removeWhitespaceLexemes(oneSent);

				auto needExpansionAfter = [&]() -> bool {
					return std::any_of(std::begin(oneSent), std::end(oneSent), [](const RawTextLexeme& x)
					{
						return x.Class == PartOfSpeech::Numeral || // arabic or roman
							x.ValueStr == L"¬";
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
				lex.Class = PartOfSpeech::Numeral;
				lex.NumeralLexView = NumeralLexicalView::Arabic;
			}
			else if (romanChCount == wordSlice.size()) // Roman number
			{
				lex.Class = PartOfSpeech::Numeral;
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
			if (wordPart->partText() == L"валуванн")
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

			wv::slice<const wchar_t> prefix = wv::make_view(wordSlice.data(), sepInd);
			wv::slice<const wchar_t> suffix = wv::make_view(wordSlice.data() + sepInd, wordSlice.size() - sepInd);

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