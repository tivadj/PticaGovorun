#pragma once
#include <gsl/span>
#include <vector>
#include "PticaGovorunCore.h"
#include "VoiceActivity.h"

namespace PticaGovorun
{
	struct ErrMsgList;

	/// Detects silence/speech regions in given audio.
	/// The implementation of G729 codec with Annex B is extended to expose VAD (Voice Activity Detection) functionality.
	/// The original code is here:
	/// https://github.com/opentelecoms-org/codecs/blob/master/g729/siphon-g729/Tests/CODER.C
	PG_EXPORTS bool detectVoiceActivityG729(gsl::span<const short> samples, float sampRate, std::vector<SegmentSpeechActivity>& activity, ErrMsgList* errMsg);
}
