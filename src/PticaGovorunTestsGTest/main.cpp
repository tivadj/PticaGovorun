#include <gtest/gtest.h>
#include <QApplication>

// HACK: this definition resolves MSVC linkage error about undefined g_linked_ptr_mutex
namespace testing { namespace internal {
GTEST_API_ GTEST_DEFINE_STATIC_MUTEX_(g_linked_ptr_mutex);
}}

int main(int ac, char* av[])
{
	QApplication app(ac, av); // for QApplication::applicationDirPath() to work

	testing::InitGoogleTest(&ac, av);
	return RUN_ALL_TESTS();
}