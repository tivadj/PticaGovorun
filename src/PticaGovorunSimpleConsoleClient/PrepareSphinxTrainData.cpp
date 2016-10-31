#include "SphinxModel.h"
#include <iostream>
#include <QString>

namespace PrepareSphinxTrainDataNS
{
	using namespace PticaGovorun;
	void run()
	{
		SphinxTrainDataBuilder bld;

		ErrMsgList errMsg;
		if (!bld.run(&errMsg))
		{
			auto msg = combineErrorMessages(errMsg).toStdWString();
			std::wcerr << msg << std::endl;
		}
	}
}