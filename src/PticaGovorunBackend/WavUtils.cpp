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
bool readAllSamplesWav(const boost::filesystem::path& filePath, std::vector<short>& result, float *sampleRate, ErrMsgList* errMsg)
{
	SF_INFO sfInfo;
	memset(&sfInfo, 0, sizeof(sfInfo));

	SNDFILE* sf = sf_open(filePath.string().c_str(), SFM_READ, &sfInfo);
	if (sf == nullptr)
	{
		if (errMsg != nullptr)
		{
			const char* err = sf_strerror(nullptr);
			auto sub = std::make_unique<ErrMsgList>();
			sub->utf8Msg = err;
			errMsg->utf8Msg = "libsndfile.sf_open() failed";
			errMsg->next = std::move(sub);
		}
		return false;
	}

	result.clear();
	result.resize(sfInfo.frames);
	if (sampleRate != nullptr)
		*sampleRate = sfInfo.samplerate;

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
		if (errMsg != nullptr)
		{
			const char* err = sf_strerror(nullptr);
			auto sub = std::make_unique<ErrMsgList>();
			sub->utf8Msg = err;
			errMsg->utf8Msg = "libsndfile.sf_close() failed";
			errMsg->next = std::move(sub);
		}
		return false;
	}

	return true;
}

std::tuple<bool, std::string> writeAllSamplesWav(const short* sampleData, int sampleCount, const std::string& fileName, int sampleRate)
{
	ErrMsgList errMsg;
	bool suc = writeAllSamplesWav(gsl::span<const short>(sampleData, sampleCount), sampleRate, fileName, &errMsg);
	return std::make_tuple(suc, errMsg.utf8Msg);
}

bool writeAllSamplesWav(gsl::span<const short> samples, int sampleRate, const boost::filesystem::path& filePath, ErrMsgList* errMsg)
{
	SF_INFO sfInfo;
	memset(&sfInfo, 0, sizeof(sfInfo));
	sfInfo.samplerate = sampleRate;
	sfInfo.channels = 1;
	sfInfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;

	SNDFILE* sf = sf_open(filePath.string().c_str(), SFM_WRITE, &sfInfo);
	if (sf == nullptr)
	{
		auto err = sf_strerror(nullptr);
		if (errMsg != nullptr)
			errMsg->utf8Msg = std::string(err);
		return false;
	}

	auto wcount = sf_write_short(sf, samples.data(), samples.size());
	if (wcount != samples.size())
	{
		if (errMsg != nullptr)
			errMsg->utf8Msg = std::string("Can't write all samples to the file");
		return false;
	}

	int code = sf_close(sf);
	if (code != 0)
	{
		auto err = sf_strerror(nullptr);
		if (errMsg != nullptr)
			errMsg->utf8Msg = std::string(err);
		return false;
	}

	return true;
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

bool resampleFrames(gsl::span<const short> audioSamples, float inputSampleRate, float outSampleRate, std::vector<short>& outSamples, ErrMsgList* errMsg)
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

	float srcSampleRate = inputSampleRate;
	float targetSampleRate = outSampleRate;

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
		if (errMsg != nullptr)
		{
			const char* msg = src_strerror(error);
			auto sub = std::make_unique<ErrMsgList>();
			sub->utf8Msg = msg;
			errMsg->utf8Msg = "samplerate.src_simple() failed";
			errMsg->next = std::move(sub);
		}
		return false;
	}

	targetSamplesFloat.resize(convertData.output_frames_gen);
	outSamples.assign(std::begin(targetSamplesFloat), std::end(targetSamplesFloat));
	//src_float_to_short_array(targetSamplesFloat.data(), targetSamples.data(), targetSamplesFloat.size());

	return true;
#endif // PG_HAS_SAMPLERATE
}

bool readAllSamplesFormatAware(const boost::filesystem::path& filePath, std::vector<short>& result, float *sampleRate, ErrMsgList* errMsg)
{
	QString fileNameQ = toQString(filePath.wstring());
#ifdef PG_HAS_FLAC
	if (fileNameQ.endsWith(".flac"))
		return readAllSamplesFlac(filePath, result, sampleRate, errMsg);
#endif
#ifdef PG_HAS_LIBSNDFILE
	if (fileNameQ.endsWith(".wav"))
		return readAllSamplesWav(filePath, result, sampleRate, errMsg);
#endif
	if (errMsg != nullptr)
		errMsg->utf8Msg = std::string("Unknown audio file extension: ") + filePath.extension().string();
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
