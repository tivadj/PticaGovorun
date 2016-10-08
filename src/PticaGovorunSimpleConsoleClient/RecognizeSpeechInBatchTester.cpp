#include <vector>
#include <array>
#include <iostream>
#include <QFile>
#include <QTextStream>

#include "AudioSpeechDecoder.h"
#include "WavUtils.h" // readAllSamples
#include "PhoneticService.h"
#include "CoreUtils.h"

namespace RecognizeSpeechInBatchTester
{
	using namespace PticaGovorun;
	const float CmuSphinxSampleRate = 16000;

	void recognizeSpeechInBatch(int argc, wchar_t* argv[])
	{
		std::wstring audioFilePath = LR"path(C:\devb\PticaGovorunProj\srcrep\data\SpeechAudio\finance.ua-pynzenykvm\2011-04-pynzenyk-q_17.wav)path";
		if (argc > 1)
			audioFilePath = argv[1];

		float sampleRate = -1;
		ErrMsgList errMsg;
		std::vector<short> audioSamples;
		audioSamples.reserve(128000);
		if (!readAllSamplesFormatAware(QString::fromStdWString(audioFilePath).toStdString(), audioSamples, &sampleRate, &errMsg))
		{
			std::cerr << "Can't read wav file. " <<str(errMsg) <<std::endl;
			return;
		}

		std::vector<short> audioSamplesSphinx;
		if (!resampleFrames(audioSamples, sampleRate, CmuSphinxSampleRate, audioSamplesSphinx, &errMsg))
		{
			std::cerr << str(errMsg) << std::endl;
			return;
		}

		//
		std::stringstream dumpFileName;
		dumpFileName << "wordsBatch.";
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
		const char* hmmPath       = R"path(C:\devb\PticaGovorunProj\data\TrainSphinx\persian\model_parameters\persian.cd_cont_200\)path";
		const char* langModelPath = R"path(C:\devb\PticaGovorunProj\data\TrainSphinx\persian\etc\persian.arpa)path";
		const char* dictPath =      R"path(C:\devb\PticaGovorunProj\data\TrainSphinx\persian\etc\persian.dic)path";
		
		AudioSpeechDecoder decoder;
		decoder.init(hmmPath, langModelPath, dictPath);

		int framesProcessed = -1;
		std::vector<std::wstring> words;
		const float utterenceSec = 10;
		std::vector<short> utterFrames;
		for (int decodeFrameInd = 0; decodeFrameInd < audioSamplesSphinx.size();)
		{
			int endFrameInd = decodeFrameInd + utterenceSec * CmuSphinxSampleRate;
			endFrameInd = std::min(endFrameInd, (int)audioSamplesSphinx.size());
			utterFrames.assign(audioSamplesSphinx.data()+decodeFrameInd, audioSamplesSphinx.data()+endFrameInd);

			words.clear();
			decoder.decode(utterFrames, CmuSphinxSampleRate, words, framesProcessed);
			if (decoder.hasError())
				break;

			decodeFrameInd += framesProcessed;

			//
			for (const std::wstring& word : words)
			{
				if (word.compare(fillerStartSilence().data()) ==  0 ||
					word.compare(fillerEndSilence().data()) == 0)
					continue;
				if (word.compare(fillerInhale().data()) == 0)
					dumpFileStream <<"\n";
				dumpFileStream << QString::fromStdWString(word) << " ";
			}
			dumpFileStream.flush();
		}
		if (decoder.hasError())
		{
			std::cerr <<"Error: " << decoder.getErrorText() << std::endl;
			return;
		}
	}

	void runMain(int argc, wchar_t* argv[])
	{
		recognizeSpeechInBatch(argc, argv);
	}
}