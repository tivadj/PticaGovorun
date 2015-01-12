#include "stdafx.h"
#include <vector>
#include <iostream>
#include <numeric>
#include <cmath> // M_PI
#include "InteropPython.h"
#include "WavUtils.h"
#include "JuliusToolNativeWrapper.h"

namespace ComputeSpeechMfccTesterNS
{
	using namespace PticaGovorun;
	void computeMfccOnSpeechTest()
	{
		// initialize Julius
		createPhoneToMfccFeaturesMap();

		// load wav file
		const char* wavFilePath = R"path(E:\devb\workshop\PticaGovorunProj\data\prokopeo_specific\prokopeo_uk_vowels_201412101116.wav)path";
		std::vector<short> audioSamples;
		auto readOp = PticaGovorun::readAllSamples(wavFilePath, audioSamples);
		if (!std::get<0>(readOp))
		{
			std::cerr << "Can't read wav file" << std::endl;
			return;
		}

		int frameSize = 400;
		int frameShift = 160;
		int mfccVecLen = 39;

		long begSampleInd = 4696;
		long endSampleInd = 8467;
		//int len = endSampleInd - begSampleInd;
		int len = frameSize;

		// collect features
		// note, this call requires initialization of Julius library
		int framesCount;
		std::vector<float> mfccFeatures;
		auto featsOp = PticaGovorun::computeMfccFeaturesPub(&audioSamples[begSampleInd], len, frameSize, frameShift, mfccVecLen, mfccFeatures, framesCount);
		if (!std::get<0>(featsOp))
		{
			auto msg = std::get<1>(featsOp);
			std::cerr << msg << std::endl;
			return;
		}
	}

	void PG_ComputeMfccOnSpeechTest()
	{
		// load wav file
		const char* wavFilePath = R"path(E:\devb\workshop\PticaGovorunProj\data\prokopeo_specific\prokopeo_uk_vowels_201412101116.wav)path";
		std::vector<short> audioSamples;
		auto readOp = PticaGovorun::readAllSamples(wavFilePath, audioSamples);
		if (!std::get<0>(readOp))
		{
			std::cerr << "Can't read wav file" << std::endl;
			return;
		}

		long begSampleInd = 4696;
		long endSampleInd = 8467;
		int len = endSampleInd - begSampleInd;
		//int len = frameSize;

		//

		int frameSize = 400;
		int frameShift = 160;
		float sampleRate = 22050;

		// init filter bank
		int binCount = 24; // number of bins in the triangular filter bank
		int fftNum = getMinDftPointsCount(frameSize);
		TriangularFilterBank filterBank;
		buildTriangularFilterBank(sampleRate, binCount, fftNum, filterBank);


		const int mfccCount = 12;
		// +1 for usage of cepstral0 coef
		// *3 for velocity and acceleration coefs
		int mfccVecLen = 3 * (mfccCount + 1);

		int framesCount = getSplitFramesCount(len, frameSize, frameShift);
		std::vector<float> mfccFeatures(mfccVecLen*framesCount, 0);
		wv::slice<short> samplesPart = wv::make_view(audioSamples.data() + begSampleInd, len);
		computeMfccVelocityAccel(samplesPart, frameSize, frameShift, framesCount, mfccCount, mfccVecLen, filterBank, mfccFeatures);
	}

	void myAreEqual(float expect, float actual, float eps = 0.0001)
	{
		if (std::abs(expect - actual) >= eps)
		{
			std::cerr << "expect=" << expect << " actual=" << actual << std::endl;
			throw std::exception("assert failed");
		}
	}

	void testPreEmphasis()
	{
		std::vector<float> x = { 1, 3, 2, 2, 6, 1 };
		preEmphasisInplace(wv::make_view(x), 0.9);
		myAreEqual(0.1, x[0]);
		myAreEqual(2.1, x[1]);
		myAreEqual(-4.4, x[5]);
	}

	void linearSpaceTest()
	{
		std::vector<float> points(3);
		linearSpace(1, 2, 3, points);
		myAreEqual(1.0, points[0]);
		myAreEqual(1.5, points[1]);
		myAreEqual(2.0, points[2]);
	}

	void run()
	{
		//computeMfccOnSpeechTest();

		testPreEmphasis();
		linearSpaceTest();

		PG_ComputeMfccOnSpeechTest();
	}
}