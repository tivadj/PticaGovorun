#pragma once

#include <vector>
#include <string>
#include <functional>
#include <tuple>
#include <QObject>
#include "PticaGovorunCore.h"

namespace PticaGovorun {

static const char* PGPhoneSilence = "sil";

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

struct AlignmentParams
{
	int FrameSize;
	int FrameShift;
	LastFrameSample TakeSample;
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

PG_EXPORTS std::tuple<bool, std::wstring> convertTextToPhoneList(const std::wstring& text, std::function<auto (const std::wstring&, std::vector<std::string>&) -> void> wordToPhoneListFun, std::vector<std::string>& speechPhones);

}