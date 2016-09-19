#include "SphinxModel.h"
#include <iostream>
#include <QString>

namespace PrepareSphinxTrainDataNS
{
	using namespace PticaGovorun;
	void run()
	{
		SphinxTrainDataBuilder bld;
		bld.run();
		if (!bld.errMsg_.isEmpty())
			std::wcerr << bld.errMsg_.toStdWString() <<std::endl;
	}
}