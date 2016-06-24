// PticaGovorunSimpleConsoleClient.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <iostream>
#include <chrono> // std::chrono::system_clock

namespace MatlabTesterNS { void run(); }
namespace SliceTesterNS { void run(); }
namespace ComputeSpeechMfccTesterNS { void run(); }
namespace ResampleAudioTesterNS { void run(); }
namespace RecognizeSpeechSphinxTester { void run(); }
namespace RecognizeSpeechInBatchTester { void runMain(int argc, wchar_t* argv[]); }
namespace EditDistanceTestsNS { void run(); }
namespace MigrateXmlSpeechAnnotRunnerNS { void run(); }
namespace PhoneticSpellerTestsNS { void run(); }
namespace StressedSyllableRunnerNS { void run(); }
namespace PronunciationChecksRunnerNS { void run(); }
namespace RunPrepareTrainModelSphinxNS { void run(); }
namespace PdfReaderRunnerNS { void run(); }
namespace RunTextParserNS { void runMain(int argc, wchar_t* argv[]); }
namespace RunBuildLanguageModelNS { void runMain(int argc, wchar_t* argv[]); }
namespace PrepareSphinxTrainDataNS { void run(); }
namespace DslDictionaryConvertRunnerNS { void run(); }
namespace FlacRunnerNS { void run(); }

int mainCore(int argc, TCHAR* argv[])
{
	//SliceTesterNS::run();
	//MatlabTesterNS::run();
	//ComputeSpeechMfccTesterNS::run();
	//ResampleAudioTesterNS::run();
	//EditDistanceTestsNS::run();
	//MigrateXmlSpeechAnnotRunnerNS::run();
	//PhoneticSpellerTestsNS::run();
	//StressedSyllableRunnerNS::run();
	//PronunciationChecksRunnerNS::run();
	//RunPrepareTrainModelSphinxNS::run();
	//PdfReaderRunnerNS::run();
	//RunTextParserNS::runMain(argc, argv);
	//RunBuildLanguageModelNS::runMain(argc, argv);
	//PrepareSphinxTrainDataNS::run(); // 1
	RecognizeSpeechSphinxTester::run(); // 2
	//RecognizeSpeechInBatchTester::runMain(argc, argv);
	//DslDictionaryConvertRunnerNS::run();
	//FlacRunnerNS::run();
	return 0;
}

int _tmain(int argc, TCHAR* argv[])
{
	typedef std::chrono::system_clock Clock;
	std::chrono::time_point<Clock> now1 = Clock::now();

	int result = mainCore(argc, argv);

	std::chrono::time_point<Clock> now2 = Clock::now();
	auto elapsedSec = std::chrono::duration_cast<std::chrono::seconds>(now2 - now1).count();
	std::cout << "main exit, elapsed=" << elapsedSec << "s" << std::endl;

	return result;
}