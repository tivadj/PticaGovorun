#include <vector>
#include <memory>
#include <gtest/gtest.h>
#include "ClnUtils.h"
#include "PhoneticService.h"

namespace PticaGovorunTests
{
	using namespace PticaGovorun;

	struct PhoneticTranscriptionTest : public testing::Test
	{
	};
	TEST_F(PhoneticTranscriptionTest, phoneListToStrConversion)
	{
		PhoneRegistry phoneReg;
		phoneReg.setPalatalSupport(PalatalSupport::AsPalatal);
		initPhoneRegistryUk(phoneReg, true, true);

		std::vector<PhoneId> phones;
		std::string phonesStrIn = "S U1 T1 B2 I";
		bool parseOp = parsePhoneList(phoneReg, phonesStrIn, phones);
		ASSERT_TRUE(parseOp);

		std::string phonesStrOut;
		bool toStrOp = phoneListToStr(phoneReg, phones, phonesStrOut);
		ASSERT_TRUE(toStrOp);
		EXPECT_EQ(phonesStrIn, phonesStrOut);
	}

	inline void spellTest(const wchar_t* word, const char* expectTranscription, const PhoneRegistry* phoneRegClient = nullptr,
		WordPhoneticTranscriber::StressedSyllableIndFunT stressedSyllableIndFun = nullptr)
	{
		std::unique_ptr<PhoneRegistry> phoneRegLocal;
		if (phoneRegClient == nullptr)
		{
			phoneRegLocal = std::make_unique<PhoneRegistry>();
			phoneRegLocal->setPalatalSupport(PalatalSupport::AsPalatal);
			initPhoneRegistryUk(*phoneRegLocal, true, true);
		}

		const PhoneRegistry* phoneReg = phoneRegClient != nullptr ? phoneRegClient : phoneRegLocal.get();

		std::vector<PhoneId> phones;
		bool spellOp;
		const char* errMsg;
		std::tie(spellOp, errMsg) = spellWordUk(*phoneReg, word, phones, stressedSyllableIndFun);
		ASSERT_TRUE(spellOp);

		std::vector<PhoneId> expectPhones;
		bool parseOp = parsePhoneList(*phoneReg, expectTranscription, expectPhones);
		ASSERT_TRUE(parseOp);
		
		bool ok = expectPhones == phones;
		ASSERT_TRUE(ok);
	}

	TEST_F(PhoneticTranscriptionTest, phoneticSimple1)
	{
		spellTest(L"���", "R U1 KH");
	}

