#pragma once
#include <tuple>
#include <string>
#include <vector>
#include "PticaGovorunCore.h" // PG_EXPORTS

#ifdef PG_HAS_FLAC
namespace PticaGovorun
{
	// Read all audio samples from FLAC (Free Lossless Audio Codec) audio file.
	PG_EXPORTS std::tuple<bool, const char*> readAllSamplesFlac(const char* fileName, std::vector<short>& result, float *frameRate = nullptr);
}
#endif