// PticaGovorunSimpleConsoleClient.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

namespace MatlabTesterNS { void run(); }
namespace SliceTesterNS { void run(); }
namespace ComputeSpeechMfccTesterNS { void run(); }
namespace ResampleAudioTesterNS { void run(); }
namespace RecognizeSpeechSphinxTester { void run(); }

int _tmain(int argc, _TCHAR* argv[])
{
	//SliceTesterNS::run();
	//MatlabTesterNS::run();
	//ComputeSpeechMfccTesterNS::run();
	//ResampleAudioTesterNS::run();
	RecognizeSpeechSphinxTester::run();

	return 0;
}