	TEST_F(PhoneticTranscriptionTest, vowelStressCantBeDetermined)
	{
		spellTest(L"�����", "A A R O N");
		spellTest(L"�����", "S L O V A");
	}
	TEST_F(PhoneticTranscriptionTest, alwaysDoubleSHCH)
	{
		spellTest(L"��", "SH CH O1");
	}
	TEST_F(PhoneticTranscriptionTest, alwaysDoubleYI)
	{
		spellTest(L"���", "V O J I N");
	}
	TEST_F(PhoneticTranscriptionTest, compoundPhoneDZDZH)
	{
		// DZ
		spellTest(L"��������", "DZ E R K A L O");
		// DZH
		spellTest(L"������", "B2 U DZH E T");
	}
	TEST_F(PhoneticTranscriptionTest, compoundPhoneZ)
	{
		// Z ZH -> ZH ZH
		spellTest(L"���������", "B E ZH ZH A L1 N O");
		spellTest(L"������", "ZH ZH E R L Y");

		// Z D ZH -> ZH DZH
		spellTest(L"�'��������", "Z J I ZH DZH A J U T1");
		spellTest(L"������", "P Y ZH DZH U");
	}
	TEST_F(PhoneticTranscriptionTest, compoundPhoneNTSTNTS1K)
	{
		// N T S T -> N S T
		spellTest(L"���������", "A H E N S T V A");
		// N T S1 K -> N S1 K
		spellTest(L"������������", "S T U D E N1 S1 K Y J");
	}
	TEST_F(PhoneticTranscriptionTest, compoundPhoneS)
	{
		// S SH -> SH SH
		spellTest(L"�������", "P R Y N1 I SH SH Y");
		spellTest(L"���", "SH SH A1");
		//spellTest(L"�������", "M A SH T A B"); // alternative=no doubling
		// ������� [S E K S SH O P] two different words, no doubling
	}
	TEST_F(PhoneticTranscriptionTest, compoundPhoneST)
	{
		// S T D -> Z D
		spellTest(L"���������", "SH2 I Z D E S1 A T");

		// S T S 1 K -> S1 K
		spellTest(L"������������", "M O D E R N1 I S1 K A");
		spellTest(L"����������", "N A TS Y S1 K A");
		spellTest(L"����������", "F A SH Y S1 K A");

		// S T S -> S S
		spellTest(L"������������", "P O S S K R Y P T U M");
		spellTest(L"�������", "SH2 I S S O T");

		// S T TS -> S TS
		spellTest(L"�������", "N E V2 I S1 TS1 I");
		spellTest(L"������", "P A S1 TS1 I");
	}
	TEST_F(PhoneticTranscriptionTest, compoundPhoneTST1STTSTCH)
	{
		// T S -> TS
		spellTest(L"���������", "B A H A TS T V O");
		// T 1 S -> TS
		spellTest(L"������������", "A K T Y V2 I Z U J U TS1 A");

		// T TS -> TS TS
		spellTest(L"����", "O TS1 TS1 A");
		spellTest(L"������", "K U R TS1 TS1 I");

		// T CH -> CH CH
		spellTest(L"�������", "V2 I CH CH Y Z N A");
		spellTest(L"�������", "L1 O CH CH Y K");
	}
	TEST_F(PhoneticTranscriptionTest, doublePhoneYEYUYAWhenItTheFirstLetter)
	{
		// first letter is � � �
		spellTest(L"�����", "J E V R E J");
		spellTest(L"����", "J U N A K");
		spellTest(L"������", "J A B L U K O");
		spellTest(L"��-���", "D E J U R E");
		spellTest(L"��-��-��", "J A K N E J A K");
		spellTest(L"����-������", "O D Y N J E D Y N Y J");
		spellTest(L"��-�����������", "P O J E V R O P E J S1 K Y");
	}
	TEST_F(PhoneticTranscriptionTest, doublePhoneYEYUYAAfterApostrophe)
	{
		spellTest(L"�'�", "B J E1");
		spellTest(L"�'�", "B J U1");
		spellTest(L"��'�", "P2 I R J A");
	}
	TEST_F(PhoneticTranscriptionTest, doublePhoneYEYUYAAfterSoftSign)
	{
		spellTest(L"�����", "D O S1 J E");
		spellTest(L"������", "S1 J U Z E N");
		spellTest(L"�����", "V2 I L1 J A M");
		spellTest(L"����-���", "B U D1 J A K A");
	}
	TEST_F(PhoneticTranscriptionTest, doublePhoneYEYUYAAfterVowel)
	{
		spellTest(L"������", "V Z A J E M N O");
		spellTest(L"������", "N A S T O J U");
		spellTest(L"�����", "A B Y J A K");
		spellTest(L"������", "H O S1 T1 U J E");
	}
	TEST_F(PhoneticTranscriptionTest, hardConsonantBeforeVowelE) {
		spellTest(L"������", "Z E M L E J U");
	}
	TEST_F(PhoneticTranscriptionTest, softConsonantBeforeI)
	{
		spellTest(L"����", "L1 I K A R");
		spellTest(L"����", "L1 I S T Y");
		spellTest(L"�����", "R1 I V E N1");
		spellTest(L"���", "I M L1 I");
		spellTest(L"��", "V2 I1 L");
		spellTest(L"����", "B2 I L K A");
	}
	TEST_F(PhoneticTranscriptionTest, softConsonantBeforeYAYU)
	{
		spellTest(L"������", "L1 A K A T Y");
		spellTest(L"����", "B U R1 A");
		spellTest(L"����", "Z O R1 A");
		spellTest(L"�������", "P O R1 A D O K");
		spellTest(L"�����", "R1 A B Y J");

		spellTest(L"�������", "H R1 U K A T Y");
		spellTest(L"����", "V A R1 U");
		spellTest(L"����", "B U R1 U");
		spellTest(L"����", "L1 U D Y");
		spellTest(L"�����", "S E L1 U K");
	}
	TEST_F(PhoneticTranscriptionTest, softDoubleConsonantBeforeYAYU)
	{
		spellTest(L"����", "I L1 L1 A");
		spellTest(L"�������", "M O D E L1 L1 U");
		spellTest(L"��", "L1 L1 E1");
		spellTest(L"������", "S U T1 T1 E V O");
	}
	TEST_F(PhoneticTranscriptionTest, pairOfConsonantsSoftenEachOther)
	{
		spellTest(L"�����", "K U Z1 N1 A");
		spellTest(L"���", "S1 N1 I1 H");
		spellTest(L"�������", "M O L O T1 TS1 I");
		spellTest(L"�����", "M Y TS1 TS1 I");
		spellTest(L"����������", "R A D1 A N1 S1 K Y J");
		spellTest(L"����", "H2 I L1 TS1 I");
		spellTest(L"��", "D1 N1 I1");
		spellTest(L"���", "D1 L1 A1");
		spellTest(L"����", "P U T1 N1 I");
		spellTest(L"���������", "A P S T R A K T1 N1 I");
		spellTest(L"����", "T1 L1 I T Y");
		// TODO: exception ���� [V O L Z1 I]
	}
	TEST_F(PhoneticTranscriptionTest, pairOfConsonantsSoftenEachOtherRecursive)
	{
		spellTest(L"��������", "B E S1 S1 L1 I D N O");
	}
	TEST_F(PhoneticTranscriptionTest, pairOfConsonantsDampEachOther_make_unvoice_BP_HKH_DT_ZHSH_ZS) {
		// B->P
		spellTest(L"���������", "N E O P KH2 I D N O");
		// H->KH
		spellTest(L"�����", "L E KH SH E");
		// D->T
		spellTest(L"�������", "V2 I T P O V2 I D A J E");
		spellTest(L"�����", "P2 I T P Y S");
		// ZH->SH
		spellTest(L"����", "L1 I SH K A");
		// Z->S
		spellTest(L"�������������", "B E S P O S E R E D1 N1 O");
		spellTest(L"������", "Z A L1 I S T Y");
		spellTest(L"����������", "R O S P O V2 I D A L Y");
		spellTest(L"���������������", "R O S P O V S1 U DZH U V A T Y");

		// BP HKH DT ZHSH ZS before soft sign and unvoiced consonant
		// D 1 ->T
		spellTest(L"������", "D1 A T1 K O");
		// Z 1 -> S
		spellTest(L"��������", "A P KH A S1 K A");
		spellTest(L"�������", "B L Y S1 K O");
		spellTest(L"������", "V U S1 K O");
	}
	TEST_F(PhoneticTranscriptionTest, pairOfConsonantsAmplify_makeVoice_EeachOther) {
		// T->D
		spellTest(L"��������", "B O R O D1 B A");
		spellTest(L"����", "O D ZH E");
		spellTest(L"������", "F U D B O L");
		// K->G
		spellTest(L"������", "V O G Z A L");
		spellTest(L"�������", "E G Z A M E N");
		spellTest(L"����", "J A G B Y");
		spellTest(L"�����", "A J A G ZH E");
		spellTest(L"���������", "V E L Y G D E N1");
		// S->Z
		spellTest(L"�����", "O Z1 D E");
		spellTest(L"����������", "J U R Y Z D Y K TS1 I J I");
		spellTest(L"��������", "L E Z B2 I J A N K A");
		// CH->DZH
		spellTest(L"�������", "U DZH B O V Y KH");
		spellTest(L"���", "L1 I DZH B2 I");
		spellTest(L"��� ��", "KH O DZH B Y");
	}
	TEST_F(PhoneticTranscriptionTest, apostropheInEnUkTransliteration) {
		// he's -> ��'�
		spellTest(L"��'�", "KH2 I1 Z"); // apostrophe is ignored
	}

