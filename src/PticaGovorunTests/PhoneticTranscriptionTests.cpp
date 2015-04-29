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
		phoneReg.setPalatalSupport(PalatalSupport::AsPalatal);
		initPhoneRegistryUk(phoneReg, true, true);

		std::vector<PhoneId> phones;
		std::string phonesStrIn = "S U1 T1 B2 I";
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
			phoneRegLocal->setPalatalSupport(PalatalSupport::AsPalatal);
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
		spellTest(L"рух", "R U1 KH");
	}

	TEST_CASE("vowel stress can't be determined")
	{
		spellTest(L"аарон", "A A R O N");
		spellTest(L"слова", "S L O V A");
	}
	TEST_CASE("always double SH CH")
	{
		spellTest(L"що", "SH CH O1");
	}
	TEST_CASE("always double YI")
	{
		spellTest(L"воїн", "V O J I N");
	}
	TEST_CASE("compound phone DZ DZH")
	{
		// DZ
		spellTest(L"дзеркало", "DZ E R K A L O");
		// DZH
		spellTest(L"бюджет", "B2 U DZH E T");
	}
	TEST_CASE("compound phone Z")
	{
		// Z ZH -> ZH ZH
		spellTest(L"безжально", "B E ZH ZH A L1 N O");
		spellTest(L"зжерли", "ZH ZH E R L Y");

		// Z D ZH -> ZH DZH
		spellTest(L"з'їзджають", "Z J I ZH DZH A J U T1");
		spellTest(L"пизджу", "P Y ZH DZH U");
	}
	TEST_CASE("compound phone NTST NTS1K")
	{
		// N T S T -> N S T
		spellTest(L"агентства", "A H E N S T V A");
		// N T S1 K -> N S1 K
		spellTest(L"студентський", "S T U D E N1 S1 K Y J");
	}
	TEST_CASE("compound phone S")
	{
		// S SH -> SH SH
		spellTest(L"принісши", "P R Y N1 I SH SH Y");
		spellTest(L"сша", "SH SH A1");
		//spellTest(L"масштаб", "M A SH T A B"); // alternative=no doubling
		// сексшоп [S E K S SH O P] two different words, no doubling
	}
	TEST_CASE("compound phone ST")
	{
		// S T D -> Z D
		spellTest(L"шістдесят", "SH2 I Z D E S1 A T");

		// S T S 1 K -> S1 K
		spellTest(L"модерністська", "M O D E R N1 I S1 K A");
		spellTest(L"нацистська", "N A TS Y S1 K A");
		spellTest(L"фашистська", "F A SH Y S1 K A");

		// S T S -> S S
		spellTest(L"постскриптум", "P O S S K R Y P T U M");
		spellTest(L"шістсот", "SH2 I S S O T");

		// S T TS -> S TS
		spellTest(L"невістці", "N E V2 I S1 TS1 I");
		spellTest(L"пастці", "P A S1 TS1 I");
	}
	TEST_CASE("compound phone TS T1S TTS TCH")
	{
		// T S -> TS
		spellTest(L"багатство", "B A H A TS T V O");
		// T 1 S -> TS
		spellTest(L"активізуються", "A K T Y V2 I Z U J U TS1 A");

		// T TS -> TS TS
		spellTest(L"отця", "O TS1 TS1 A");
		spellTest(L"куртці", "K U R TS1 TS1 I");

		// T CH -> CH CH
		spellTest(L"вітчизна", "V2 I CH CH Y Z N A");
		spellTest(L"льотчик", "L1 O CH CH Y K");
	}
	TEST_CASE("double phone YE YU YA")
	{
		SECTION("double phone YE YU YA when it the First letter") {
			// first letter is Є Ю Я
			spellTest(L"єврей", "J E V R E J");
			spellTest(L"юнак", "J U N A K");
			spellTest(L"яблуко", "J A B L U K O");
			spellTest(L"де-юре", "D E J U R E");
			spellTest(L"як-не-як", "J A K N E J A K");
			spellTest(L"один-єдиний", "O D Y N J E D Y N Y J");
			spellTest(L"по-європейськи", "P O J E V R O P E J S1 K Y");
		}
		SECTION("double phone YE YU YA after apostrophe") {
			spellTest(L"б'є", "B J E1");
			spellTest(L"б'ю", "B J U1");
			spellTest(L"пір'я", "P2 I R J A");
		}
		SECTION("double phone YE YU YA after soft sign") {
			spellTest(L"досьє", "D O S1 J E");
			spellTest(L"сьюзен", "S1 J U Z E N");
			spellTest(L"вільям", "V2 I L1 J A M");
			spellTest(L"будь-яка", "B U D1 J A K A");
		}
		SECTION("double phone YE YU YA after vowel") {
			spellTest(L"взаємно", "V Z A J E M N O");
			spellTest(L"настою", "N A S T O J U");
			spellTest(L"абияк", "A B Y J A K");
			spellTest(L"гостює", "H O S1 T1 U J E");
		}
	}
	TEST_CASE("hard consonant before vowel E") {
		spellTest(L"землею", "Z E M L E J U");
	}
	TEST_CASE("soft consonant before I")
	{
		spellTest(L"лікар", "L1 I K A R");
		spellTest(L"лісти", "L1 I S T Y");
		spellTest(L"рівень", "R1 I V E N1");
		spellTest(L"імлі", "I M L1 I");
		spellTest(L"віл", "V2 I1 L");
		spellTest(L"білка", "B2 I L K A");
	}
	TEST_CASE("soft consonant before YA YU")
	{
		spellTest(L"лякати", "L1 A K A T Y");
		spellTest(L"буря", "B U R1 A");
		spellTest(L"зоря", "Z O R1 A");
		spellTest(L"порядок", "P O R1 A D O K");
		spellTest(L"рябий", "R1 A B Y J");

		spellTest(L"грюкати", "H R1 U K A T Y");
		spellTest(L"варю", "V A R1 U");
		spellTest(L"бурю", "B U R1 U");
		spellTest(L"люди", "L1 U D Y");
		spellTest(L"селюк", "S E L1 U K");
	}
	TEST_CASE("soft Double consonant before YA YU")
	{
		spellTest(L"ілля", "I L1 L1 A");
		spellTest(L"моделлю", "M O D E L1 L1 U");
		spellTest(L"ллє", "L1 L1 E1");
		spellTest(L"суттєво", "S U T1 T1 E V O");
	}
	TEST_CASE("pair of consonants soften each other")
	{
		spellTest(L"кузня", "K U Z1 N1 A");
		spellTest(L"сніг", "S1 N1 I1 H");
		spellTest(L"молодці", "M O L O T1 TS1 I");
		spellTest(L"митці", "M Y TS1 TS1 I");
		spellTest(L"радянський", "R A D1 A N1 S1 K Y J");
		spellTest(L"гілці", "H2 I L1 TS1 I");
		spellTest(L"дні", "D1 N1 I1");
		spellTest(L"для", "D1 L1 A1");
		spellTest(L"путні", "P U T1 N1 I");
		spellTest(L"абстрактні", "A P S T R A K T1 N1 I");
		spellTest(L"тліти", "T1 L1 I T Y");
		// TODO: exception волзі [V O L Z1 I]
	}
	TEST_CASE("pair of consonants soften each other Recursive")
	{
		spellTest(L"бесслідно", "B E S1 S1 L1 I D N O");
	}
	TEST_CASE("pair of consonants damp each other (make unvoice) BP HKH DT ZHSH ZS") {
		// B->P
		spellTest(L"необхідно", "N E O P KH2 I D N O");
		// H->KH
		spellTest(L"легше", "L E KH SH E");
		// D->T
		spellTest(L"відповідає", "V2 I T P O V2 I D A J E");
		spellTest(L"підпис", "P2 I T P Y S");
		// ZH->SH
		spellTest(L"ліжка", "L1 I SH K A");
		// Z->S
		spellTest(L"безпосередньо", "B E S P O S E R E D1 N1 O");
		spellTest(L"залізти", "Z A L1 I S T Y");
		spellTest(L"розповідали", "R O S P O V2 I D A L Y");
		spellTest(L"розповсюджувати", "R O S P O V S1 U DZH U V A T Y");

		SECTION("BP HKH DT ZHSH ZS before soft sign and unvoiced consonant") {
			// D 1 ->T
			spellTest(L"дядько", "D1 A T1 K O");
			// Z 1 -> S
			spellTest(L"абхазька", "A P KH A S1 K A");
			spellTest(L"близько", "B L Y S1 K O");
			spellTest(L"вузько", "V U S1 K O");
		}
	}
	TEST_CASE("pair of consonants amplify (make voice) each other") {
		// T->D
		spellTest(L"боротьба", "B O R O D1 B A");
		spellTest(L"отже", "O D ZH E");
		spellTest(L"футбол", "F U D B O L");
		// K->G
		spellTest(L"вокзал", "V O G Z A L");
		spellTest(L"екзамен", "E G Z A M E N");
		spellTest(L"якби", "J A G B Y");
		spellTest(L"аякже", "A J A G ZH E");
		spellTest(L"великдень", "V E L Y G D E N1");
		// S->Z
		spellTest(L"осьде", "O Z1 D E");
		spellTest(L"юрисдикції", "J U R Y Z D Y K TS1 I J I");
		spellTest(L"лесбіянка", "L E Z B2 I J A N K A");
		// CH->DZH
		spellTest(L"учбових", "U DZH B O V Y KH");
		spellTest(L"лічбі", "L1 I DZH B2 I");
		spellTest(L"хоч би", "KH O DZH B Y");
	}
	TEST_CASE("apostrophe in en->uk transliteration") {
		// he's -> хі'з
		spellTest(L"хі'з", "KH2 I1 Z"); // apostrophe is ignored
	}
	TEST_CASE("vary phone softness and stress")
	{
		SECTION("no soft no stress") {
			PhoneRegistry phoneReg;
			bool allowSoftHardConsonant = false;
			bool allowVowelStress = false;
			initPhoneRegistryUk(phoneReg, allowSoftHardConsonant, allowVowelStress);
			spellTest(L"суть", "S U T", &phoneReg);
		}
		SECTION("do soft no stress") {
			PhoneRegistry phoneReg;
			bool allowSoftHardConsonant = true;
			bool allowVowelStress = false;
			initPhoneRegistryUk(phoneReg, allowSoftHardConsonant, allowVowelStress);
			spellTest(L"суть", "S U T1", &phoneReg);
		}
		SECTION("no soft do stress") {
			PhoneRegistry phoneReg;
			bool allowSoftHardConsonant = false;
			bool allowVowelStress = true;
			initPhoneRegistryUk(phoneReg, allowSoftHardConsonant, allowVowelStress);
			spellTest(L"суть", "S U1 T", &phoneReg);
		}
		SECTION("do soft do stress") {
			PhoneRegistry phoneReg;
			bool allowSoftHardConsonant = true;
			bool allowVowelStress = true;
			initPhoneRegistryUk(phoneReg, allowSoftHardConsonant, allowVowelStress);
			spellTest(L"суть", "S U1 T1", &phoneReg);
		}
	}

	TEST_CASE("palatal consonants support")
	{
		PhoneRegistry phoneReg;
		bool allowSoftHardConsonant = true;
		bool allowVowelStress = true;
		initPhoneRegistryUk(phoneReg, allowSoftHardConsonant, allowVowelStress);

		phoneReg.setPalatalSupport(PalatalSupport::AsHard);
		spellTest(L"бій", "B I1 J", &phoneReg);

		phoneReg.setPalatalSupport(PalatalSupport::AsSoft);
		spellTest(L"бій", "B1 I1 J", &phoneReg);

		phoneReg.setPalatalSupport(PalatalSupport::AsPalatal);
		spellTest(L"бій", "B2 I1 J", &phoneReg);
	}
}

// TODO:
// надсилають [N A T S Y L A J U T1] no T+S->TS on the border prefix-suffix
