#include <vector>
#include <array>
#include <iostream>
#include <fstream>
#include <locale>
#include <windows.h>

#include "SpeechProcessing.h"
#include "PhoneticService.h"
#include <CoreUtils.h>

namespace PronunciationChecksRunnerNS
{
	using namespace PticaGovorun;

	void checkWordUniquenessPronunciation()
	{
		QTextCodec* pTextCodec = QTextCodec::codecForName("windows-1251");
		std::map<std::wstring, std::vector<Pronunc>> wordToPhoneListDict;

		normalizePronunciationVocabulary(wordToPhoneListDict);

		//
		const wchar_t* wavRootDir = LR"path(C:\devb\PticaGovorunProj\srcrep\data\SpeechAudio\)path";
		const wchar_t* annotRootDir = LR"path(C:\devb\PticaGovorunProj\srcrep\data\SpeechAnnot\)path";
		const wchar_t* wavDirToAnalyze = LR"path(C:\devb\PticaGovorunProj\srcrep\data\SpeechAudio\)path";

		std::vector<AnnotatedSpeechSegment> segments;
		auto segPredBeforeFun = [](const AnnotatedSpeechSegment& seg) -> bool { return true; };
		ErrMsgList errMsg;
		bool removeSilenceAnnot = true;
		bool padSilStart = true;
		bool padSilEnd = true;
		bool removeInterSpeechSilence = true;
		bool loadOp = loadSpeechAndAnnotation(QFileInfo(QString::fromWCharArray(wavDirToAnalyze)), wavRootDir, annotRootDir, MarkerLevelOfDetail::Word, false, removeSilenceAnnot, removeInterSpeechSilence, padSilStart, padSilEnd, 0, segPredBeforeFun, segments, &errMsg);

		//

		std::stringstream dumpFileName;
		dumpFileName << "wordMultiplePronsDump.";
		appendTimeStampNow(dumpFileName);
		dumpFileName << ".txt";

		QFile dumpFile(dumpFileName.str().c_str());
		if (!dumpFile.open(QIODevice::WriteOnly | QIODevice::Text))
		{
			std::cerr << "Can't open ouput file" << std::endl;
			return;
		}

		QTextStream dumpFileStream(&dumpFile);
		dumpFileStream.setCodec("UTF-8");

		//

		std::vector<boost::wstring_view> words;
		std::wstring word;
		word.reserve(32);
		GrowOnlyPinArena<wchar_t> arena(1024);
		for (const AnnotatedSpeechSegment& seg : segments)
		{
			words.clear();
			arena.clear();
			splitUtteranceIntoPronuncList(seg.TranscriptText, arena, words);

			// iterate through words
			for (boost::wstring_view wordRef : words)
			{
				toStdWString(wordRef, word);

				auto it = wordToPhoneListDict.find(word);
				if (it != std::end(wordToPhoneListDict))
				{
					const std::vector<Pronunc>& prons = it->second;
					if (prons.size() > 1) // word has multiple pronunciations
					{
						dumpFileStream << QString::fromStdWString(seg.AudioFilePath) << "\n";
						dumpFileStream << QString::fromStdWString(word) << "\n";
						for (int pronInd = 0; pronInd < prons.size(); pronInd++)
						{
							dumpFileStream << prons[pronInd].StrDebug.c_str() << "\n";
						}

						dumpFileStream << "\n";
					}
				}
			}
		}
		dumpFile.close();
	}

	void run()
	{
		checkWordUniquenessPronunciation();
	}
}

