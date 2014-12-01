#include "stdafx.h"
#include <sstream>
#include <memory> // std::unique_ptr
#include <fstream> // std::ofstream
#include "JuliusToolNativeWrapper.h"
#include "SoundUtils.h"

#include "../../ThirdParty/julius4/libjulius/include/julius/julius.h"
#include "../../ThirdParty/julius4/julius/app.h"

//struct Recog;
extern "C" int mainJuliusTool(int argc, char *argv[]);
extern "C" void mainJuliusToolDestructor();
extern "C" void main_recognition_stream_loop(Recog *recog);
extern "C" void myGlobalSetSoundFile(const char* soundFile);
extern "C" unsigned char adin_file_standby(int freq, void *arg);
extern "C" void update_gconst(HTK_HMM_Dens *d);
std::vector<std::string> recogOutputTextPass1_; // woutput
std::vector<std::string> recogOutputWordSeqPass1_; // wname
std::vector<std::vector<std::string>> recogOutputPhonemeSeqPass1_; // wseq
std::vector<std::string> wordAsPhonSeq_; // temporary buffer to construct phoneme sequence for word

int recogStatus_;
std::vector<PticaGovorun::AlignedPhoneme> recogOutputPhonemeSeq1_; // phseq1
std::vector<std::string> recogOutputWordSeqPass2_; // wname
extern "C" Recog *myGlobalRecog_;

extern "C" void myRecogOutputTextPass1(const char* woutput)
{
	recogOutputTextPass1_.push_back(woutput);
}
extern "C" void myRecogOutputWordSeqPass1(const char* wname)
{
	recogOutputWordSeqPass1_.push_back(wname);
}
extern "C" void myRecogOutputPhonemeSeqPass1_PushPhonemeSeq()
{
	if (wordAsPhonSeq_.size() > 0)
		recogOutputPhonemeSeqPass1_.push_back(wordAsPhonSeq_);

	wordAsPhonSeq_.clear();
}
extern "C" void myRecogOutputPhonemeSeqPass1_AddPhoneme(const char* wseq)
{
	wordAsPhonSeq_.push_back(wseq);
}

// phase 2
extern "C" void myRecogStatus(int recogStatus)
{
	// 0=success, errors are J_RESULT_STATUS_FAIL,...
	recogStatus_ = recogStatus;
}
extern "C" void myRecogOutputWordSeqPass2(const char* wname)
{
	recogOutputWordSeqPass2_.push_back(wname);
}


extern "C" void myRecogOutputPhonemeSeq1(const char* name, float avgScore, int begFrame, int endFrameIncl)
{
	PticaGovorun::AlignedPhoneme phoneme;
	phoneme.Name = name;
	phoneme.AvgScore = avgScore;
	phoneme.BegFrame = begFrame;
	phoneme.EndFrameIncl = endFrameIncl;
	recogOutputPhonemeSeq1_.push_back(phoneme);
}

