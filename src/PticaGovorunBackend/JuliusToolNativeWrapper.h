#pragma once
#include <mutex>
#include <array>
#include <vector>
#include <string>
#include <QTextCodec>
#include "SpeechProcessing.h"
#include "ComponentsInfrastructure.h"

namespace PticaGovorun {

struct RecognizerSettings
{
	int FrameSize;
	int FrameShift;
	int SampleRate;
	
	// enable inter-word short pause handing (for multipath)
	// Julius -iwsp option
	bool UseWsp;

	// Julius -logfile option
	std::string LogFile;

	// Julius -d option
	std::string LanguageModelFilePath;

	// Julius -v option
	std::string DictionaryFilePath;

	// Julius -h option
	std::string AcousticModelFilePath;

	// Julius -hlist option
	std::string TiedListFilePath;

	// Julius -filelist option
	std::string FileListFileName;

	// Julius  -C option
	std::string CfgFileName;

	std::string CfgHeaderFileName;

	// temporary files
	std::string TempSoundFile;
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
	friend PG_EXPORTS auto createJuliusRecognizer(const RecognizerSettings& recognizerSettings, std::unique_ptr<QTextCodec, NoDeleteFunctor<QTextCodec>> textCodec)->std::tuple < bool, std::string, std::unique_ptr<JuliusToolWrapper> >;

	std::mutex pInitializingMutex_;
	std::unique_ptr<QTextCodec, NoDeleteFunctor<QTextCodec>> textCodec_ = nullptr;
	RecognizerSettings recognizerSettings_;
public:
	explicit JuliusToolWrapper(const RecognizerSettings& recognizerSettings);
	~JuliusToolWrapper();

	// Translates the audio into ukrainian text.
	std::tuple<bool, std::string> recognize(LastFrameSample takeSample, const short* audioSamples, int audioSamplesCount, JuiliusRecognitionResult& result);

	// Aligns ground truth phones according to given audio.
	void alignPhones(const short* audioSamples, int audioSamplesCount, const std::vector<std::string>& speechPhones, const AlignmentParams& paramsInfo, int tailSize, PhoneAlignmentInfo& alignmentResult);

	const RecognizerSettings& settings() const;
};

PG_EXPORTS bool initRecognizerConfiguration(const std::string& recogName, RecognizerSettings& rs);

PG_EXPORTS auto createJuliusRecognizer(const RecognizerSettings& recognizerSettings, std::unique_ptr<QTextCodec, NoDeleteFunctor<QTextCodec>> textCodec)->std::tuple < bool, std::string, std::unique_ptr<JuliusToolWrapper> >;

// Computes MFCC features using global initialized 'Recog' object.
// mfccFeatures contains MFCC features for the first frame, then for the second etc.
PG_EXPORTS std::tuple<bool,const char*> computeMfccFeaturesPub(const short* sampleData, int sampleDataSize, size_t frameSize, size_t frameShift, size_t mfccVecLen, std::vector<float>& mfccFeatures, int& framesCount);

void JuliusClearCache();

}