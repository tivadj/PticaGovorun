// PticaGovorunSimpleConsoleClient.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

namespace MatlabTesterNS { void run(); }
namespace SliceTesterNS { void run(); }
namespace ComputeSpeechMfccTesterNS { void run(); }
namespace ResampleAudioTesterNS { void run(); }
namespace RecognizeSpeechSphinxTester { void run(); }
namespace EditDistanceTestsNS { void run(); }
namespace MigrateXmlSpeechAnnotRunnerNS { void run(); }
namespace PhoneticSpellerTestsNS { void run(); }
namespace PronunciationChecksRunnerNS { void run(); }
namespace RunPrepareTrainModelSphinxNS { void run(); }
namespace PdfReaderRunnerNS { void run(); }
namespace RunTextParserNS { void runMain(int argc, wchar_t* argv[]); }
namespace RunBuildLanguageModelNS { void runMain(int argc, wchar_t* argv[]); }

int _tmain(int argc, TCHAR* argv[])
{
	//SliceTesterNS::run();
	//MatlabTesterNS::run();
	//ComputeSpeechMfccTesterNS::run();
	//ResampleAudioTesterNS::run();
	//RecognizeSpeechSphinxTester::run();
	//EditDistanceTestsNS::run();
	//MigrateXmlSpeechAnnotRunnerNS::run();
	//PhoneticSpellerTestsNS::run();
	//PronunciationChecksRunnerNS::run();
	//RunPrepareTrainModelSphinxNS::run();
	//PdfReaderRunnerNS::run();
	//RunTextParserNS::runMain(argc, argv);
	RunBuildLanguageModelNS::runMain(argc, argv);

	return 0;
}

