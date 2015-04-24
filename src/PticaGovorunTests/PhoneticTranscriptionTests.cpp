#include "catch.hpp"
#include <vector>
#include <memory>

#include "ClnUtils.h"
#include "PhoneticService.h"

namespace PticaGovorun
{
	TEST_CASE("phone list to str conversion")
	{
		PhoneRegistry phoneReg;
		initPhoneRegistryUk(phoneReg, true, true);

		std::vector<PhoneId> phones;
		std::string phonesStrIn = "S U1 T1";
		bool parseOp = parsePhoneList(phoneReg, phonesStrIn, phones);
		REQUIRE(parseOp);

		std::string phonesStrOut;
		bool toStrOp = phoneListToStr(phoneReg, phones, phonesStrOut);
		REQUIRE(toStrOp);
		REQUIRE(phonesStrIn == phonesStrOut);
	}

	inline void spellTest(const wchar_t* word, const char* expectTranscription, const PhoneRegistry* phoneRegClient = nullptr)
	{
		std::unique_ptr<PhoneRegistry> phoneRegLocal;
		if (phoneRegClient == nullptr)
		{
			phoneRegLocal = std::make_unique<PhoneRegistry>();
			initPhoneRegistryUk(*phoneRegLocal, true, true);
		}

		const PhoneRegistry* phoneReg = phoneRegClient != nullptr ? phoneRegClient : phoneRegLocal.get();

		std::vector<PhoneId> phones;
		bool spellOp;
		const char* errMsg;
		std::tie(spellOp, errMsg) = spellWordUk(*phoneReg, word, phones);
		REQUIRE(spellOp);

		std::vector<PhoneId> expectPhones;
		bool parseOp = parsePhoneList(*phoneReg, expectTranscription, expectPhones);
		REQUIRE(parseOp);
		
		bool ok = expectPhones == phones;
		REQUIRE(ok);
	}

	TEST_CASE("phonetic simple1")
	{
		spellTest(L"���", "R U1 KH");
	}

	TEST_CASE("vowel stress can't be determined")
	{
		spellTest(L"�����", "A A R O N");
		spellTest(L"�����", "S L O V A");
	}

	TEST_CASE("simple compound phones")
	{
		// DZ
		spellTest(L"��������", "DZ E R K A L O");
		// DZH
		spellTest(L"������", "B U DZH E T");

		SECTION("always double YI") {
			spellTest(L"���", "V O J I N");
		}
		SECTION("always double SH CH") {
			spellTest(L"��", "SH CH O1");
		}
	}

