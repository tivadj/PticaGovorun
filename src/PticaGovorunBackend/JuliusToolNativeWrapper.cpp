#include "stdafx.h"
#include <sstream>
#include <memory> // std::unique_ptr
#include <fstream> // std::ofstream
#include <hash_map>
#include "JuliusToolNativeWrapper.h"
#include "WavUtils.h"
#include "PhoneAlignment.h"

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

	auto createJuliusRecognizer(const RecognizerSettings& recognizerSettings, std::unique_ptr<QTextCodec, NoDeleteFunctor<QTextCodec>> textCodec)
		-> std::tuple < bool, std::string, std::unique_ptr<JuliusToolWrapper> >
	{
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

			cfgFile << "-v " << '\"' << recognizerSettings.DictionaryFilePath << '\"' << std::endl;
			cfgFile << "-d " << '\"' << recognizerSettings.LanguageModelFilePath << '\"' << std::endl;

			cfgFile << "-h " << '\"' << recognizerSettings.AcousticModelFilePath << '\"' << std::endl;
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
		recognizer->textCodec_ = std::move(textCodec);
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
			err << "Error (" << recogStatus_ << ") ";
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

		PG_Assert(textCodec_ != nullptr && "Text codec must be initialized at construction time");

		encodeVectorOfStrs(recogOutputTextPass1_, *textCodec_, result.TextPass1);

		if (recogStatus_ != 0)
		{
			encodeVectorOfStrs(recogOutputWordSeqPass1_, *textCodec_, result.WordSeq);

			// only phase 1 is valid
			result.PhonemeSeqPass1 = recogOutputPhonemeSeqPass1_;
		}
		else
		{
			encodeVectorOfStrs(recogOutputWordSeqPass2_, *textCodec_, result.WordSeq);

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
			for (size_t i = 0; i < recogOutputPhonemeSeq1_.size(); ++i)
			{
				AlignedPhoneme& nativePh = recogOutputPhonemeSeq1_[i];

				long begSample = -1;
				long endSample = -1;
				frameRangeToSampleRange(nativePh.BegFrame, nativePh.EndFrameIncl, takeSample, recognizerSettings_.FrameSize, recognizerSettings_.FrameShift, begSample, endSample);

				nativePh.BegSample = begSample;
				nativePh.EndSample = endSample;
			}
			result.AlignedPhonemeSeq = recogOutputPhonemeSeq1_;
		}

		//
		JuliusClearCache(); // optional tidy up

		int remOp = remove(recognizerSettings_.TempSoundFile.c_str());
		if (remOp != 0) std::make_tuple(false, "Can't remove temporary sound file");

		return std::make_tuple(true, std::string());
	}

	extern "C" VECT logNormalProb(HTK_HMM_Dens* pDens, VECT* x, size_t xsize)
	{
		HTK_HMM_Dens& dens = *pDens;
		assert(dens.meanlen == xsize);
		assert(dens.var->len == xsize);

		// assumes variances are inversed
		float* invVar = dens.var->vec;

		VECT power = 0;
		for (int i = 0; i < dens.meanlen; ++i)
		{
			auto xm = x[i] - dens.mean[i];

			auto co3 = xm * xm * invVar[i];
			power += co3;
		}

		VECT gconst = pDens->gconst;
		if (gconst == 0)
		{
			gconst = xsize * static_cast<float>(LOGTPI);
			VECT s1 = 0;
			for (int i = 0; i < dens.meanlen; ++i)
			{
				s1 += log(invVar[i]);
			}

			gconst = gconst - s1;
		}

		auto res = -0.5f * (gconst + power);

		assert(res <= 0 && "Logarithm of probability must be <= 0");
		assert(res != std::numeric_limits<decltype(res)>::infinity() && "LogProbability can't be infinity");

		return res;
	}

	double calculateDensityLogProb(HTK_HMM_State* state, VECT* x, size_t xsize)
	{
		double probSum = 0;

		for (int streamInd = 0; streamInd < state->nstream; ++streamInd)
		{
			HTK_HMM_PDF* pdfInfo = state->pdf[streamInd];
			if (pdfInfo == nullptr)
				continue;

			for (int densInd = 0; densInd < pdfInfo->mix_num; ++densInd)
			{
				HTK_HMM_Dens* dens = pdfInfo->b[densInd];
				if (dens == nullptr)
					continue;

				double logProb = logNormalProb(dens, x, xsize);

				auto densWeight = pdfInfo->bweight[densInd];
				logProb += densWeight;

				auto prob = exp(logProb);
				probSum += prob;
			}
		}

		auto logProbTotal = log(probSum);

		if (logProbTotal < LogProbTraits<double>::zeroValue())
			logProbTotal = LogProbTraits<double>::zeroValue();

		return logProbTotal;
	}

	std::vector<std::vector<float>> computeMfccFeatures(const std::vector<short>& sampleData, Recog* recog, size_t frameSize, size_t frameShift, size_t mfccVecLen)
	{
		using namespace std;

		size_t mfccCount = mfccVecLen;
		if (mfccCount == -1)
			mfccCount = recog->mfcclist->para->veclen;
		if (frameSize == -1)
			frameSize = recog->mfcclist->para->framesize;
		if (frameShift == -1)
			frameShift = recog->mfcclist->para->frameshift;

		auto framesCount = 1 + (sampleData.size() - frameSize) / frameShift;

		auto mfccData = std::vector<std::vector<float>>(framesCount, std::vector<float>(mfccCount, 0));
		std::vector<float*> mfccPtrs(framesCount);
		for (size_t i = 0; i < framesCount; ++i)
			mfccPtrs[i] = &mfccData[i][0];

		Value* para1 = recog->mfcclist->para;
		MFCCWork* work = recog->mfcclist->frontend.mfccwrk_ss;

		//int framesProcessed = Wav2MFCC(&sampleData[0], &mfccPtrs[0], para1, (int)sampleData.size(), work);
		//wav2mfcc(SP16 speech[], int speechlen, Recog *recog)
		VECT** mfccPtrs2tmp = recog->mfcclist->param->parvec;
		auto wavRes = wav2mfcc(const_cast<SP16*>(&sampleData[0]), (int)sampleData.size(), recog);

		VECT** mfccPtrs2 = recog->mfcclist->param->parvec;

		jlog("MY result features matrix:\n");
		for (size_t t = 0; t < framesCount; ++t)
		{
			float* feats = mfccPtrs2[t];
			for (size_t i = 0; i < mfccCount; ++i)
			{
				mfccData[t][i] = feats[i];
				//cout <<feats[i] <<" ";
				jlog("%f ", feats[i]);
			}
			//cout <<endl;
			jlog("\n");
		}

		return mfccData;
	}

	std::tuple<bool, const char*> computeMfccFeaturesPub(const short* sampleData, int sampleDataSize, size_t frameSize, size_t frameShift, size_t mfccVecLen, std::vector<float>& mfccFeatures, int& framesCount)
	{
		Recog* recog = myGlobalRecog_;
		if (recog == nullptr)
			return std::make_tuple(false, "computeMfccFeaturesPub: Global recognizer is not initialized");

		size_t mfccCount = mfccVecLen;
		if (mfccCount == -1)
			mfccCount = recog->mfcclist->para->veclen;
		if (frameSize == -1)
			frameSize = recog->mfcclist->para->framesize;
		if (frameShift == -1)
			frameShift = recog->mfcclist->para->frameshift;

		framesCount = 1 + (sampleDataSize - frameSize) / frameShift;

		static_assert(sizeof(sampleData[0]) == sizeof(SP16), "Sample has a SP16 type");

		auto wavRes = wav2mfcc(const_cast<SP16*>(sampleData), sampleDataSize, recog);
		if (!wavRes)
			return std::make_tuple(false, "Error calculating MFCC features");

		// populate results
		mfccFeatures.reserve(framesCount * mfccCount);

		VECT** mfccPtrs2 = recog->mfcclist->param->parvec;

		for (size_t t = 0; t < framesCount; ++t)
		{
			float* feats = mfccPtrs2[t];
			for (size_t i = 0; i < mfccCount; ++i)
			{
				mfccFeatures.push_back(feats[i]);
			}
		}
		return std::make_tuple(true, "");
	}

	struct PhoneStateModel
	{
		HTK_HMM_Data *phone;
		int stateInd;
		HTK_HMM_State *state;
	};

	// Enumerates all phone's states for given state.
	int FindPhoneStates(HTK_HMM_INFO *hmmInfo, const std::string& searchPhone, std::vector<PhoneStateModel>& resultStates)
	{
		using namespace std;

		int statesAdded = 0;

		for (HTK_HMM_Data* phone = hmmInfo->start; phone != nullptr; phone = phone->next)
		{
			if (phone == nullptr)
				continue;

			auto phoneName = string(phone->name);
			if (phone->name == nullptr || phoneName != searchPhone)
				continue;

			for (int stateInd = 0; stateInd < phone->state_num; ++stateInd)
			{
				HTK_HMM_State* state = phone->s[stateInd];
				if (state == nullptr)
					continue;

				PhoneStateModel phoneState;
				phoneState.phone = phone;
				phoneState.stateInd = stateInd;
				phoneState.state = state;
				resultStates.push_back(phoneState);
				++statesAdded;

				for (int streamInd = 0; streamInd < state->nstream; ++streamInd)
				{
					HTK_HMM_PDF* pdfInfo = state->pdf[streamInd];
					if (pdfInfo == nullptr)
						continue;


					//std::vector<float> weightsCopy(pdfInfo->mix_num);
					//for (size_t i=0; i<weightsCopy.size(); ++i) {
					//    auto tmp = pdfInfo->bweight[i];
					//    weightsCopy[i] = tmp;
					//}

					//for (int densInd = 0; densInd < pdfInfo->mix_num; ++densInd)
					//{
					//    auto dens = pdfInfo->b[densInd];
					//    if (dens == nullptr)
					//        continue;

					//}
				}
			}
		}
		return statesAdded;
	}

	std::vector<std::tuple<size_t, size_t>> AlignPhonesHelper(const std::vector<PhoneStateModel>& expectedStates, const std::vector<std::vector<float>>& frames, size_t tailSize,
		std::vector<PhoneStateDistribution>& stateDistrs, double& alignmenScore)
	{
		// prepare emit prob table

		size_t statesCount = expectedStates.size();
		size_t framesCount = frames.size();

		// find distinct phone (state) distributions among expected states
		std::vector<size_t> stateIndexToPhoneDistributionIndex(statesCount);
		std::vector<PhoneStateModel> distinctPhoneDistributions;
		distinctPhoneDistributions.reserve(statesCount);

		{
			std::hash_map<int, HTK_HMM_State*> stateStateIdToDensity;
			for (size_t i = 0; i < expectedStates.size(); ++i)
			{
				const PhoneStateModel& phoneInfo = expectedStates[i];

				// TODO: why only unique states where selected?
				//if (stateStateIdToDensity.find(phoneInfo.state->id) != stateStateIdToDensity.end())
				//	continue;

				distinctPhoneDistributions.push_back(phoneInfo);
				stateIndexToPhoneDistributionIndex[i] = distinctPhoneDistributions.size() - 1;

				stateStateIdToDensity[phoneInfo.state->id] = phoneInfo.state;
			}
		}

		// construct cache: distinct states vs frames

		size_t distinctStatesCount = distinctPhoneDistributions.size();

		std::vector<double> emitLogProb(distinctStatesCount * framesCount);
		for (size_t frameIndex = 0; frameIndex < frames.size(); ++frameIndex)
		{
			for (size_t stateIndex = 0; stateIndex < distinctStatesCount; ++stateIndex)
			{
				const std::vector<float>& fr = frames[frameIndex];

				//
				HTK_HMM_State* state = distinctPhoneDistributions[stateIndex].state;

				auto prob = calculateDensityLogProb(state, const_cast<VECT*>(&fr[0]), fr.size());
				if (prob == std::numeric_limits<decltype(prob)>::lowest())
					throw std::exception("prob == inf");
				emitLogProb[stateIndex + frameIndex * distinctStatesCount] = prob;
			}
		}

		//
		std::function<double(size_t, size_t)> emitFun = [&emitLogProb, distinctStatesCount, &stateIndexToPhoneDistributionIndex](size_t stateIndex, size_t frameIndex) -> double
		{
			auto distinctStateIndex = stateIndexToPhoneDistributionIndex[stateIndex];
			return emitLogProb[distinctStateIndex + frameIndex * distinctStatesCount];
		};

		std::vector<std::tuple<size_t, size_t>> resultAlignedStates;
		PticaGovorun::PhoneAlignment phoneAligner(distinctStatesCount, frames.size(), emitFun);
		phoneAligner.compute(resultAlignedStates);

		phoneAligner.populateStateDistributions(resultAlignedStates, tailSize, stateDistrs);

		alignmenScore = phoneAligner.getAlignmentScore();

		return resultAlignedStates;
	}

	void JuliusToolWrapper::alignPhones(const short* audioSamples, int audioSamplesCount, const std::vector<std::string>& speechPhones, const AlignmentParams& paramsInfo, int tailSize, PhoneAlignmentInfo& alignmentResult)
	{
		std::vector<short> sampleData(audioSamples, audioSamples + audioSamplesCount);

		//

		Recog *recog = myGlobalRecog_;
		size_t frameSize = -1;
		size_t frameShift = -1;
		size_t mfccVecLen = -1;
		Value& mfccInfo = *recog->mfcclist->para;
		std::vector<std::vector<float>> frames = computeMfccFeatures(sampleData, recog, frameSize, frameShift, mfccVecLen); // convert samples to features (MFCC)
		frameSize = recog->mfcclist->para->framesize;
		frameShift = recog->mfcclist->para->frameshift;

		//
		HTK_HMM_INFO* hmmInfo = recog->amlist->hmminfo;

		// convert phone list to list of densities
		std::vector<PhoneStateModel> expectedStates;
		for (int i = 0; i < speechPhones.size(); ++i)
		{
			auto phoneStr = speechPhones[i];

			int statesAdded = FindPhoneStates(hmmInfo, phoneStr, expectedStates);
			if (statesAdded == 0)
			{
				std::stringstream ss;
				ss << "Can't find states for phone=" << phoneStr << std::endl;
				throw std::exception(ss.str().c_str());
			}
		}

		std::vector<PhoneStateDistribution> stateDistribs;
		double alignmenScore = 0;
		auto alignedStates = AlignPhonesHelper(expectedStates, frames, tailSize, stateDistribs, alignmenScore);

		// marshal results

		//PhoneAlignmentInfo result;
		PhoneAlignmentInfo& result = alignmentResult;
		result.UsedFrameShift = frameShift;
		result.UsedFrameSize = frameSize;
		result.AlignmentScore = (float)alignmenScore;

		auto alignInfo = std::vector<AlignedPhoneme>(alignedStates.size());
		for (size_t i = 0; i < alignedStates.size(); ++i)
		{
			const auto& seg = alignedStates[i];

			long begSample = -1;
			long endSample = -1;
			frameRangeToSampleRange(std::get<0>(seg), std::get<1>(seg), paramsInfo.TakeSample, frameSize, frameShift, begSample, endSample);

			AlignedPhoneme align;
			align.BegSample = begSample;
			align.EndSample = endSample;


			// state name

			std::string stateName;

			auto pState = expectedStates[i].state;
			if (pState->name != nullptr)
				stateName = pState->name;

			if (stateName.empty())
			{
				auto ph = expectedStates[i].phone;
				if (ph->name != nullptr)
				{
					std::stringstream ss;
					ss << ph->name << expectedStates[i].stateInd + 1;
					stateName = ss.str();
				}
			}

			if (stateName.empty())
				stateName = "?"; // default state name

			align.Name = stateName;

			alignInfo[i] = align;
		}

		std::vector<AlignedPhoneme> monoPhones;
		mergeSamePhoneStates(alignInfo, monoPhones);

		result.AlignInfo = monoPhones;

		auto stateDistribsMng = std::vector<PhoneDistributionPart>(stateDistribs.size());
		for (size_t i = 0; i < stateDistribs.size(); ++i)
		{
			PhoneDistributionPart distr;
			distr.OffsetFrameIndex = stateDistribs[i].OffsetFrameIndex;

			//distr.LogProbs = stateDistribs[i].LogProbs;
			distr.LogProbs.resize(stateDistribs[i].LogProbs.size());
			std::transform(std::begin(stateDistribs[i].LogProbs), std::end(stateDistribs[i].LogProbs), std::begin(distr.LogProbs),
				[](double p) { return (float)p; });

			stateDistribsMng[i] = distr;
		}
		result.PhoneDistributions = stateDistribsMng;

		//return result;
	}

	const RecognizerSettings& JuliusToolWrapper::settings() const
	{
		return recognizerSettings_;
	}

	bool initRecognizerConfiguration(const std::string& recogName, RecognizerSettings& rs)
	{
		if (recogName == "shrekky")
		{
			rs.FrameSize = FrameSize;
			rs.FrameShift = FrameShift;
			rs.SampleRate = SampleRate;
			rs.UseWsp = false;

			rs.DictionaryFilePath = R"path(C:\devb\PticaGovorunProj\data\shrekky\shrekkyDic.voca)path";
			rs.LanguageModelFilePath = R"path(C:\devb\PticaGovorunProj\data\shrekky\shrekkyLM.blm)path";
			rs.AcousticModelFilePath = R"path(C:\devb\PticaGovorunProj\data\shrekky\shrekkyAM.bam)path";

			rs.LogFile = recogName + "-LogFile.txt";
			rs.FileListFileName = recogName + "-FileList.txt";
			rs.TempSoundFile = recogName + "-TmpAudioFile.wav";
			rs.CfgFileName = recogName + "-Config.txt";
			rs.CfgHeaderFileName = recogName + "-ConfigHeader.txt";
		}
		else if (recogName == "persian")
		{
			rs.FrameSize = FrameSize;
			rs.FrameShift = FrameShift;
			rs.SampleRate = SampleRate;
			rs.UseWsp = false; //?

			rs.DictionaryFilePath = R"path(C:\progs\cygwin\home\mmore\voxforge_p111-117\lexicon\voxforge_lexicon)path";
			rs.LanguageModelFilePath = R"path(C:\progs\cygwin\home\mmore\voxforge_p111-117\auto\persian.ArpaLM.bi.bin)path";
			rs.AcousticModelFilePath = R"path(C:\progs\cygwin\home\mmore\voxforge_p111-117\auto\acoustic_model_files\hmmdefs)path";
			rs.TiedListFilePath = R"path(C:\progs\cygwin\home\mmore\voxforge_p111-117\auto\acoustic_model_files\tiedlist)path";

			rs.LogFile = recogName + "-LogFile.txt";
			rs.FileListFileName = recogName + "-FileList.txt";
			rs.TempSoundFile = recogName + "-TmpAudioFile.wav";
			rs.CfgFileName = recogName + "-Config.txt";
			rs.CfgHeaderFileName = recogName + "-ConfigHeader.txt";
		}

		return true;
	}

}