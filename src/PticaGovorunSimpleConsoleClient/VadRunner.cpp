#include "WavUtils.h"
#include "AppHelpers.h"
#include "SpeechProcessing.h"

namespace VadRunnerNS
{
	using namespace PticaGovorun;

	void testPGVad()
	{
		auto filePath = AppHelpers::mapPathBfs("data/audiodb/wordSoundUk_0000ms-0001ms.wav");
		std::vector<short> samples;
		float sampleRate = -1;
		ErrMsgList errMsg;
		if (!readAllSamplesWav(filePath, samples, &sampleRate, &errMsg))
		{
			std::cout << str(errMsg) << std::endl;
			return;
		}

		float targSampRate = 16000;
		std::vector<short> targSamples;
		if (!resampleFrames(samples, sampleRate, targSampRate, targSamples, &errMsg))
		{
			std::cout << str(errMsg) << std::endl;
			return;
		}

		std::vector<SegmentSpeechActivity> activity;
		if (!pgDetectVoiceActivity(samples, sampleRate, activity, &errMsg))
		{
			std::cout << str(errMsg) << std::endl;
			return;
		}
		std::cout << activity.size() << "\n";
	}

	void run()
	{
		testPGVad();
	}
}