	TEST_CASE("spell tests")
	{
		SECTION("double phone YE YU YA after apostrophe") {
			spellTest(L"�'�", "B J E1");
			spellTest(L"�'�", "B J U1");
			spellTest(L"��'�", "P I R J A");
		}
		SECTION("double phone YE YU YA after soft sign") {
			spellTest(L"�����", "D O S1 J E");
			spellTest(L"������", "S1 J U Z E N");
			spellTest(L"�����", "V I L1 J A M");
		}
		SECTION("double phone YE YU YA when it the first letter") {
			// first letter is � � �
			spellTest(L"�����", "J E V R E J");
			spellTest(L"����", "J U N A K");
			spellTest(L"������", "J A B L U K O");
		}
		SECTION("double phone YE YU YA after vowel") {
			spellTest(L"������", "V Z A J E M N O");
			spellTest(L"������", "N A S T O J U");
			spellTest(L"�����", "A B Y J A K");
		}
		SECTION("single phone YE YU YA after soft consonant") {
			spellTest(L"������", "S U T T E V O");
			spellTest(L"�����", "S E L U K");
			// YA
			spellTest(L"�������", "Z O R A N Y J");
			spellTest(L"����", "B U R A"); // TODO: B U R1 A
		}
		SECTION("BP HKH DT ZHSH ZS before unvoiced consonant") {
			// B->P
			spellTest(L"���������", "N E O P KH I D N O");
			// H->KH
			spellTest(L"�����", "L E KH SH E");
			// D->T
			spellTest(L"�������", "V I T P O V I D A J E");
			spellTest(L"�����", "P I T P Y S");
			// ZH->SH
			spellTest(L"����", "L I SH K A");
			// Z->S
			spellTest(L"�������������", "B E S P O S E R E D N1 O");
			spellTest(L"������", "Z A L I S T Y");
			spellTest(L"����������", "R O S P O V I D A L Y");
			spellTest(L"���������������", "R O S P O V S U DZH U V A T Y");

			SECTION("BP HKH DT ZHSH ZS before soft sign and unvoiced consonant") {
				// D 1 ->T
				spellTest(L"������", "D A T1 K O");
				// Z 1 -> S
				spellTest(L"��������", "A P KH A S1 K A");
				spellTest(L"�������", "B L Y S1 K O");
				spellTest(L"������", "V U S1 K O");
			}
		}
	}
	TEST_CASE("redundant letter Z")
	{
		// Z ZH -> ZH ZH
		spellTest(L"���������", "B E ZH ZH A L1 N O");
		spellTest(L"������", "ZH ZH E R L Y");

		// Z D ZH -> ZH DZH
		spellTest(L"�'��������", "Z J I ZH DZH A J U T1");
		spellTest(L"������", "P Y ZH DZH U");
	}
	TEST_CASE("redundant letter NTST NTS1K")
	{
		// N T S T -> N S T
		spellTest(L"���������", "A H E N S T V A");
		// N T S1 K -> N S1 K
		spellTest(L"������������", "S T U D E N S1 K Y J");
	}
	TEST_CASE("redundant letter S")
	{
		// S SH -> SH SH
		spellTest(L"�������", "P R Y N I SH SH Y");
		spellTest(L"���", "SH SH A1");
		//spellTest(L"�������", "M A SH T A B"); // alternative=no doubling
		// ������� [S E K S SH O P] two different words, no doubling
	}
	TEST_CASE("redundant letter ST")
	{
		// S T D -> Z D
		spellTest(L"���������", "SH I Z D E S A T");

		// S T S 1 K -> S1 K
		spellTest(L"������������", "M O D E R N I S1 K A");
		spellTest(L"����������", "N A TS Y S1 K A");
		spellTest(L"����������", "F A SH Y S1 K A");

		// S T S -> S S
		spellTest(L"������������", "P O S S K R Y P T U M");
		spellTest(L"�������", "SH I S S O T");

		// S T TS -> S TS
		spellTest(L"�������", "N E V I S TS I");
		spellTest(L"������", "P A S TS I");
	}
	TEST_CASE("redundant letter T")
	{
		// T S -> TS
		spellTest(L"���������", "B A H A TS T V O");
		// T 1 S -> TS
		spellTest(L"������������", "A K T Y V I Z U J U TS1 A");

		// T TS -> TS TS
		spellTest(L"����", "O TS TS A");
		spellTest(L"������", "K U R TS TS I");

		// T CH -> CH CH
		spellTest(L"�������", "V I CH CH Y Z N A");
		spellTest(L"�������", "L1 O CH CH Y K");
	}

	TEST_CASE("vary phone softness and stress")
	{
		SECTION("no soft no stress") {
			PhoneRegistry phoneReg;
			bool allowSoftHardConsonant = false;
			bool allowVowelStress = false;
			initPhoneRegistryUk(phoneReg, allowSoftHardConsonant, allowVowelStress);
			spellTest(L"����", "S U T", &phoneReg);
		}
		SECTION("do soft no stress") {
			PhoneRegistry phoneReg;
			bool allowSoftHardConsonant = true;
			bool allowVowelStress = false;
			initPhoneRegistryUk(phoneReg, allowSoftHardConsonant, allowVowelStress);
			spellTest(L"����", "S U T1", &phoneReg);
		}
		SECTION("no soft do stress") {
			PhoneRegistry phoneReg;
			bool allowSoftHardConsonant = false;
			bool allowVowelStress = true;
			initPhoneRegistryUk(phoneReg, allowSoftHardConsonant, allowVowelStress);
			spellTest(L"����", "S U1 T", &phoneReg);
		}
		SECTION("do soft do stress") {
			PhoneRegistry phoneReg;
			bool allowSoftHardConsonant = true;
			bool allowVowelStress = true;
			initPhoneRegistryUk(phoneReg, allowSoftHardConsonant, allowVowelStress);
			spellTest(L"����", "S U1 T1", &phoneReg);
		}
	}

}
