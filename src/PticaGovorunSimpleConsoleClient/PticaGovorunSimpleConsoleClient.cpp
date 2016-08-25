// PticaGovorunSimpleConsoleClient.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <iostream>
#include <chrono> // std::chrono::system_clock
#include <QtCore>
#include <AppHelpers.h>

#ifdef WIN32
#include <io.h> // _setmode
#include <fcntl.h> // _O_U16TEXT
#endif

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
namespace RunTextParserNS { void run(); }
namespace UkrainianPhoneticSplitterNS { void run(); }
namespace RunBuildLanguageModelNS { void runMain(int argc, wchar_t* argv[]); }
namespace PrepareSphinxTrainDataNS { void run(); }
namespace DslDictionaryConvertRunnerNS { void run(); }
namespace FlacRunnerNS { void run(); }

int mainCore(int argc, char* argv[])
{
	QString taskStr = PticaGovorun::AppHelpers::configParamQString("task", "");
	if (taskStr == "createSpeechModel")
	{
		PrepareSphinxTrainDataNS::run(); // 1
		return 0;
	}
	if (taskStr == "decode")
	{
		RecognizeSpeechSphinxTester::run(); // 2
		return 0;
	}

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
	//RunTextParserNS::run();
	UkrainianPhoneticSplitterNS::run();
	//RunBuildLanguageModelNS::runMain(argc, argv);
	//RecognizeSpeechInBatchTester::runMain(argc, argv);
	//DslDictionaryConvertRunnerNS::run();
	//FlacRunnerNS::run();
	return 0;
}

int _tmain(int argc, char* argv[])
{
	typedef std::chrono::system_clock Clock;
	std::chrono::time_point<Clock> now1 = Clock::now();

#ifdef WIN32
	// enable unicode for console
	// further, only std::wcout (not std::cout) calls are allowed http://stackoverflow.com/questions/2492077/output-unicode-strings-in-windows-console-app
	_setmode(_fileno(stdout), _O_U16TEXT);
	_setmode(_fileno(stderr), _O_U16TEXT);
#endif

	QCoreApplication a(argc, argv); // initialize for QApplication::applicationFilePath() to work

	int result = mainCore(argc, argv);

	std::chrono::time_point<Clock> now2 = Clock::now();
	auto elapsedSec = std::chrono::duration_cast<std::chrono::seconds>(now2 - now1).count();
	std::wcout << L"main exit, elapsed=" << elapsedSec << L"s" << std::endl;

	return result;
}
