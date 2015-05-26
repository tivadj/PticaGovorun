#include "stdafx.h"
#include <array>
#include "WavUtils.h"
#include <samplerate.h>

namespace PticaGovorun {


std::tuple<bool, std::string> readAllSamples(const std::string& fileName, std::vector<short>& result, float *frameRate)
{
	SF_INFO sfInfo;
	memset(&sfInfo, 0, sizeof(sfInfo));

	SNDFILE* sf = sf_open(fileName.c_str(), SFM_READ, &sfInfo);
	if (sf == nullptr)
	{
		auto err = sf_strerror(nullptr);
		return std::make_tuple(false, err);
	}

	result.clear();
	result.resize(sfInfo.frames);
	if (frameRate != nullptr)
		*frameRate = sfInfo.samplerate;

	long long count = 0;
	std::array<short, 32> buf;
	auto targetIt = begin(result);
	while (true)
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
		return std::make_tuple(false, sf_strerror(nullptr));

	return std::make_tuple(true, std::string());
}

std::tuple<bool, std::string> writeAllSamplesWav(const short* sampleData, int sampleCount, const std::string& fileName, int sampleRate)
{
	SF_INFO sfInfo;
	memset(&sfInfo, 0, sizeof(sfInfo));
	sfInfo.samplerate = sampleRate;
	sfInfo.channels = 1;
	sfInfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;

	SNDFILE* sf = sf_open(fileName.c_str(), SFM_WRITE, &sfInfo);
	if (sf == nullptr)
	{
		auto err = sf_strerror(nullptr);
		return make_tuple(false, std::string(err));
	}

	auto wcount = sf_write_short(sf, sampleData, sampleCount);
	if (wcount != sampleCount)
		return make_tuple(false, std::string("Can't write all samples to the file"));

	int code = sf_close(sf);
	if (code != 0)
	{
		auto err = sf_strerror(nullptr);
		return make_tuple(false, std::string(err));
	}

	return make_tuple(true, std::string());
}

std::tuple<bool, std::string> writeAllSamplesWavVirtual(short* sampleData, int sampleCount, int sampleRate,
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
		return make_tuple(false, std::string(err));
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

std::tuple<bool, const char*> resampleFrames(wv::slice<short> audioSamples, float inputFrameRate, float outFrameRate, std::vector<short>& outFrames)
{
	// we can cast float-short or 
	// use src_short_to_float_array/src_float_to_short_array which convert to float in [-1;1] range; both work
	std::vector<float> samplesFloat(std::begin(audioSamples), std::end(audioSamples));
	//src_short_to_float_array(audioSamples.data(), samplesFloat.data(), audioSamples.size());

	std::vector<float> targetSamplesFloat(samplesFloat.size(), 0);

	float srcSampleRate = inputFrameRate;
	float targetSampleRate = outFrameRate;

	SRC_DATA convertData;
	convertData.data_in = samplesFloat.data();
	convertData.input_frames = samplesFloat.size();
	convertData.data_out = targetSamplesFloat.data();
	convertData.output_frames = targetSamplesFloat.size();
	convertData.src_ratio = targetSampleRate / srcSampleRate;

	int converterType = SRC_SINC_BEST_QUALITY;
	int channels = 1;
	int error = src_simple(&convertData, converterType, channels);
	if (error != 0)
	{
		const char* msg = src_strerror(error);
		return std::make_tuple(false, msg);
	}

	targetSamplesFloat.resize(convertData.output_frames_gen);
	outFrames.assign(std::begin(targetSamplesFloat), std::end(targetSamplesFloat));
	//src_float_to_short_array(targetSamplesFloat.data(), targetSamples.data(), targetSamplesFloat.size());

	return std::make_tuple(true, nullptr);
}
}