#include "stdafx.h"
#include <vector>
#include <iostream>
#include <samplerate.h>
#include "WavUtils.h" // readAllSamples

namespace ResampleAudioTesterNS
{
	void resampleWaveform()
	{
		const char* wavFilePath = R"path(E:\devb\workshop\PticaGovorunProj\data\!\2011-04-pynzenyk-q_11_0001917-0033272_22050Hz.wav)path";
		std::vector<short> audioSamples;
		auto readOp = PticaGovorun::readAllSamples(wavFilePath, audioSamples);
		if (!std::get<0>(readOp))
		{
			std::cerr << "Can't read wav file" << std::endl;
			return;
		}

		// we can cast float-short or 
		// use src_short_to_float_array/src_float_to_short_array which convert to float in [-1;1] range; both work
		std::vector<float> samplesFloat(std::begin(audioSamples), std::end(audioSamples));
		//src_short_to_float_array(audioSamples.data(), samplesFloat.data(), audioSamples.size());

		std::vector<float> targetSamplesFloat(samplesFloat.size(), 0);

		float srcSampleRate = 22050;
		float targetSampleRate = 16000;
		
		SRC_DATA convertData;
		convertData.data_in = samplesFloat.data();
		convertData.input_frames = samplesFloat.size();
		convertData.data_out = targetSamplesFloat.data();
		convertData.output_frames = targetSamplesFloat.size();
		convertData.src_ratio = targetSampleRate / srcSampleRate;

		int converterType = SRC_SINC_BEST_QUALITY;
		int channels = 1;
		int error = src_simple(&convertData, converterType, channels);
		if (error != 0)
		{
			const char* msg = src_strerror(error);
			std::cerr << msg << std::endl;
			return;
		}

		targetSamplesFloat.resize(convertData.output_frames_gen);
		std::vector<short> targetSamples(std::begin(targetSamplesFloat), std::end(targetSamplesFloat));
		//src_float_to_short_array(targetSamplesFloat.data(), targetSamples.data(), targetSamplesFloat.size());

		const char* wavOutPath = R"path(E:\devb\workshop\PticaGovorunProj\data\!\out2.wav)path";
		PticaGovorun::writeAllSamplesWav(targetSamples.data(), targetSamples.size(), wavOutPath, targetSampleRate);
	}
	void run()
	{
		resampleWaveform();
	}
}