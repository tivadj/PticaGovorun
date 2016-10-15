#pragma once
#include <gsl/span>
#include <vector>
#include "PticaGovorunCore.h"
#include "VoiceActivity.h"

namespace PticaGovorun
{
	struct ErrMsgList;

	/// Gets speech/silence regions for audio using Sphinx implementation.
	PG_EXPORTS bool detectVoiceActivityG729(gsl::span<const short> samples, float sampRate, std::vector<SegmentSpeechActivity>& activity, ErrMsgList* errMsg);
}
