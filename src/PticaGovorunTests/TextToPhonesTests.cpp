#include "catch.hpp"
#include <vector>
#include <boost/utility/string_ref.hpp>

#include "SpeechProcessing.h"

namespace PticaGovorun
{
	TEST_CASE("split utterance into words")
	{
		std::vector<boost::wstring_ref> pronCodes;
		
		SECTION("simple") {
			splitUtteranceIntoWords(L"пір'я із-за", pronCodes);
			REQUIRE(pronCodes.size() == 2);
			REQUIRE(pronCodes[0].compare(L"пір'я") == 0);
			REQUIRE(pronCodes[1].compare(L"із-за") == 0);
		}
		SECTION("filler words") {
			splitUtteranceIntoWords(L"<s> happy <sil> sheep </s>", pronCodes);
			REQUIRE(pronCodes.size() == 5);
			REQUIRE(pronCodes[0].compare(L"<s>") == 0);
			REQUIRE(pronCodes[1].compare(L"happy") == 0);
			REQUIRE(pronCodes[2].compare(L"<sil>") == 0);
			REQUIRE(pronCodes[3].compare(L"sheep") == 0);
			REQUIRE(pronCodes[4].compare(L"</s>") == 0);
		}
	}
}