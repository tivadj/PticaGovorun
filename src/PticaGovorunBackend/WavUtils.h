#pragma once
#include <tuple>
#include <string>
#include <vector>
#include <gsl/span>
//#include <sndfile.h> // SF_VIRTUAL_IO
#include "PticaGovorunCore.h" // PG_EXPORTS
#include "ClnUtils.h"

namespace PticaGovorun {

PG_EXPORTS std::tuple<bool, std::string> readAllSamples(const std::string& fileName, std::vector<short>& result, float *frameRate = nullptr);
PG_EXPORTS std::tuple<bool, std::string> writeAllSamplesWav(const short* sampleData, int sampleCount, const std::string& fileName, int sampleRate);
//PG_EXPORTS std::tuple<bool, std::string> writeAllSamplesWavVirtual(short* sampleData, int sampleCount, int sampleRate, const SF_VIRTUAL_IO& virtualIO, void* userData);
PG_EXPORTS bool resampleFrames(gsl::span<const short> audioSamples, float inputFrameRate, float outFrameRate, std::vector<short>& outFrames, std::wstring* errMsg);

PG_EXPORTS std::tuple<bool, const char*> readAllSamplesFormatAware(const char* fileName, std::vector<short>& result, float *frameRate = nullptr);

// Checks whether the audio file format is supported.
bool isSupportedAudioFile(const wchar_t* fileName);
}
