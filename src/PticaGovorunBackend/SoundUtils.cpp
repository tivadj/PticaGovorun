#include "stdafx.h"
#include "SoundUtils.h"
#include <sndfile.h>
#include <array>
using namespace std;

namespace PticaGovorun {

SoundUtils::SoundUtils(void)
{
}


SoundUtils::~SoundUtils(void)
{
}

std::tuple<bool,std::string> readAllSamples(const string& fileName, vector<short>& result)
{
    SF_INFO sfInfo;
    memset(&sfInfo, 0, sizeof(sfInfo));

    SNDFILE* sf = sf_open(fileName.c_str(),SFM_READ, &sfInfo);
    if (sf == nullptr)
    {
        auto err = sf_strerror(nullptr);
        return make_tuple(false,err);
    }

    result.clear();
    result.resize(sfInfo.frames);

    long long count = 0;
    std::array<short, 32> buf;
    auto targetIt = begin(result);
    while(true)
    {
        auto readc = sf_read_short(sf, &buf[0], buf.size());
        if (readc == 0)
            break;
        count += readc;

        targetIt = std::copy(begin(buf), begin(buf) + readc, targetIt);
        //for (size_t i=0; i<buf.size(); ++i)
        //    result->Append(buf[i]);
    }

    int code = sf_close(sf);
    if (code != 0)
        return make_tuple(false, sf_strerror(nullptr));

    return make_tuple(true, string());
}

std::tuple<bool,std::string> writeAllSamplesWav(const short* sampleData, int sampleCount, const std::string& fileName, int sampleRate)
{
    SF_INFO sfInfo;
    memset(&sfInfo, 0, sizeof(sfInfo));
    sfInfo.samplerate = sampleRate;
    sfInfo.channels = 1;
    sfInfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;

    SNDFILE* sf = sf_open(fileName.c_str(),SFM_WRITE, &sfInfo);
    if (sf == nullptr)
    {
        auto err = sf_strerror(nullptr);
        return make_tuple(false,std::string(err));
    }

    auto wcount = sf_write_short(sf, sampleData, sampleCount);
    if (wcount != sampleCount)
        return make_tuple(false, std::string("Can't write all samples to the file"));

    int code = sf_close(sf);
    if (code != 0)
    {
        auto err = sf_strerror(nullptr);
        return make_tuple(false,std::string(err));
    }

    return make_tuple(true, std::string());
}

std::tuple<bool,std::string> writeAllSamplesWavVirtual(short* sampleData, int sampleCount, int sampleRate,
                                                       const SF_VIRTUAL_IO& virtualIO, void* userData)
{
    SF_INFO sfInfo;
    memset(&sfInfo, 0, sizeof(sfInfo));
    sfInfo.samplerate = sampleRate;
    sfInfo.channels = 1;
    sfInfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;

    SNDFILE* sf = sf_open_virtual(const_cast<SF_VIRTUAL_IO*>(&virtualIO), SFM_WRITE, &sfInfo, userData);
    if (sf == nullptr)
    {
        auto err = sf_strerror(nullptr);
        return make_tuple(false,std::string(err));
    }

    auto wcount = sf_write_short(sf, sampleData, sampleCount);
    if (wcount != sampleCount)
        return make_tuple(false, std::string("Can't write all samples to the file"));

    //int code = sf_close(sf);
    //if (code != 0)
    //{
    //    auto err = sf_strerror(nullptr);
    //    return make_tuple(false,std::string(err));
    //}

    return make_tuple(true, std::string());
}

} // ns