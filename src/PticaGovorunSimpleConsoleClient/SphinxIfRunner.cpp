#include "WavUtils.h"
#include "AppHelpers.h"
#include "SphinxIf.h"

namespace SphinxIfRunnerNS
{
	using namespace PticaGovorun;

	void testSphinxVoiceActivityDetection()
	{
		auto filePath = AppHelpers::mapPathBfs("../../../sphinxfe1/ogryzko_16KHz_69s.wav");
		std::vector<short> samples;
		float sampleRate = -1;
		ErrMsgList errMsg;
		if (!readAllSamplesWav(filePath, samples, &sampleRate, &errMsg))
		{
			std::cout << str(errMsg) << std::endl;
			return;
		}

		SphinxVadParams vadArgs;
		vadArgs.WindowLength = 0.025625;
		int frameSize = sampleRate * vadArgs.WindowLength;
		vadArgs.FrameRate = 100;
		int frameShift = sampleRate / vadArgs.FrameRate;
		vadArgs.PreSpeech = 20;
		vadArgs.PostSpeech = 50;
		vadArgs.StartSpeech = 10;
		vadArgs.VadThreashold = 2.0f;
		std::vector<SegmentSpeechActivity> activity;
		if (!detectVoiceActivitySphinx(samples, sampleRate, vadArgs, activity, &errMsg))
		{
			std::cout << str(errMsg) << std::endl;
			return;
		}
	}

	void run()
	{
		testSphinxVoiceActivityDetection();
	}
}