namespace PticaGovorun {

// Encodes the vector of std::string as std::wstring.
void encodeVectorOfStrs(const std::vector<std::string>& wordList, const QTextCodec& textCodec, std::vector<std::wstring>& wideWordList)
{
	for (const std::string& word : wordList)
	{
		QString wordQStr = textCodec.toUnicode(word.c_str());
		std::wstring wordWide = wordQStr.toStdWString();
		wideWordList.push_back(wordWide);
	}
}


void JuliusClearCache()
{
	recogStatus_ = 999;
	recogOutputTextPass1_.clear();
	recogOutputWordSeqPass1_.clear();
	recogOutputPhonemeSeqPass1_.clear();
	wordAsPhonSeq_.clear();

	// phase 2
	recogOutputPhonemeSeq1_.clear();
	recogOutputWordSeqPass2_.clear();
}

size_t chooseFrameEndSample(LastFrameSample chooseTechnique, size_t endFramIndexIncl, size_t frameSize, size_t frameShift)
{
	size_t endSample;
	if (chooseTechnique == LastFrameSample::BeginOfTheNextFrame)
		endSample = (1 + endFramIndexIncl) * frameShift;
	else if (chooseTechnique == LastFrameSample::EndOfLastFrame)
		endSample = endFramIndexIncl * frameShift + frameSize;
	else if (chooseTechnique == LastFrameSample::MostLikely)
		endSample = endFramIndexIncl * frameShift + static_cast<size_t>(0.5 * (frameSize + frameShift));
	else
		throw std::exception("Unknown type of sample choosing technique");

	return endSample;
}

auto createJuliusRecognizer(const RecognizerSettings& recognizerSettings) 
-> std::tuple<bool, std::string, std::unique_ptr<JuliusToolWrapper>>
{
	// we work with Julius in 'Windows-1251' encoding
	auto textCodec = QTextCodec::codecForName("windows-1251");
	if (textCodec == nullptr)
		return std::make_tuple(false, "Can't find 'windows-1251' encoding", nullptr);

	auto fileListFileName = recognizerSettings.FileListFileName;
	{
		std::ofstream fileListFile;
		fileListFile.open(fileListFileName, std::ios::trunc);
		if (!fileListFile.is_open())
			return std::make_tuple(false, "Can't create Julius file list", nullptr);

		fileListFile << recognizerSettings.TempSoundFile << std::endl;
	}

	auto cfgFileName = recognizerSettings.CfgFileName;
	{
		std::ofstream cfgFile;
		cfgFile.open(cfgFileName, std::ios::trunc);
		if (!cfgFile.is_open())
			return std::make_tuple(false, "Can't create Julius configuration file", nullptr);

		// append header
		if (!recognizerSettings.CfgHeaderFileName.empty())
		{
			auto cfgHeaderFileName = recognizerSettings.CfgHeaderFileName;
			std::ifstream cfgHeaderFile;
			cfgHeaderFile.open(cfgHeaderFileName);
			if (cfgHeaderFile.is_open())
			{
				std::string line;

				while (!cfgHeaderFile.eof()) {
					getline(cfgHeaderFile, line);

					cfgFile << line << std::endl;
				}
			}
		}

		cfgFile << "-input rawfile" << std::endl;
		cfgFile << "-filelist " << fileListFileName << std::endl;

		if (recognizerSettings.SampleRate != 0)
			cfgFile << "-smpFreq " << recognizerSettings.SampleRate << std::endl;

		if (recognizerSettings.FrameSize != 0)
			cfgFile << "-fsize " << recognizerSettings.FrameSize << std::endl;

		if (recognizerSettings.FrameShift != 0)
			cfgFile << "-fshift " << recognizerSettings.FrameShift << std::endl;

		if (recognizerSettings.UseWsp)
			cfgFile << "-iwsp" << std::endl;

		cfgFile << "-logfile " << recognizerSettings.LogFile << std::endl;

		// B.6. Recognition process and search
		//cfgFile <<"-palign" <<std::endl; // align by phonemes

		cfgFile << "-v " << recognizerSettings.DictionaryFilePath << std::endl;
		cfgFile << "-d " << recognizerSettings.LanguageModelFilePath << std::endl;

		cfgFile << "-h " << recognizerSettings.AcousticModelFilePath << std::endl;
		if (!recognizerSettings.TiedListFilePath.empty())
			cfgFile << "-hlist " << recognizerSettings.TiedListFilePath << std::endl;
	}

	std::array<const char*, 3> inputParams;
	int i = 0;
	inputParams[i++] = "prog.exe";
	inputParams[i++] = "-C";
	inputParams[i++] = cfgFileName.c_str();

	int mainCode;
	try
	{
		mainCode = mainJuliusTool(inputParams.size(), const_cast<char**>(inputParams.data()));
	}
	catch (const std::exception& stdExc)
	{
		return std::make_tuple(false, stdExc.what(), nullptr);
	}
	catch (...)
	{
		return std::make_tuple(false, "Unrecognized native exception", nullptr);
	}

	if (mainCode != 555)
		return std::make_tuple(false, "Can't initialize julius tool", nullptr);

	auto recognizer = std::make_unique<JuliusToolWrapper>(recognizerSettings);
	recognizer->pTextCodec = textCodec;
	return std::make_tuple(true, "", std::move(recognizer));
}

JuliusToolWrapper::JuliusToolWrapper(const RecognizerSettings& recognizerSettings)
	:recognizerSettings_(recognizerSettings)
{
}

JuliusToolWrapper::~JuliusToolWrapper()
{
	mainJuliusToolDestructor();
}

std::tuple<bool, std::string> JuliusToolWrapper::recognize(LastFrameSample takeSample, const short* audioSamples, int audioSamplesCount, JuiliusRecognitionResult& result)
{
	JuliusClearCache();

	auto writeOp = PticaGovorun::writeAllSamplesWav(audioSamples, audioSamplesCount, recognizerSettings_.TempSoundFile, recognizerSettings_.SampleRate);
	if (!std::get<0>(writeOp))
		return std::make_tuple(false, std::string("Can't write segments file. ") + std::get<1>(writeOp));

	// run

	adin_file_standby(recognizerSettings_.SampleRate, (void*)recognizerSettings_.FileListFileName.c_str());
	main_recognition_stream_loop(myGlobalRecog_);

	//
	// prepare results

	result.AlignmentStatus = recogStatus_;
	if (recogStatus_ != 0) // error
	{
		std::stringstream err;
		err << "Error (" << recogStatus_ <<") ";
		if (recogStatus_ == -1)
			err << " J_RESULT_STATUS_FAIL";
		else if (recogStatus_ == -2)
			err << " J_RESULT_STATUS_REJECT_SHORT";
		else if (recogStatus_ == -4)
			err << " J_RESULT_STATUS_ONLY_SILENCE";
		else if (recogStatus_ == -6)
			err << " J_RESULT_STATUS_REJECT_POWER";
		else if (recogStatus_ == -7)
			err << " J_RESULT_STATUS_BUFFER_OVERFLOW";
		result.AlignmentErrorMessage = err.str();
	}

	PG_Assert(pTextCodec != nullptr && "Text codec must be initialized at construction time");

	encodeVectorOfStrs(recogOutputTextPass1_, *pTextCodec, result.TextPass1);

	if (recogStatus_ != 0)
	{
		encodeVectorOfStrs(recogOutputWordSeqPass1_, *pTextCodec, result.WordSeq);

		// only phase 1 is valid
		result.PhonemeSeqPass1 = recogOutputPhonemeSeqPass1_;
	}
	else
	{
		encodeVectorOfStrs(recogOutputWordSeqPass2_, *pTextCodec, result.WordSeq);

		// do checks
		for (size_t i = 0; i < recogOutputPhonemeSeq1_.size(); ++i)
		{
			if (i > 0 && recogOutputPhonemeSeq1_[i].BegFrame != recogOutputPhonemeSeq1_[i - 1].EndFrameIncl + 1)
			{
				std::stringstream err;
				err << "Frame are not desnsely aligned on phoneme index=" << i;
				return std::make_tuple(false, err.str());
			}
		}

		// update begin/end sample index of a moving window (frame in Julius terminology)
		for (size_t i = 0; i<recogOutputPhonemeSeq1_.size(); ++i)
		{
			AlignedPhoneme& nativePh = recogOutputPhonemeSeq1_[i];

			nativePh.BegSample = nativePh.BegFrame * recognizerSettings_.FrameShift;

			size_t endSample = chooseFrameEndSample(takeSample, nativePh.EndFrameIncl, recognizerSettings_.FrameSize, recognizerSettings_.FrameShift);
			nativePh.EndSample = (int)endSample;
		}
		result.AlignedPhonemeSeq = recogOutputPhonemeSeq1_;
	}

	//
	JuliusClearCache(); // optional tidy up

	int remOp = remove(recognizerSettings_.TempSoundFile.c_str());
	if (remOp != 0) std::make_tuple(false, "Can't remove temporary sound file");

	return std::make_tuple(true, std::string());
}

}