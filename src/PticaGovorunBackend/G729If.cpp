#include <vector>
#include "ComponentsInfrastructure.h" // ErrMsgList
#include "G729If.h"

#define PG_G729ANNEXB_VAD_HACKING

#ifdef PG_G729ANNEXB_VAD_HACKING
extern "C" {
#include <typedef.h>
#include <ld8k.h> // Init_Pre_Process
#include <dtx.h> // Init_Cod_cng
}
#endif

namespace PticaGovorun
{
	// The implementation of G729 codec with Annex B is extended to expose VAD (Voice Activity Detection) functionality.
	// The original code is here:
	// https://github.com/opentelecoms-org/codecs/blob/master/g729/siphon-g729/Tests/CODER.C
	bool detectVoiceActivityG729(gsl::span<const short> samples, float sampRate,  std::vector<SegmentSpeechActivity>& activity, ErrMsgList* errMsg)
	{
#ifndef PG_G729ANNEXB_VAD_HACKING
		if (errMsg != nullptr) errMsg->utf8Msg = "G279 VAD is not compiled in, use PG_G729ANNEXB_VAD_HACKING";
		return false;
#else
		if (sampRate != 8000)
		{
			if (errMsg != nullptr) errMsg->utf8Msg = "G279 codec requires sample rate of 8000";
			return false;
		}

		Word16 prm[PRM_SIZE + 1];        /* Analysis parameters.                  */
		Word16 syn[L_FRAME];           /* Buffer for synthesis speech           */

		// Initialization of the coder

		Init_Pre_Process();
		Init_Coder_ld8k(); // also initializes 'new_speech'
		for (int i = 0; i<PRM_SIZE; i++) prm[i] = (Word16)0;

		/* for G.729B */
		Init_Cod_cng();

		//
		const size_t frameSize = L_FRAME;
		auto frameShift = frameSize; // frames are not overlapped

		size_t framesCount = samples.size() / frameShift;
		std::vector<int> framesVadMask;
		framesVadMask.reserve(framesCount);

		//
		Word16 frame = 0; // used by G.729
		
		for (ptrdiff_t frameStartSampleInd = 0; ; frameStartSampleInd += frameShift)
		{
			if (frameStartSampleInd + frameSize > samples.size())
				break;

			gsl::span<const short> frameSamples = samples.subspan(frameStartSampleInd, frameSize);

			// new_speech is a global variable from where G.729 gets audio samples
			Word16 *new_speech = get_new_speech_buffer();
			std::copy(frameSamples.begin(), frameSamples.end(), new_speech);

			if (frame == 32767) frame = 256;
			else frame++;

			Pre_Process(new_speech, frameSize);

			const Word16 vad_enable = 1;
			Word16 isSpeech = 0;
			Coder_ld8k(prm, syn, frame, vad_enable, &isSpeech);
			framesVadMask.push_back(isSpeech);
		}

		convertVadMaskToSegments(framesVadMask, frameSize, frameShift, activity);
		return true;
#endif
	}
}