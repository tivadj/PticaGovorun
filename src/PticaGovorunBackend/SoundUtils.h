#pragma once
#include "stdafx.h"
#include <tuple>
#include <string>
#include <vector>

#include <sndfile.h> // SF_VIRTUAL_IO

namespace PticaGovorun {

class SoundUtils
{
public:
    SoundUtils(void);
    ~SoundUtils(void);
};

PG_EXPORTS std::tuple<bool, std::string> readAllSamples(const std::string& fileName, std::vector<short>& result);
PG_EXPORTS std::tuple<bool, std::string> writeAllSamplesWav(short* sampleData, int sampleCount, const std::string& fileName, int sampleRate);
PG_EXPORTS std::tuple<bool, std::string> writeAllSamplesWavVirtual(short* sampleData, int sampleCount, int sampleRate, const SF_VIRTUAL_IO& virtualIO, void* userData);


} // ns