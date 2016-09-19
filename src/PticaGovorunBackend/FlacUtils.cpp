#include "FlacUtils.h"
#include <QString>

#ifdef PG_HAS_FLAC
#include "FLAC++/decoder.h"

namespace PticaGovorun
{
	namespace details
	{
		static bool write_little_endian_uint16(FILE *f, FLAC__uint16 x)
		{
			return fputc(x, f) != EOF && fputc(x >> 8, f) != EOF;
		}

		class OurDecoder : public FLAC::Decoder::File {
			FLAC__uint64 total_samples = 0;
			unsigned sample_rate = 0;
			unsigned channels = 0;
			unsigned bps = 0;
			const char* statusMsg_ = nullptr;
			std::vector<short>& resultFrames_;
		public:
			OurDecoder(std::vector<short>& resultFrames) : resultFrames_(resultFrames) { }
			unsigned sampleRate() const { return sample_rate; }
		protected:
			virtual ::FLAC__StreamDecoderWriteStatus write_callback(const ::FLAC__Frame *frame, const FLAC__int32 * const buffer[]) override;
			virtual void metadata_callback(const ::FLAC__StreamMetadata *metadata) override;
			virtual void error_callback(::FLAC__StreamDecoderErrorStatus status) override;
		private:
			OurDecoder(const OurDecoder&);
			OurDecoder&operator=(const OurDecoder&);
		};

		void OurDecoder::error_callback(::FLAC__StreamDecoderErrorStatus status)
		{
			fprintf(stderr, "Got error callback: %s\n", FLAC__StreamDecoderErrorStatusString[status]);
		}

		void OurDecoder::metadata_callback(const ::FLAC__StreamMetadata *metadata)
		{
			/* print some stats */
			if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
				/* save for later */
				total_samples = metadata->data.stream_info.total_samples;
				sample_rate = metadata->data.stream_info.sample_rate;
				channels = metadata->data.stream_info.channels;
				bps = metadata->data.stream_info.bits_per_sample;

				// NOTE: printing to std::cout when Unicode is set up is an ERROR (see _setmode call in main)
				//fprintf(stderr, "sample rate    : %u Hz\n", sample_rate);
				//fprintf(stderr, "channels       : %u\n", channels);
				//fprintf(stderr, "bits per sample: %u\n", bps);
				//fprintf(stderr, "total samples  : %" PRIu64 "\n", total_samples);

				auto allSamples = total_samples * channels;
				resultFrames_.reserve(allSamples);
			}
		}

		::FLAC__StreamDecoderWriteStatus OurDecoder::write_callback(const ::FLAC__Frame *frame, const FLAC__int32 * const buffer[])
		{
			const FLAC__uint32 total_size = (FLAC__uint32)(total_samples * channels * (bps / 8));
			size_t i;

			if (total_samples == 0) {
				fprintf(stderr, "ERROR: this example only works for FLAC files that have a total_samples count in STREAMINFO\n");
				return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
			}
			if (channels != 1 || bps != 16) {
				statusMsg_ = "ERROR: this example only supports 16bit stereo streams";
				return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
			}

			/* write decoded PCM samples */
			for (i = 0; i < frame->header.blocksize; i++) {
				// left channel
				short value = static_cast<short>(buffer[0][i]);
				resultFrames_.push_back(value);
			}

			return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
		}
	}

	PG_EXPORTS bool readAllSamplesFlac(const boost::filesystem::path& filePath, std::vector<short>& result, float *frameRate, std::wstring* errMsg)
	{
		details::OurDecoder decoder(result);
		decoder.set_md5_checking(true);

		FLAC__StreamDecoderInitStatus init_status = decoder.init(filePath.string());
		if (init_status != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
			const char* msg = FLAC__StreamDecoderInitStatusString[init_status];
			*errMsg = QString("ERROR: initializing decoder. %1").arg(msg).toStdWString();
			return false;
		}

		bool ok = decoder.process_until_end_of_stream();
		if (!ok)
		{
			*errMsg = std::wstring(L"ERROR: decoder.process_until_end_of_stream. %1");
			return false;
		}

		if (frameRate != nullptr)
			*frameRate = decoder.sampleRate();

		return true;
	}
}
#endif