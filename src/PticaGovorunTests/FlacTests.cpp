#include <string>
#include <vector>
#include <boost/filesystem.hpp>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "FlacUtils.h"
#include "WavUtils.h"
#include "AppHelpers.h"

namespace PticaGovorunTests
{
	using namespace PticaGovorun;

	struct FlacTest : public testing::Test
	{
	};

	// Tests FLAC file can be read.
	// Tests that samples do match for the same file in FLAC and WAV formats.
	TEST_F(FlacTest, flacDecodeAndMatchWav)
	{
		ErrMsgList errMsg;

		// wav
		std::vector<short> samplesWav;
		float sampleRateWav = -1;
		auto wavPath = boost::filesystem::path(AppHelpers::mapPath("testdata/audio/ocin_naslidky_golodomoru.wav").toStdWString());
		bool readOp = readAllSamplesWav(wavPath, samplesWav, &sampleRateWav, &errMsg);
		ASSERT_TRUE(readOp) << str(errMsg);

		// flac
		std::vector<short> samplesFlac;
		float sampleRateFlac = -1;
		auto flacPath = boost::filesystem::path(AppHelpers::mapPath("testdata/audio/ocin_naslidky_golodomoru.flac").toStdWString());
		readOp = readAllSamplesFlac(flacPath, samplesFlac, &sampleRateFlac, &errMsg);
		ASSERT_TRUE(readOp) << str(errMsg);

		ASSERT_EQ(sampleRateFlac, sampleRateWav);
		ASSERT_THAT(samplesWav, testing::Eq(samplesFlac));
	}
}
