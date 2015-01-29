#pragma once
#include <tuple>
#include <string>
#include <vector>

#include <sndfile.h> // SF_VIRTUAL_IO
#include "PticaGovorunCore.h" // PG_EXPORTS

namespace PticaGovorun {

PG_EXPORTS std::tuple<bool, std::string> readAllSamples(const std::string& fileName, std::vector<short>& result, float *frameRate = nullptr);
PG_EXPORTS std::tuple<bool, std::string> writeAllSamplesWav(const short* sampleData, int sampleCount, const std::string& fileName, int sampleRate);
PG_EXPORTS std::tuple<bool, std::string> writeAllSamplesWavVirtual(short* sampleData, int sampleCount, int sampleRate, const SF_VIRTUAL_IO& virtualIO, void* userData);
}