	TEST_F(PhoneticTranscriptionTest, varyPhoneSoftnessAndStress_NoSoftNoStress) {
		PhoneRegistry phoneReg;
		bool allowSoftHardConsonant = false;
		bool allowVowelStress = false;
		initPhoneRegistryUk(phoneReg, allowSoftHardConsonant, allowVowelStress);
		spellTest(L"����", "S U T", &phoneReg);
	}
	TEST_F(PhoneticTranscriptionTest, varyPhoneSoftnessAndStress_DoSoftNoStress) 
	{
		PhoneRegistry phoneReg;
		bool allowSoftHardConsonant = true;
		bool allowVowelStress = false;
		initPhoneRegistryUk(phoneReg, allowSoftHardConsonant, allowVowelStress);
		spellTest(L"����", "S U T1", &phoneReg);
	}
	TEST_F(PhoneticTranscriptionTest, varyPhoneSoftnessAndStress_NoSoftDoStress)
	{
		PhoneRegistry phoneReg;
		bool allowSoftHardConsonant = false;
		bool allowVowelStress = true;
		initPhoneRegistryUk(phoneReg, allowSoftHardConsonant, allowVowelStress);
		spellTest(L"����", "S U1 T", &phoneReg);
	}
	TEST_F(PhoneticTranscriptionTest, varyPhoneSoftnessAndStress_DoSoftDoStress)
	{
		PhoneRegistry phoneReg;
		bool allowSoftHardConsonant = true;
		bool allowVowelStress = true;
		initPhoneRegistryUk(phoneReg, allowSoftHardConsonant, allowVowelStress);
		spellTest(L"����", "S U1 T1", &phoneReg);
	}

