#include "stdafx.h"
#include <vector>
#include <array>
#include <iostream>
#include <QFile>
#include <QTextStream>

#include <samplerate.h>

#include "AudioSpeechDecoder.h"
#include "WavUtils.h" // readAllSamples
#include "PhoneticService.h"
#include "CoreUtils.h"

namespace RecognizeSpeechInBatchTester
{
	using namespace PticaGovorun;
	const float CmuSphinxFrameRate = 16000;

	std::tuple<bool,const char*> convertFrameRate(const std::vector<short>& srcFrames, float srcFrameRate, float requiredFrameRate, std::vector<short>& dstFrames)
	{
		if (srcFrameRate == requiredFrameRate)
		{
			dstFrames = srcFrames;
			return std::make_tuple(true, nullptr);
		}

		// cast to float
		std::vector<float> srcFramesFloat(std::begin(srcFrames), std::end(srcFrames));

		// prepare output buffer
		double rateRatio = requiredFrameRate / srcFrameRate;
		size_t dstFramesCount = static_cast<size_t>(srcFrames.size() * rateRatio);
		std::vector<float> targetSamplesFloat(dstFramesCount, 0);
		
		//
		SRC_DATA convertData;
		convertData.data_in = srcFramesFloat.data();
		convertData.input_frames = srcFramesFloat.size();
		convertData.data_out = targetSamplesFloat.data();
		convertData.output_frames = targetSamplesFloat.size();
		convertData.src_ratio = rateRatio;

		int converterType = SRC_SINC_BEST_QUALITY;
		int channels = 1;
		int error = src_simple(&convertData, converterType, channels);
		if (error != 0)
		{
			const char* msg = src_strerror(error);
			return std::make_tuple(false, msg);
		}

		targetSamplesFloat.resize(convertData.output_frames_gen);
		dstFrames = std::vector<short>(std::begin(targetSamplesFloat), std::end(targetSamplesFloat));
		return std::make_tuple(true, nullptr);
	}

	void recognizeSpeechInBatch(int argc, wchar_t* argv[])
	{
		std::wstring audioFilePath = LR"path(C:\devb\PticaGovorunProj\srcrep\data\SpeechAudio\finance.ua-pynzenykvm\2011-04-pynzenyk-q_17.wav)path";
		if (argc > 1)
			audioFilePath = argv[1];

		float frameRate = -1;
		std::vector<short> audioFrames;
		audioFrames.reserve(128000);
		auto readOp = readAllSamples(QString::fromStdWString(audioFilePath).toStdString(), audioFrames, &frameRate);
		if (!std::get<0>(readOp))
		{
			std::cerr << "Can't read wav file" <<std::endl;
			return;
		}

		std::vector<short> audioFramesSphinx;
		bool convOp;
		const char* errMsg;
		std::tie(convOp, errMsg) = convertFrameRate(audioFrames, frameRate, CmuSphinxFrameRate, audioFramesSphinx);
		if (!convOp)
		{
			std::cerr << errMsg << std::endl;
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
		const char* langModelPath = R"path(C:\devb\PticaGovorunProj\srcrep\build\x64\Release\persian.lm.DMP)path";
		const char* dictPath =      R"path(C:\devb\PticaGovorunProj\srcrep\build\x64\Release\persianDic.txt)path";
		
		AudioSpeechDecoder decoder;
		decoder.init(hmmPath, langModelPath, dictPath);

		int framesProcessed = -1;
		std::vector<std::wstring> words;
		const float utterenceSec = 5;
		std::vector<short> utterFrames;
		for (int decodeFrameInd = 0; decodeFrameInd < audioFramesSphinx.size();)
		{
			int endFrameInd = decodeFrameInd + utterenceSec * CmuSphinxFrameRate;
			endFrameInd = std::min(endFrameInd, (int)audioFramesSphinx.size());
			utterFrames.assign(audioFramesSphinx.data()+decodeFrameInd, audioFramesSphinx.data()+endFrameInd);

			words.clear();
			decoder.decode(utterFrames, CmuSphinxFrameRate, words, framesProcessed);
			if (decoder.hasError())
				break;

			decodeFrameInd += framesProcessed;

			//
			for (const std::wstring& word : words)
			{
				if (word == L"<s>" || word == L"</s>")
					continue;
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