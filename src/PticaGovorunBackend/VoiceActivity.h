#pragma once
#include <cstddef> // ptrdiff_t
#include <vector>
#include <gsl/span>

namespace PticaGovorun
{
	// VAD (Voice Activity Detection)

	/// Determines speech/silence activity for some range of audio.
	struct SegmentSpeechActivity
	{
		bool IsSpeech = false; // false=silence
		ptrdiff_t StartSampleInd = -1;
		ptrdiff_t EndSampleInd = -1;
	};

	void convertVadMaskToSegments(gsl::span<int> framesVadMask, int frameSize, int frameShift, std::vector<SegmentSpeechActivity>& activity);
}
