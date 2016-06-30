#include "stdafx.h"
#include <vector>
#include <array>
#include <cassert>
#include <iostream>
#include "FlacUtils.h"
#include "WavUtils.h"

namespace FlacRunnerNS
{
	using namespace PticaGovorun;

	// Tests FLAC file can be read.
	// Tests that samples do match for the same file in FLAC and WAV formats.
	void flacDecodeAndMatchWavTest()
	{
		std::vector<short> samples;
		float frameRate = -1;
		auto readOp = readAllSamplesFlac(R"path(C:\devb\PticaGovorunProj\srcrep\data\SpeechAudio\ocin_naslidky_golodomoru.flac)path", samples, &frameRate);
		if (!std::get<0>(readOp))
			std::cerr << std::get<1>(readOp);

		// wav match
		std::vector<short> samplesWav;
		float frameRateWav = -1;
		auto readOpWav = readAllSamples(R"path(C:\devb\PticaGovorunProj\srcrep\data\SpeechAudio\ocin_naslidky_golodomoru.wav)path", samplesWav, &frameRateWav);
		if (!std::get<0>(readOpWav))
			std::cerr << std::get<1>(readOpWav);

		PG_Assert(frameRate == frameRateWav);
		PG_Assert(samples.size() == samplesWav.size());
		for (size_t i = 0; i < samples.size(); ++i)
			PG_Assert(samples[i] == samplesWav[i]);
	}

	void run()
	{
		flacDecodeAndMatchWavTest();
	}
}