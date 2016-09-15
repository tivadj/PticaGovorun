#include "stdafx.h"
#include <iostream>
#include "ClnUtils.h"

namespace SliceTesterNS
{
	void testSliceImpl()
	{
		std::vector<float> v1 = { 1, 2, 3 };
		wv::slice<float> s1 = wv::make_view(std::begin(v1), std::end(v1));
		for (auto& f : s1)
		{
			std::cout << f << std::endl;
		}

		float* pData = &v1[0];
		wv::slice<float> sPointer = wv::make_view(pData, 3);
		for (auto& f : sPointer)
		{
			std::cout << f << std::endl;
		}

		std::vector<float> vEmpty = {};
		wv::slice<float> s2 = wv::make_view(vEmpty);
		for (auto& f : s2)
		{
			std::cout << f << std::endl;
		}

		wv::slice<float> s2Copy = s2;
		for (auto& f : s2Copy)
		{
			std::cout << f << std::endl;
		}
	}

	void run()
	{
		testSliceImpl();
#if HAS_ARV_IMPL
		testArvImpl();
#endif
	}
}