#include "catch.hpp"
#include <vector>

#include "ClnUtils.h"
#include "TextProcessing.h"
#include "PhoneticService.h"

namespace PticaGovorun
{
	inline void spellTest(const wchar_t* word, const char* expectTranscription)
	{
		std::vector<UkrainianPhoneId> phones;
		bool spellOp;
		const char* errMsg;
		std::tie(spellOp, errMsg) = spellWord(word, phones);
		REQUIRE(spellOp);

		std::vector<UkrainianPhoneId> expectPhones;
		bool parseOp = parsePhoneListStrs(expectTranscription, expectPhones);
		REQUIRE(parseOp);
		
		bool ok = phones == expectPhones;
		REQUIRE(ok);
	}

	TEST_CASE("simple compound phones")
	{
		// DZ
		spellTest(L"дзеркало", "DZ E R K A L O");
		// DZH
		spellTest(L"бюджет", "B U DZH E T");

		SECTION("always double YI") {
			spellTest(L"воїн", "V O J I N");
		}
		SECTION("always double SH CH") {
			spellTest(L"що", "SH CH O");
		}
	}
	TEST_CASE("spell tests")
	{
		SECTION("double phone YE YU YA after apostrophe") {
			spellTest(L"б'є", "B J E");
			spellTest(L"б'ю", "B J U");
			spellTest(L"пір'я", "P I R J A");
		}
		SECTION("double phone YE YU YA after soft sign") {
			spellTest(L"досьє", "D O S J E");
			spellTest(L"сьюзен", "S J U Z E N");
			spellTest(L"вільям", "V I L J A M");
		}
		SECTION("double phone YE YU YA when it the first letter") {
			// first letter is Є Ю Я
			spellTest(L"єврей", "J E V R E J");
			spellTest(L"юнак", "J U N A K");
			spellTest(L"яблуко", "J A B L U K O");
		}
		SECTION("double phone YE YU YA after vowel") {
			spellTest(L"взаємно", "V Z A J E M N O");
			spellTest(L"настою", "N A S T O J U");
			spellTest(L"абияк", "A B Y J A K");
		}
		SECTION("single phone YE YU YA after soft consonant") {
			spellTest(L"суттєво", "S U T T E V O");
			spellTest(L"селюк", "S E L U K");
			// YA
			spellTest(L"зоряний", "Z O R A N Y J");
			spellTest(L"буря", "B U R A");
		}
		SECTION("BP HKH DT ZHSH ZS before unvoiced consonant") {
			// B->P
			spellTest(L"необхідно", "N E O P KH I D N O");
			// H->KH
			spellTest(L"легше", "L E KH SH E");
			// D->T
			spellTest(L"відповідає", "V I T P O V I D A J E");
			spellTest(L"підпис", "P I T P Y S");
			// ZH->SH
			spellTest(L"ліжка", "L I SH K A");
			// Z->S
			spellTest(L"безпосередньо", "B E S P O S E R E D N O");
			spellTest(L"залізти", "Z A L I S T Y");
			spellTest(L"розповідали", "R O S P O V I D A L Y");
			spellTest(L"розповсюджувати", "R O S P O V S U DZH U V A T Y");

			SECTION("BP HKH DT ZHSH ZS before soft sign and unvoiced consonant") {
				// D 1 ->T
				spellTest(L"дядько", "D A T K O");
				// Z 1 -> S
				spellTest(L"абхазька", "A P KH A S K A");
				spellTest(L"близько", "B L Y S K O");
				spellTest(L"вузько", "V U S K O");
			}
		}
	}
	TEST_CASE("redundant letter Z")
	{
		// Z ZH -> ZH ZH
		spellTest(L"безжально", "B E ZH ZH A L N O");
		spellTest(L"зжерли", "ZH ZH E R L Y");

		// Z D ZH -> ZH DZH
		spellTest(L"з'їзджають", "Z J I ZH DZH A J U T");
		spellTest(L"пизджу", "P Y ZH DZH U");
	}
	TEST_CASE("redundant letter NTST NTS1K")
	{
		// N T S T -> N S T
		spellTest(L"агентства", "A H E N S T V A");
		// N T S1 K -> N S1 K
		spellTest(L"студентський", "S T U D E N S K Y J");
	}
	TEST_CASE("redundant letter S")
	{
		// S SH -> SH SH
		spellTest(L"принісши", "P R Y N I SH SH Y");
		spellTest(L"сша", "SH SH A");
		//spellTest(L"масштаб", "M A SH T A B"); // alternative=no doubling
		// сексшоп [S E K S SH O P] two different words, no doubling
	}
	TEST_CASE("redundant letter ST")
	{
		// S T D -> Z D
		spellTest(L"шістдесят", "SH I Z D E S A T");

		// S T S 1 K -> S1 K
		spellTest(L"модерністська", "M O D E R N I S K A");
		spellTest(L"нацистська", "N A TS Y S K A");
		spellTest(L"фашистська", "F A SH Y S K A");

		// S T S -> S S
		spellTest(L"постскриптум", "P O S S K R Y P T U M");
		spellTest(L"шістсот", "SH I S S O T");

		// S T TS -> S TS
		spellTest(L"невістці", "N E V I S TS I");
		spellTest(L"пастці", "P A S TS I");
	}
	TEST_CASE("redundant letter T")
	{
		// T S -> TS
		spellTest(L"багатство", "B A H A TS T V O");
		// T 1 S -> TS
		spellTest(L"активізуються", "A K T Y V I Z U J U TS A");

		// T TS -> TS TS
		spellTest(L"отця", "O TS TS A");
		spellTest(L"куртці", "K U R TS TS I");

		// T CH -> CH CH
		spellTest(L"вітчизна", "V I CH CH Y Z N A");
		spellTest(L"льотчик", "L O CH CH Y K");
	}
}
