#include "VoiceActivity.h"

namespace PticaGovorun
{
	SegmentSpeechActivity::SegmentSpeechActivity(bool isSpeech, ptrdiff_t startSampleInd, ptrdiff_t endSampleInd)
		: IsSpeech(isSpeech),
		StartSampleInd(startSampleInd),
		EndSampleInd(endSampleInd)
	{
	}

	void convertVadMaskToSegments(gsl::span<int> framesVadMask, int frameSize, int frameShift, std::vector<SegmentSpeechActivity>& activity)
	{
		if (framesVadMask.empty())
			return;

		int frameMask = framesVadMask[0];
		int startFrameInd = 0;

		auto putSeg = [&, frameShift](int frameMaskValue, int endFrameInd)
		{
			SegmentSpeechActivity seg;
			// use frameRangeToSampleRange?
			// set sample to the beginning of frame
			seg.StartSampleInd = startFrameInd * frameShift;
			seg.EndSampleInd = endFrameInd * frameShift;
			seg.IsSpeech = frameMaskValue > 0;
			activity.push_back(seg);
		};
		for (size_t i = 1; i < framesVadMask.size(); ++i)
		{
			int curMaskValue = framesVadMask[i];
			if (curMaskValue == frameMask)
				continue;

			putSeg(frameMask, i);

			frameMask = curMaskValue;
			startFrameInd = i;
		}
		// last segment
		if (activity.empty() || // all frames have the same mask (speech or silence)
			activity.back().EndSampleInd != framesVadMask.size())
		{
			putSeg(frameMask, framesVadMask.size());
		}
	}
}