	TEST_F(PhoneticTranscriptionTest, palatalConsonantsSupport)
	{
		PhoneRegistry phoneReg;
		bool allowSoftHardConsonant = true;
		bool allowVowelStress = true;
		initPhoneRegistryUk(phoneReg, allowSoftHardConsonant, allowVowelStress);

		phoneReg.setPalatalSupport(PalatalSupport::AsHard);
		spellTest(L"��", "B I1 J", &phoneReg);

		phoneReg.setPalatalSupport(PalatalSupport::AsSoft);
		spellTest(L"��", "B1 I1 J", &phoneReg);

		phoneReg.setPalatalSupport(PalatalSupport::AsPalatal);
		spellTest(L"��", "B2 I1 J", &phoneReg);
	}
	TEST_F(PhoneticTranscriptionTest, externalProviderOfStressedSyllable)
	{
		auto getStressedSyllableIndFun = [](boost::wstring_view word, std::vector<int>& stressedSyllableInds) -> bool
		{
			if (word.compare(L"�����") == 0)
				stressedSyllableInds.assign({ 1 });
			else if (word.compare(L"�������������") == 0)
				stressedSyllableInds.assign({ 0, 1, 2 });
			else
				return false;
			return true;
		};

		spellTest(L"�����", "T U L U1 B", nullptr, getStressedSyllableIndFun);
		spellTest(L"�������������", "U1 K R T R A1 N S N A1 F T A", nullptr, getStressedSyllableIndFun);
	}
}

// TODO:
// ���������� [N A T S Y L A J U T1] no T+S->TS on the border prefix-suffix
