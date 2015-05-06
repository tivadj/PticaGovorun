#include "stdafx.h"
#include <vector>
#include <array>
#include <iostream>
#include <fstream>
#include <locale>
#include <windows.h>

#include "SpeechProcessing.h"
#include "PhoneticService.h"
#include <CoreUtils.h>
#include <QTextStream>
#include <yaml-cpp/yaml.h>

namespace PronunciationChecksRunnerNS
{
	using namespace PticaGovorun;

	void checkWordUniquenessPronunciation()
	{
		QTextCodec* pTextCodec = QTextCodec::codecForName("windows-1251");
		std::map<std::wstring, std::vector<Pronunc>> wordToPhoneListDict;
		//const wchar_t* shrekkyDic = LR"path(C:\devb\PticaGovorunProj\data\shrekky\shrekkyDic.voca)path";
		//const wchar_t* shrekkyDic = LR"path(C:\devb\PticaGovorunProj\data\TrainSphinx\SpeechModels\newvoca_pynz.voca)path";
		bool loadOp;
		const char* errMsg;
		//std::tie(loadOp, errMsg) = loadPronunciationVocabulary2(shrekkyDic, wordToPhoneListDict, *pTextCodec); // TODO:
		//if (!loadOp)
		//	return;

		//const wchar_t* outDict = LR"path(C:\devb\PticaGovorunProj\data\TrainSphinx\SpeechModels\phoneticKnown.yml)path";
		//std::vector<PhoneticWord> phoneticDict;
		//for (auto& pair : wordToPhoneListDict)
		//{
		//	PhoneticWord pw;
		//	pw.Word = pair.first;

		//	std::vector<std::string> phonesStr;
		//	parsePhoneListStrs(QString::fromStdString(pair.second[0].StrDebug).toUpper().toStdString(), phonesStr);
		//	
		//	PronunciationFlavour pron;
		//	pron.PronAsWord = pair.first;
		//	pron.PhoneStrs = phonesStr;
		//	pw.Pronunciations = {pron};
		//	phoneticDict.push_back(pw);
		//}
		//savePhoneticDictionaryYaml(phoneticDict, outDict);

		normalizePronunciationVocabulary(wordToPhoneListDict);

		//
		const wchar_t* wavRootDir = LR"path(C:\devb\PticaGovorunProj\srcrep\data\SpeechAudio\)path";
		const wchar_t* annotRootDir = LR"path(C:\devb\PticaGovorunProj\srcrep\data\SpeechAnnot\)path";
		const wchar_t* wavDirToAnalyze = LR"path(C:\devb\PticaGovorunProj\srcrep\data\SpeechAudio\)path";

		std::vector<AnnotatedSpeechSegment> segments;
		auto segPredBeforeFun = [](const AnnotatedSpeechSegment& seg) -> bool { return true; };
		std::tie(loadOp, errMsg) = loadSpeechAndAnnotation(QFileInfo(QString::fromWCharArray(wavDirToAnalyze)), wavRootDir, annotRootDir, MarkerLevelOfDetail::Word, false, segPredBeforeFun, segments);

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

		std::vector<wv::slice<const wchar_t>> wordsAsSlices;
		std::wstring word;
		word.reserve(32);
		for (const AnnotatedSpeechSegment& seg : segments)
		{

			wordsAsSlices.clear();
			splitUtteranceIntoWords(seg.TranscriptText, wordsAsSlices);

			// iterate through words
			for (wv::slice<const wchar_t>& wordSlice : wordsAsSlices)
			{
				word.clear();
				word.assign(wordSlice.begin(), wordSlice.end());

				auto it = wordToPhoneListDict.find(word);
				if (it != std::end(wordToPhoneListDict))
				{
					const std::vector<Pronunc>& prons = it->second;
					if (prons.size() > 1) // word has multiple pronunciations
					{
						dumpFileStream << QString::fromStdWString(seg.FilePath) << "\n";
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
		//testYaml();
		//testYamlEmitter();
		//testLoadYaml();
		checkWordUniquenessPronunciation();
	}
}

