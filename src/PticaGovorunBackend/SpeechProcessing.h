#pragma once

#include <vector>
#include <string>
#include <functional>
#include <tuple>
#include <memory>

#include <QObject>
#include <QFileInfo>
#include <boost/utility/string_view.hpp>
#include <boost/optional/optional.hpp>

#if PG_HAS_OPENCV
#include <opencv2/ml.hpp>
#endif

#include "PticaGovorunCore.h"
#include "MLUtils.h"
#include "ClnUtils.h"
#include "ComponentsInfrastructure.h"

namespace PticaGovorun {

static const char* PGPhoneSilence = "sil";
static const char* PGShortPause = "sp";

// we work with Julius in 'Windows-1251' encoding
static const char* PGEncodingStr = "windows-1251";

// TODO: determine dependent code
static const int SampleRate = 22050;
static const int FrameSize = 400;
static const int FrameShift = 160;
//static const int FrameShift = 80;
static const size_t MfccVecLen = 39;
static const size_t NumClusters = 1;

enum class LastFrameSample
{
	BeginOfTheNextFrame,
	EndOfThisFrame,
	MostLikely // two consequent frames are separated on the median between begin of the left frame and end of the right frame
};

static const PticaGovorun::LastFrameSample FrameToSamplePicker = PticaGovorun::LastFrameSample::MostLikely;

// Represents indices of the frames (samples) range.
struct TwoFrameInds // FramesRangeRef
{
	// The index of the first frame, this range points to.
	long Start;
	// The number of frames in the range.
	long Count;
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

// Represents word which is anchored to the waveform.
struct AlignedWord
{
	QString Name;
	int BegSample;
	int EndSample;
	float Prob;
};


struct AlignmentParams
{
	int FrameSize;
	int FrameShift;
	LastFrameSample TakeSample;
};

// Stores probabilities of each phone for each window (frame).
struct ClassifiedSpeechSegment
{
	int BegFrame;
	int EndFrameIncl;

	// 
	int BegSample;
	int EndSample;

	std::vector<float> PhoneLogProbs;
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

// The language of spoken speech.
enum class SpeechLanguage
{
	NotSet,    // null, the language is not set or has empty xml string representation
	Ukrainian, // 'uk'
	Russian    // 'ru'
};

enum class ResourceUsagePhase
{
	Train,
	Test,
};

boost::string_view toString(ResourceUsagePhase phase);
boost::optional<ResourceUsagePhase> resourceUsagePhaseFromString(boost::string_view phase);

struct TimePointMarker
{
	// Uniquely identifies the marker. For example, when two markers have the same sample index.
	int Id = -1;

	// The index of the frame (sample) this marker points at.
	long SampleInd;

	// Separates phones in audio.
	MarkerLevelOfDetail LevelOfDetail = MarkerLevelOfDetail::Word;

	QString TranscripText;

	// The spoken language of corresponding segment of the speech.
	// The language is set only when transcript text has been set.
	SpeechLanguage Language = SpeechLanguage::NotSet;

	// The speaker id or speaker's name.
	std::wstring SpeakerBriefId;

	// Value=null means, allow usage without constraints.
	// Value=Train means, ignore this segment in training.
	boost::optional<ResourceUsagePhase> ExcludePhase;

	// Determines if this marker stops the audio playback.
	// by default: true for word-level markers and false for phone-level markers
	// This property is not serializable.
	bool StopsPlayback;

	// Whether marker was added by user (manual=true). Automatic markers (manual=false) can be freely
	// removed from model without user's concern. For example, when audio is reanalyzed, old automatic markers
	// may be replaced with new ones.
	bool IsManual;
};

/// The markers starting with '#' are excluded from building speech model,
/// but included in validation.
bool includeInTrainOrTest(const TimePointMarker& marker);

// Represents annotated part of a speech audio used for training the speech recognizer.
struct AnnotatedSpeechSegment
{
	int SegmentId;

	// Text associated with this speech segment.
	std::wstring TranscriptText;

	SpeechLanguage Language = SpeechLanguage::NotSet;

	std::wstring FilePath;

	// Frames' frame rate.
	float FrameRate = -1;

	// Actual samples in [FrameStart; FrameEnd) range.
	std::vector<short> Frames;
	
	int StartMarkerId = -1;
	int EndMarkerId = -1;
	
	TimePointMarker StartMarker;
	TimePointMarker EndMarker;
	TimePointMarker ContentMarker;

