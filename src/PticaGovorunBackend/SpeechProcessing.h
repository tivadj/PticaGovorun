#pragma once

#include <vector>
#include <string>
#include <functional>
#include <tuple>
#include <QObject>
#include "PticaGovorunCore.h"

namespace PticaGovorun {

static const char* PGPhoneSilence = "sil";
static const char* PGShortPause = "sp";

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

struct PhoneDistributionPart
{
	int OffsetFrameIndex;

	std::vector<float> LogProbs;
};

struct PhoneAlignmentInfo
{
	std::vector<AlignedPhoneme> AlignInfo;

	std::vector<PhoneDistributionPart> PhoneDistributions;

	int UsedFrameSize;

	int UsedFrameShift;

	float AlignmentScore;
};

enum class MarkerLevelOfDetail
{
	// Seaparates words or sentences in audio. Audio player stops on a marker with such level of detail.
	Word,

	// Separates phones. The number of samples per phone is so small, that there is no sense to play audio of the single phone.
	// Audio player do not stop if hits such markers.
	Phone
};

struct TimePointMarker
{
	// Uniquely identifies the marker. For example, when two markers have the same sample index.
	int Id = -1;

	// The index of the frame (sample) this marker points at.
	long SampleInd;

	// Whether marker was added by user (manual=true). Automatic markers (manual=false) can be freely
	// removed from model without user's concern. For example, when audio is reanalyzed, old automatic markers
	// may be replaced with new ones.
	bool IsManual;

	// Separates phones in audio.
	MarkerLevelOfDetail LevelOfDetail = MarkerLevelOfDetail::Word;

	// Determines if this marker stops the audio playback.
	// by default: true for word-level markers and false for phone-level markers
	// This property is not serializable.
	bool StopsPlayback;

	QString TranscripText;
	PhoneAlignmentInfo TranscripTextPhones; // splits transcripted text into phones and align them onto audio

	// recognition results
	QString RecogSegmentText;
	QString RecogSegmentWords;
	std::vector<AlignedPhoneme> RecogAlignedPhonemeSeq;
};

// Determines if a marker with given level of detail will stop the audio playback.
PG_EXPORTS bool getDefaultMarkerStopsPlayback(MarkerLevelOfDetail levelOfDetail);

// insertShortPause=true to insert sp phone between words.
PG_EXPORTS std::tuple<bool, std::wstring> convertTextToPhoneList(const std::wstring& text, std::function<auto (const std::wstring&, std::vector<std::string>&) -> void> wordToPhoneListFun, bool insertShortPause, std::vector<std::string>& speechPhones);

// Appends silience frames before and after the given audio.
// Result (padded audio) has a size=initial audio size + 2*silenceFramesCount.
PG_EXPORTS void padSilence(const short* audioFrames, int audioFramesCount, int silenceFramesCount, std::vector<short>& paddedAudio);

// Merges a sequence of phone-states (such as j2,j3,j4,o2,o3...) into monophone sequence (such as j,o...)
void mergeSamePhoneStates(const std::vector<AlignedPhoneme>& phoneStates, std::vector<AlignedPhoneme>& monoPhones);
}