#pragma once
#include <mutex>
#include <array>
#include <vector>
#include <string>
#include <QTextCodec>

namespace PticaGovorun {

struct RecognizerSettings
{
	int FrameSize;
	int FrameShift;
	int SampleRate;
	
	// -wsp option
	bool UseWsp;

	// -logfile option
	std::string LogFile;

	// -d option
	std::string LanguageModelFilePath;

	// -v option
	std::string DictionaryFilePath;

	// -h option
	std::string AcousticModelFilePath;

	// -hlist option
	std::string TiedListFilePath;

	// -filelist option
	std::string FileListFileName;

	// -C option
	std::string CfgFileName;

	std::string CfgHeaderFileName;

	// temporary files
	std::string TempSoundFile;
};

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

struct JuiliusRecognitionResult
{
	std::vector<std::wstring> TextPass1;
	std::vector<std::wstring> WordSeq; // word seq from Phase1 or Phase2
	std::vector<std::vector<std::string>> PhonemeSeqPass1;

	// phase 2
	int AlignmentStatus; // 0=success
	std::string AlignmentErrorMessage;
	std::vector<AlignedPhoneme> AlignedPhonemeSeq;
};

class PG_EXPORTS JuliusToolWrapper
{
	friend PG_EXPORTS auto createJuliusRecognizer(const RecognizerSettings& recognizerSettings)->std::tuple < bool, std::string, std::unique_ptr<JuliusToolWrapper> >;

	std::mutex pInitializingMutex_;
	QTextCodec* pTextCodec = nullptr;
private:
	RecognizerSettings recognizerSettings_;
public:
	explicit JuliusToolWrapper(const RecognizerSettings& recognizerSettings);
	~JuliusToolWrapper();

	std::tuple<bool, std::string> recognize(LastFrameSample takeSample, const short* audioSamples, int audioSamplesCount, JuiliusRecognitionResult& result);
};

PG_EXPORTS auto createJuliusRecognizer(const RecognizerSettings& recognizerSettings)->std::tuple < bool, std::string, std::unique_ptr<JuliusToolWrapper> > ;

void JuliusClearCache();

}