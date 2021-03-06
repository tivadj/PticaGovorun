#include <gsl/span>
#if PG_HAS_SPHINX
#include <pocketsphinx.h>
#include "ClnUtils.h"
#include "PticaGovorunCore.h"

namespace PticaGovorun
{
	struct ParsedSpeechSegment
	{
		std::wstring Word;
		int StartSampleInd;
		int EndSampleInd;
	};

	// Converts speech into text in most generic fashion.
	// Algorithm tries to decode short utterance (eg five seconds). It assumes that the beginning of utterence is 
	// recognized better and decoding in the end of the recognized utterance may accumulate error. So it drops couple of the last words.
	class PG_EXPORTS AudioSpeechDecoder
	{
		int frameSize_;
		int frameShift_;
		ps_decoder_t *ps_ = nullptr;
		bool hasError_ = false;
		const char* errorMsg_ = nullptr;
	public:
		void init(const char* hmmPath, const char* langModelPath, const char* dictPath);
		AudioSpeechDecoder();
		AudioSpeechDecoder(const AudioSpeechDecoder&) = delete;
		~AudioSpeechDecoder();
		
		void decode(gsl::span<const short> samples, float sampleRate, std::vector<std::wstring>& words, int& samplesProcessed);
		
		// Words that placed after the result index are truncated.
		int determineWordIndToExclude(const std::vector<ParsedSpeechSegment>& wordsMerged) const;

		bool hasError() const;
		const char* getErrorText() const;
	private:
		void setError(const char* errorText);
	};
}
#endif
