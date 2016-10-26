#pragma once
#include <gsl/span>
#include <vector>
#include "PticaGovorunCore.h"
#include "VoiceActivity.h"

namespace PticaGovorun
{
	/// Parameters to sphinx_fe utility.
	struct SphinxVadParams
	{
		float WindowLength = 0.025625; // ms, -wlen, for SampleRate=16000 corresponds to 16k*this=410 samples
		int FrameRate = 100; // frames per second, for SampleRate=16000 corresponds to 16k/this=160 samples
		int PreSpeech = 20; // -vad_prespeech
		int PostSpeech = 50; // -vad_postspeech
		int StartSpeech = 10; // -vad_startspeech
		float VadThreashold = 2; // -vad_threshold
	};

	struct ErrMsgList;

	/// Gets speech/silence regions for audio using Sphinx implementation.
	PG_EXPORTS bool detectVoiceActivitySphinx(gsl::span<const short> samples, float sampRate, const SphinxVadParams& vadArgs, std::vector<SegmentSpeechActivity>& activity, ErrMsgList* errMsg);
}