	bool AudioStartsWithSilence = false; // true if there are silence samples in the start of audio
	bool AudioEndsWithSilence = false;
	long StartSilenceFramesCount = 0;
	long EndSilenceFramesCount = 0;
	bool TranscriptionStartsWithSilence = false; // true if transcription starts with <s>
	bool TranscriptionEndsWithSilence = false;
};

// Determines if a marker with given level of detail will stop the audio playback.
PG_EXPORTS bool getDefaultMarkerStopsPlayback(MarkerLevelOfDetail levelOfDetail);

// Converts language to string.
PG_EXPORTS std::string speechLanguageToStr(SpeechLanguage lang);

/// Separates different pronunciations (pronCode or pronunciation's descriptor) from given string.
PG_EXPORTS void splitUtteranceIntoPronuncList(boost::wstring_view text, GrowOnlyPinArena<wchar_t>& arena, std::vector<boost::wstring_view>& words);

// insertShortPause=true to insert sp phone between words.
PG_EXPORTS std::tuple<bool, std::wstring> convertTextToPhoneList(const std::wstring& text, std::function<auto (const std::wstring&, std::vector<std::string>&) -> bool> wordToPhoneListFun, bool insertShortPause, std::vector<std::string>& speechPhones);

// Appends silience frames before and after the given audio.
// Result (padded audio) has a size=initial audio size + 2*silenceFramesCount.
PG_EXPORTS void padSilence(const short* audioFrames, int audioFramesCount, int silenceFramesCount, std::vector<short>& paddedAudio);

// Merges a sequence of phone-states (such as j2,j3,j4,o2,o3...) into monophone sequence (such as j,o...)
PG_EXPORTS void mergeSamePhoneStates(const std::vector<AlignedPhoneme>& phoneStates, std::vector<AlignedPhoneme>& monoPhones);

// framEndIndex=inclusive index
PG_EXPORTS void frameRangeToSampleRange(size_t framBegIndex, size_t framEndIndex, LastFrameSample endSampleChoice, size_t frameSize, size_t frameShift, long& sampleBeg, long& sampleEnd);

// For given audio (wav) file from audio repository gives absolute path of corresponding annotation (xml) file path.
PG_EXPORTS std::wstring speechAnnotationFilePathAbs(const std::wstring& wavFileAbs, const std::wstring& wavRootDir, const std::wstring& annotRootDir);

// Loads annotated speech for training.
// targetLevelOfDetail=type of marker (segment) to query annotation.
// segPredBefore=predicate to determine whether to include segment into the result set; called before actual samples are loaded
PG_EXPORTS bool loadSpeechAndAnnotation(const QFileInfo& folderOrWavFilePath, const std::wstring& wavRootDir, const std::wstring& annotRootDir,
	MarkerLevelOfDetail targetLevelOfDetail, bool loadAudio, std::function<auto(const AnnotatedSpeechSegment& seg)->bool> segPredBefore, std::vector<AnnotatedSpeechSegment>& segments, std::wstring* errMsg);

// Collects the segments associated with the set of markers.
PG_EXPORTS void collectAnnotatedSegments(const std::vector<TimePointMarker>& markers, std::vector<std::pair<const TimePointMarker*, const TimePointMarker*>>& segments);

PG_EXPORTS bool collectMfccFeatures(const QFileInfo& folderOrWavFilePath, int frameSize, int frameShift, int mfccVecLen, std::map<std::string, std::vector<float>>& phoneNameToFeaturesVector, std::wstring* errMsg);

//PG_EXPORTS void makeFlatData(const std::map<std::string, std::vector<float>>& phoneNameToFeaturesVector)

// Gets the number of frames. Eatch frame contains 'mfccVecLen' features. Total number of features is 'featuresTotalCount'.
PG_EXPORTS int featuresFramesCount(int featuresTotalCount, int mfccVecLen);

#if PG_HAS_OPENCV
// IMPL: uses cv::ml::EM to train the classifiers.
PG_EXPORTS std::tuple<bool, const char*> trainMonophoneClassifier(const std::map<std::string, std::vector<float>>& phoneNameToFeaturesVector, int mfccVecLen, int numClusters,
	std::map<std::string, cv::Ptr<cv::ml::EM>>& phoneNameToEMObj);
#endif

PG_EXPORTS void preEmphasisInplace(wv::slice<float> xs, float preEmph);

// Calculates average of absolute values of amplitudes.
PG_EXPORTS float signalMagnitude(wv::slice<short> xs);

PG_EXPORTS void hammingInplace(wv::slice<float> frame);

PG_EXPORTS int getMinDftPointsCount(int frameSize);

// The segment [freqLowHz; freqHighHz] is covered with overlapped triangles.
// Each triangle has unit height (unit response) at the 'center' (the left shoulder is less then the right, 
// hence it is not exactly the center).
// Each triangle has zero height (zero response) in the centers of two neighbour trangles.
// Majority of frequencies hit two neighbour bins.
// Freqencies in [freqLowHz; bin[0].center] have response only in the right bin.
// Freqencies in [lastBin.center; freqHighHz] have response only in the left bin.
struct FilterHitInfo
{
	const static int NoBin = -1;

	int LeftBinInd = NoBin;
	int RightBinInd = NoBin;

	float LeftFilterResponse = 0.0f;
	float RightFilterResponse = 0.0f;
};

struct TriangularFilterBank
{
	int BinCount; // number of triangles (bins) in the filter
	int FftNum; // number of DFT points
	std::vector<FilterHitInfo> fftFreqIndToHitInfo;
};

PG_EXPORTS void buildTriangularFilterBank(float sampleRate, int binsCount, int fftNum, TriangularFilterBank& filterBank);

// Returns number of frames when samples are split into sliding frames.
PG_EXPORTS int slidingWindowsCount(long framesCount, int windowSize, int windowShift);

// Fills the buffer with coordinates of sliding windows.
// If frames range is [0 15], windowSize=10, windowShift=5 then windows bounaries are [[0 10], [5 15]]
PG_EXPORTS void slidingWindows(long startFrameInd, long framesCount, int windowSize, int windowShift, wv::slice<TwoFrameInds> windowBounds);

PG_EXPORTS void computeMfccVelocityAccel(const wv::slice<short> samples, int frameSize, int frameShift, int framesCount, int mfcc_dim, int mfccVecLen, const TriangularFilterBank& filterBank, wv::slice<float> mfccFeatures);
}
