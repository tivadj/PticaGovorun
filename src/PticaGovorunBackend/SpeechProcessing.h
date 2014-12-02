#pragma once

#include <vector>
#include <QObject>

namespace PticaGovorun {

enum class LastFrameSample
{
	BeginOfTheNextFrame,
	EndOfLastFrame,
	MostLikely
};

struct AlignedPhoneme
{
	std::string Name;
	float AvgScore;
	int BegFrame;
	int EndFrameIncl;

	// 
	int BegSample;
	int EndSample;
};

struct TimePointMarker
{
	long SampleInd;

	// Whether marker was added by user (manual=true). Automatic markers (manual=false) can be freely
	// removed from model without user's concern. For example, when audio is reanalyzed, old automatic markers
	// may be replaced with new ones.
	bool IsManual;

	QString TranscripText;

	// recognition results
	QString RecogSegmentText;
	QString RecogSegmentWords;
	std::vector<AlignedPhoneme> AlignedPhonemeSeq;
};

}