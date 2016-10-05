#pragma once
#include <tuple>
#include <string>
#include <vector>
#include <gsl/span>
#include <boost/filesystem.hpp>
//#include <sndfile.h> // SF_VIRTUAL_IO
#include "PticaGovorunCore.h" // PG_EXPORTS
#include "ClnUtils.h"
#include "TranscriberUI/FileWorkspaceWidget.h"

namespace PticaGovorun {

#if PG_HAS_LIBSNDFILE
PG_EXPORTS bool readAllSamplesWav(const boost::filesystem::path& filePath, std::vector<short>& result, float *frameRate, std::wstring* errMsg);
PG_EXPORTS std::tuple<bool, std::string> writeAllSamplesWav(const short* sampleData, int sampleCount, const std::string& fileName, int sampleRate);
PG_EXPORTS bool writeAllSamplesWav(gsl::span<const short> samples, int sampleRate, const boost::filesystem::path& filePath, ErrMsgList* errMsg);
//PG_EXPORTS std::tuple<bool, std::string> writeAllSamplesWavVirtual(short* sampleData, int sampleCount, int sampleRate, const SF_VIRTUAL_IO& virtualIO, void* userData);
#endif

/// Reads audio file in any supported format (wav, flac).
PG_EXPORTS bool readAllSamplesFormatAware(const boost::filesystem::path& filePath, std::vector<short>& result, float *frameRate, std::wstring* errMsg);

// Checks whether the audio file format is supported.
bool isSupportedAudioFile(const wchar_t* fileName);

PG_EXPORTS bool resampleFrames(gsl::span<const short> audioSamples, float inputFrameRate, float outFrameRate, std::vector<short>& outFrames, std::wstring* errMsg);
}
