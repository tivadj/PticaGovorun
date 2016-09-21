#include <array>
#include <QString>
#if PG_HAS_LIBSNDFILE
#include <sndfile.h> // SF_VIRTUAL_IO
#endif

#if PG_HAS_SAMPLERATE
#include <samplerate.h>
#endif

#include "WavUtils.h"
#include "FlacUtils.h"
#include "CoreUtils.h"

namespace PticaGovorun {

#if PG_HAS_LIBSNDFILE
bool readAllSamplesWav(const boost::filesystem::path& filePath, std::vector<short>& result, float *frameRate, std::wstring* errMsg)
{
	SF_INFO sfInfo;
	memset(&sfInfo, 0, sizeof(sfInfo));

	SNDFILE* sf = sf_open(filePath.string().c_str(), SFM_READ, &sfInfo);
	if (sf == nullptr)
	{
		auto err = sf_strerror(nullptr);
		*errMsg = QString::fromLatin1(err).toStdWString();
		return false;
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
	}

	int code = sf_close(sf);
	if (code != 0)
	{
		auto err = sf_strerror(nullptr);
		*errMsg = QString::fromLatin1(err).toStdWString();
		return false;
	}

	return true;
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

#endif

bool resampleFrames(gsl::span<const short> audioSamples, float inputFrameRate, float outFrameRate, std::vector<short>& outFrames, std::wstring* errMsg)
{
#ifndef PG_HAS_SAMPLERATE
	if (errMsg != nullptr) *errMsg = L"Resampling is not available because libsamplerate is not compiled in. Specify PG_HAS_SAMPLERATE C++ preprocessor directive";
	return false;
#else
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
		*errMsg = QString::fromLatin1(msg).toStdWString();
		return false;
	}

	targetSamplesFloat.resize(convertData.output_frames_gen);
	outFrames.assign(std::begin(targetSamplesFloat), std::end(targetSamplesFloat));
	//src_float_to_short_array(targetSamplesFloat.data(), targetSamples.data(), targetSamplesFloat.size());

	return true;
#endif // PG_HAS_SAMPLERATE
}

bool readAllSamplesFormatAware(const boost::filesystem::path& filePath, std::vector<short>& result, float *frameRate, std::wstring* errMsg)
{
	QString fileNameQ = toQString(filePath.wstring());
#ifdef PG_HAS_FLAC
	if (fileNameQ.endsWith(".flac"))
		return readAllSamplesFlac(filePath, result, frameRate, errMsg);
#endif
#ifdef PG_HAS_LIBSNDFILE
	if (fileNameQ.endsWith(".wav"))
		return readAllSamplesWav(filePath, result, frameRate, errMsg);
#endif
	*errMsg = std::wstring(L"Error: unknown audio file extension ") + filePath.extension().wstring();
	return false;
}

bool isSupportedAudioFile(const wchar_t* fileName)
{
	QString fileNameQ = QString::fromWCharArray(fileName);
#ifdef PG_HAS_FLAC
	if (fileNameQ.endsWith(".flac"))
		return true;
#endif
#ifdef PG_HAS_LIBSNDFILE
	if (fileNameQ.endsWith(".wav"))
		return true;
#endif
	return false;
}

}
