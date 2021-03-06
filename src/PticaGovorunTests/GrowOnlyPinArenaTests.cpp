#include <string>
#include <gtest/gtest.h>
#include "ComponentsInfrastructure.h"

namespace PticaGovorunTests
{
	using namespace PticaGovorun;

	struct GrowOnlyPinArenaTest : public testing::Test
	{
	};
	TEST_F(GrowOnlyPinArenaTest, GrowOnlyPinArena1)
	{
		GrowOnlyPinArena<wchar_t> arena(5);

		std::wstring w1 = L"to";
		wchar_t* c1 = arena.put(w1.data(), w1.data() + w1.size());
		ASSERT_NE(c1, (wchar_t*)nullptr);

		// allocates new line, because "to".size + "live".size = 6 > 5
		std::wstring w2 = L"live";
		wchar_t* c2 = arena.put(w2.data(), w2.data() + w2.size());
		ASSERT_NE(c2, (wchar_t*)nullptr);

		// can't put data longer than one line
		std::wstring w3 = L"forever";
		wchar_t* c3 = arena.put(w3.data(), w3.data() + w3.size());
		ASSERT_EQ(c3, (wchar_t*)nullptr);
	}
}