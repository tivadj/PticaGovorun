#pragma once
#include <tuple>
#include <string>
#include <vector>
#include <boost/filesystem.hpp>
#include "PticaGovorunCore.h" // PG_EXPORTS
#include "ComponentsInfrastructure.h"

#ifdef PG_HAS_FLAC
namespace PticaGovorun
{
	// Read all audio samples from FLAC (Free Lossless Audio Codec) audio file.
	PG_EXPORTS bool readAllSamplesFlac(const boost::filesystem::path& filePath, std::vector<short>& result, float *sampleRate, ErrMsgList* errMsg);
}
#endif