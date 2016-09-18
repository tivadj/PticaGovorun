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
		std::wstring errMsg;

		// wav
		std::vector<short> samplesWav;
		float frameRateWav = -1;
		auto wavPath = boost::filesystem::path(AppHelpers::mapPath("testdata/audio/ocin_naslidky_golodomoru.wav").toStdWString());
		bool readOp = readAllSamplesWav(wavPath, samplesWav, &frameRateWav, &errMsg);
		ASSERT_TRUE(readOp) << errMsg;

		// flac
		std::vector<short> samplesFlac;
		float frameRateFlac = -1;
		auto flacPath = boost::filesystem::path(AppHelpers::mapPath("testdata/audio/ocin_naslidky_golodomoru.flac").toStdWString());
		readOp = readAllSamplesFlac(flacPath, samplesFlac, &frameRateFlac, &errMsg);
		ASSERT_TRUE(readOp) << errMsg;

		ASSERT_EQ(frameRateFlac, frameRateWav);
		ASSERT_THAT(samplesWav, testing::Eq(samplesFlac));
	}
}
