#include "stdafx.h"
#include <vector>
#include <array>
#include <iostream>
#include <locale>
#include <set>

#include <windows.h>

#include <pocketsphinx.h>
#include <QDir>
#include <QTextCodec>
#include <QDebug>
#include <QXmlStreamWriter>

#include "ps_alignment.h"
#include "state_align_search.h"

#include <samplerate.h>

#include "WavUtils.h" // readAllSamples
#include "SpeechProcessing.h"
#include "StringUtils.h" // findEditDistance
#include "CoreUtils.h" // timeStamp
#include "ClnUtils.h"
#include "PhoneticService.h"
#include "SphinxModel.h"
#include "assertImpl.h"
#include "AppHelpers.h"

namespace RecognizeSpeechSphinxTester
{
	using namespace PticaGovorun;
	const float CmuSphinxFrameRate = 16000;

	void recognizeWav()
	{
		//SetConsoleOutputCP(1251);
		//SetConsoleCP(1251);

		std::locale::global(std::locale("ukrainian"));
		//::setlocale(LC_ALL, ".65010");
		//::setlocale(LC_ALL, "Ukrainian_Ukraine.866");
		//SetConsoleOutputCP(866);
		
		//SetConsoleCP(65001);
		//SetConsoleOutputCP(65001);

		const char* wavFilePath = R"path(E:\devb\workshop\PticaGovorunProj\data\!\chitki16000Hz.wav)path";
		std::vector<short> audioSamples;
		float frameRate = -1;
		auto readOp = PticaGovorun::readAllSamples(wavFilePath, audioSamples, &frameRate);
		if (!std::get<0>(readOp))
		{
			std::cerr << "Can't read wav file" << std::endl;
			return;
		}
		if (frameRate != CmuSphinxFrameRate)
		{
			std::cerr << "CMU PocketSphinx words with 16KHz files only" << std::endl;
			return;
		}

		const char* hmmPath = R"path(C:/devb/PticaGovorunProj/data/TrainSphinx/persian/model_parameters/persian.cd_cont_200/)path";
		const char* langModelPath = R"path(C:/devb/PticaGovorunProj/data/TrainSphinx/persian/etc/persian.lm.DMP)path";
		const char* dictPath = R"path(C:/devb/PticaGovorunProj/data/TrainSphinx/persian/etc/persian.dic)path";
		cmd_ln_t *config = SphinxConfig::pg_init_cmd_ln_t(hmmPath, langModelPath, dictPath, true, false, true, boost::string_ref());
		if (config == nullptr)
			return;

		ps_decoder_t *ps = ps_init(config);
		if (ps == nullptr)
			return;

		int rv = ps_start_utt(ps);
		if (rv < 0)
			return;

		std::array<short,512> buf;
		int packetsCount = audioSamples.size() / buf.size();
		for (size_t i = 0; i < packetsCount; ++i) {
			std::copy_n(audioSamples.data() + i*buf.size(), buf.size(), buf.data());

			rv = ps_process_raw(ps, buf.data(), buf.size(), false, false);
		}

		//
		rv = ps_end_utt(ps);
		if (rv < 0)
			return;

		//
		int32 score;
		const char* hyp = ps_get_hyp(ps, &score);
		if (hyp == nullptr)
		{
			std::wcout << "No hypothesis is available" << std::endl;
			return;
		}

		QTextCodec* textCodec = QTextCodec::codecForName("utf8");
		std::wstring hypWStr = textCodec->toUnicode(hyp).toStdWString();

		printf("Recognized: %s\n", hyp);
		std::wcout << "Recognized: " << hyp << std::endl;
		std::wcout << "Recognized: " << hypWStr << std::endl;

		// print word segmentation
		ps_seg_t *seg;
		for (seg = ps_seg_iter(ps); seg; seg = ps_seg_next(seg))
		{
			const char* word = ps_seg_word(seg);
			std::wstring wordWStr = textCodec->toUnicode(word).toStdWString();
			
			int sf, ef;
			ps_seg_frames(seg, &sf, &ef);

			int32 lscr, ascr, lback;
			int32 post = ps_seg_prob(seg, &ascr, &lscr, &lback); // posterior probability?

			float64 prob = logmath_exp(ps_get_logmath(ps), post);
			std::wcout << wordWStr.c_str();
			std::wcout 
				<< " " << sf << " " <<  ef << " " <<prob
				<< " " << ascr << " " << lscr << " " << lback << std::endl;
		}

		//ps_lattice_t* lat = ps_get_lattice(ps);
		
		//ps_search_t *search;
		//acmod_t *acmod;
		//acmod = ps->acmod;
		//ps_alignment_t *al;
		//search = state_align_search_init(config, acmod, al)

		ps_free(ps);
	}

	template <typename Letter>
	class WordErrorCosts {
	public:
		typedef float CostType;

		static CostType getZeroCosts() {
			return 0;
		}
		inline CostType getInsertSymbolCost(Letter x) const {
			return 1;
		}
		inline CostType getRemoveSymbolCost(Letter x) const {
			return 1;
		}
		inline CostType getSubstituteSymbolCost(Letter x, Letter y) const {
			return x == y ? 0 : 1;
		}
	};
	
	template <typename T>
	bool startsWith(wv::slice<T> items, wv::slice<T> prefix)
	{
		if (prefix.size() > items.size())
			return false; // prefix will not fit the items?

		for (int i = 0; i < prefix.size(); ++i)
		{
			if (prefix[i] == items[i])
				continue;
			return false;
		}
		return true;
	}

	class CharPhonationGroupCosts {
	public:
		typedef int CostType;
		static int getZeroCosts() {
			return 0;
		}
		inline int getInsertSymbolCost(wchar_t x) const {
			return 1;
		}
		inline int getRemoveSymbolCost(wchar_t x) const {
			return 1;
		}
		inline int getSubstituteSymbolCost(wchar_t x, wchar_t y) const
		{
			if (x == y) return 0;
			boost::optional<CharGroup> xClass = classifyUkrainianChar(x);
			boost::optional<CharGroup> yClass = classifyUkrainianChar(y);
			if (xClass && xClass == yClass) // chars from the same group
				return 1;
			return 99; // chars from different groups
		}
	};

	// Checks whether {a1,a2} = {b1, b2}.
	template <typename T>
	bool setTwoEq(T a1, T a2, T b1, T b2)
	{
		return (a1 == b1 && a2 == b2) || (a1 == b2 && a2 == b1);
	}

	class PhoneProximityCosts {
		const PhoneRegistry& phoneReg_;
		std::set<PhoneId> groupBP_;
		std::set<PhoneId> groupVF_;
		std::set<PhoneId> groupHKH_;
		std::set<PhoneId> groupHG_;
		std::set<PhoneId> groupDT_;
		std::set<PhoneId> groupZS_;
		std::set<PhoneId> groupZHSH_;
		PhoneId consonantJ_;
	public:
		typedef float CostType;

		PhoneProximityCosts(const PhoneRegistry& phoneReg) : phoneReg_(phoneReg)
		{
			consonantJ_ = phoneReg.phoneIdSingle("J", SoftHardConsonant::Hard, nullptr).get();

			std::vector<PhoneId> phoneIds;
			phoneReg.findPhonesByBasicPhoneStr("B", phoneIds);
			phoneReg.findPhonesByBasicPhoneStr("P", phoneIds);
			groupBP_.insert(phoneIds.begin(), phoneIds.end());

			phoneIds.clear();
			phoneReg.findPhonesByBasicPhoneStr("V", phoneIds);
			phoneReg.findPhonesByBasicPhoneStr("F", phoneIds);
			groupVF_.insert(phoneIds.begin(), phoneIds.end());

			phoneIds.clear();
			phoneReg.findPhonesByBasicPhoneStr("H", phoneIds);
			phoneReg.findPhonesByBasicPhoneStr("KH", phoneIds);
			groupHKH_.insert(phoneIds.begin(), phoneIds.end());

			phoneIds.clear();
			phoneReg.findPhonesByBasicPhoneStr("H", phoneIds);
			phoneReg.findPhonesByBasicPhoneStr("G", phoneIds);
			groupHG_.insert(phoneIds.begin(), phoneIds.end());

			phoneIds.clear();
			phoneReg.findPhonesByBasicPhoneStr("D", phoneIds);
			phoneReg.findPhonesByBasicPhoneStr("T", phoneIds);
			groupDT_.insert(phoneIds.begin(), phoneIds.end());

			phoneIds.clear();
			phoneReg.findPhonesByBasicPhoneStr("Z", phoneIds);
			phoneReg.findPhonesByBasicPhoneStr("S", phoneIds);
			groupZS_.insert(phoneIds.begin(), phoneIds.end());

			phoneIds.clear();
			phoneReg.findPhonesByBasicPhoneStr("ZH", phoneIds);
			phoneReg.findPhonesByBasicPhoneStr("SH", phoneIds);
			groupZHSH_.insert(phoneIds.begin(), phoneIds.end());
		}

		static CostType getZeroCosts() {
			return 0;
		}
		inline CostType getInsertSymbolCost(const PhoneId& x) const {
			return 3;
		}
		inline CostType getRemoveSymbolCost(const PhoneId& x) const {
			return 3;
		}
		inline CostType getSubstituteSymbolCost(const PhoneId&  x, const PhoneId&  y) const
		{
			if (x == y) return 0;

			const Phone* ph1 = phoneReg_.phoneById(x);
			const Phone* ph2 = phoneReg_.phoneById(y);

			// the same basic phone
			if (ph1->BasicPhoneId == ph2->BasicPhoneId)
				return 1;

			// two phones sound similar
			const CostType SimilarSound = 2;
			if (groupBP_.find(x) != groupBP_.end() && groupBP_.find(y) != groupBP_.end())
				return SimilarSound;
			if (groupVF_.find(x) != groupVF_.end() && groupVF_.find(y) != groupVF_.end())
				return SimilarSound;
			if (groupHKH_.find(x) != groupHKH_.end() && groupHKH_.find(y) != groupHKH_.end())
				return SimilarSound;
			if (groupHG_.find(x) != groupHG_.end() && groupHG_.find(y) != groupHG_.end())
				return SimilarSound;
			if (groupDT_.find(x) != groupDT_.end() && groupDT_.find(y) != groupDT_.end())
				return SimilarSound;
			if (groupZS_.find(x) != groupZS_.end() && groupZS_.find(y) != groupZS_.end())
				return SimilarSound;
			if (groupZHSH_.find(x) != groupZHSH_.end() && groupZHSH_.find(y) != groupZHSH_.end())
				return SimilarSound;
			// DZ DZH?

			// though J is a consonant, better to avoid substituting it with other consonants (or vowels)
			if (x == consonantJ_ || y == consonantJ_)
				return 99;

			//
			const BasicPhone* basicPh1 = phoneReg_.basicPhone(ph1->BasicPhoneId);
			const BasicPhone* basicPh2 = phoneReg_.basicPhone(ph2->BasicPhoneId);
			if (basicPh1->DerivedFromChar == basicPh2->DerivedFromChar)
				return 3;
			return 99; // chars from different groups
		}
	};

	std::vector<std::wstring> toStdWStringVec(const std::vector<boost::wstring_ref>& refs)
	{
		std::vector<std::wstring> result(refs.size());
		std::transform(refs.begin(), refs.end(), result.begin(), [](boost::wstring_ref r) -> std::wstring 
		{
			return toStdWString(r);
		});
		return result;
	}

	struct TranscribedAudioSegment
	{
		std::wstring RelFilePathNoExt; // relative path to speech file
		std::wstring Transcription;
		std::vector<short> Frames;
		float FrameRate;
	};

	// Result of comparing expected text with decoded speech.
	struct TwoUtterances
	{
		std::wstring RelFilePathNoExt; // relative path to speech file
		float ErrorWord; // edit distance between utterances as a sequence of words
		float ErrorChars; // edit distance between utterances as a sequence of chars
		TranscribedAudioSegment Segment;
		std::wstring TextActual;
		std::vector<boost::wstring_ref> PronIdsExpected; // слова(2)
		std::vector<boost::wstring_ref> WordsExpected; // слова without any braces
		std::vector<std::wstring> PronIdsActualRaw;
		std::vector<std::wstring> WordsActual;
		std::vector<float> WordProbs; // confidence of each recognized word
		std::vector<float> WordProbsMerged;
		std::vector<PhoneId> PhonesExpected;
		std::vector<PhoneId> PhonesActual;
		std::string PhonesExpectedAlignedStr;
		std::string PhonesActualAlignedStr;
	};

	void decodeSpeechSegments(const std::vector<TranscribedAudioSegment>& segs,
		
		ps_decoder_t *ps, float targetFrameRate, std::vector<TwoUtterances>& recogUtterances,
		std::vector<int>& phoneConfusionMat, int phonesCount,
		int& sentErrorTotalCount,
		int& wordErrorTotalCount, int& wordTotalCount,
		int& phoneErrorTotalCount, int& phoneTotalCount,
		EditDistance<PhoneId, PhoneProximityCosts>& phonesEditDist,
		const PhoneProximityCosts& editCost,
		const std::map<boost::wstring_ref, boost::wstring_ref>& pronCodeToWordWellFormed,
		const std::map<boost::wstring_ref, PronunciationFlavour>& pronCodeToPronTest,
		const std::map<boost::wstring_ref, PronunciationFlavour>& pronCodeToPronFiller,
		const PhoneRegistry& phoneReg, bool excludeSilFromDecOutput)
	{
		QTextCodec* textCodec = QTextCodec::codecForName("utf8");

		for (int i = 0; i < segs.size(); ++i)
		{
			const TranscribedAudioSegment& seg = segs[i];

			// decode

			const std::vector<short>* speechFramesActual = &seg.Frames;
			std::vector<short> speechFramesResamp;
			bool requireResampling = seg.FrameRate != targetFrameRate;
			if (requireResampling)
			{
				std::vector<float> inFramesFloat(std::begin(seg.Frames), std::end(seg.Frames));
				std::vector<float> outFramesFloat(inFramesFloat.size(), 0);

				SRC_DATA convertData;
				convertData.data_in = inFramesFloat.data();
				convertData.input_frames = inFramesFloat.size();
				convertData.data_out = outFramesFloat.data();
				convertData.output_frames = outFramesFloat.size();
				convertData.src_ratio = targetFrameRate / seg.FrameRate;

				int converterType = SRC_SINC_BEST_QUALITY;
				int channels = 1;
				int error = src_simple(&convertData, converterType, channels);
				if (error != 0)
				{
					const char* msg = src_strerror(error);
					std::cerr << msg;
					break;
				}

				outFramesFloat.resize(convertData.output_frames_gen);
				speechFramesResamp.assign(std::begin(outFramesFloat), std::end(outFramesFloat));
				speechFramesActual = &speechFramesResamp;
			}

			std::wcout << L"SpeechFile=" << seg.RelFilePathNoExt << std::endl;
			int rv = ps_start_utt(ps);
			if (rv < 0)
			{
				std::cerr << "Error: Can't start utterance";
				break;
			}

			bool fullUtterance = true;
			rv = ps_process_raw(ps, speechFramesActual->data(), speechFramesActual->size(), false, fullUtterance);
			if (rv < 0)
			{
				std::cerr << "Error: Can't process utterance";
				break;
			}
			rv = ps_end_utt(ps);
			if (rv < 0)
			{
				std::cerr << "Error: Can't end utterance";
				break;
			}

			//
			int32 score;
			const char* hyp = ps_get_hyp(ps, &score);
			if (hyp == nullptr)
			{
				// "No hypothesis is available"
				hyp = "";
			}

			std::wstring hypWStr = textCodec->toUnicode(hyp).toStdWString();

			// find words and word probabilities
			std::vector<std::wstring> pronIdsActualRaw;
			std::vector<float> wordsActualProbs;
			for (ps_seg_t *recogSeg = ps_seg_iter(ps); recogSeg; recogSeg = ps_seg_next(recogSeg))
			{
				const char* word = ps_seg_word(recogSeg);
				std::wstring wordWStr = textCodec->toUnicode(word).toStdWString();
				pronIdsActualRaw.push_back(wordWStr);

				int startFrame, endFrame;
				ps_seg_frames(recogSeg, &startFrame, &endFrame);

				int32 lscr, ascr, lback;
				int32 post = ps_seg_prob(recogSeg, &ascr, &lscr, &lback); // posterior probability?

				float64 prob = logmath_exp(ps_get_logmath(ps), post);
				wordsActualProbs.push_back((float)prob);
			}

			// split expected text into words

			std::vector<boost::wstring_ref> pronCodesExpected;
			splitUtteranceIntoWords(seg.Transcription, pronCodesExpected);

			// merge actual pronCodes (word parts)

			std::vector<std::wstring> pronCodesActualMergedStr;
			std::vector<float> pronCodesActualProbsMerged;
			if (!pronIdsActualRaw.empty())
			{
				pronCodesActualMergedStr.push_back(pronIdsActualRaw.front());
				pronCodesActualProbsMerged.push_back(wordsActualProbs.front());
			}
			for (size_t wordInd = 1; wordInd < pronIdsActualRaw.size(); ++wordInd)
			{
				const std::wstring& prev = pronCodesActualMergedStr.back();
				const std::wstring& cur = pronIdsActualRaw[wordInd];
				float prevProb = pronCodesActualProbsMerged.back();
				float curProb = wordsActualProbs[wordInd];
				if (prev.back() == L'~' && cur.front() == L'~')
				{
					std::wstring merged = prev;
					merged.pop_back();
					merged.append(cur.data() + 1, cur.size() - 1);

					pronCodesActualMergedStr.back() = merged;
					pronCodesActualProbsMerged.back() = prevProb * curProb;
				}
				else
				{
					pronCodesActualMergedStr.push_back(cur);
					pronCodesActualProbsMerged.push_back(curProb);
				}
			}

			std::vector<boost::wstring_ref> pronCodesActualMerged;
			std::transform(pronCodesActualMergedStr.begin(), pronCodesActualMergedStr.end(), std::back_inserter(pronCodesActualMerged), [](std::wstring& w)
			{
				return boost::wstring_ref(w);
			});

			// expected no sil

			std::vector<boost::wstring_ref> pronCodesExpectedNoSil;
			std::remove_copy_if(std::begin(pronCodesExpected), std::end(pronCodesExpected), std::back_inserter(pronCodesExpectedNoSil), [&](boost::wstring_ref pronCode)
			{
				return pronCodeToPronFiller.find(pronCode) != pronCodeToPronFiller.end();
			});

			// actual no sil

			std::vector<boost::wstring_ref> pronCodesActualNoSil;
			std::remove_copy_if(std::begin(pronCodesActualMerged), std::end(pronCodesActualMerged), std::back_inserter(pronCodesActualNoSil), [&](boost::wstring_ref pronCode)
			{
				return pronCodeToPronFiller.find(pronCode) != pronCodeToPronFiller.end();
			});

			// expected words

			const auto& pronCodesExpectedSrc = excludeSilFromDecOutput ? pronCodesExpectedNoSil : pronCodesExpected;

			// map pronId -> word (eg. слова(2) -> слова)
			std::vector<boost::wstring_ref> wordsExpected;
			for (boost::wstring_ref pronId : pronCodesExpectedSrc)
			{
				//auto wordIt = pronCodeToWordWellFormed.find(pronId);
				//if (wordIt != pronCodeToWordWellFormed.end())
				//{
				//	boost::wstring_ref word = wordIt->second;
				//	wordsExpected.push_back(word);
				//	continue;
				//}
				//wordsExpected.push_back(pronId);
				boost::wstring_ref baseWord;
				parsePronId(pronId, baseWord);
				wordsExpected.push_back(baseWord);
			}

			// actual words

			const auto& pronCodesActualSrc = excludeSilFromDecOutput ? pronCodesActualNoSil : pronCodesActualMerged;

			// map pronId -> word (eg. слова(2) -> слова)
			std::vector<boost::wstring_ref> wordsActual;
			std::transform(pronCodesActualSrc.begin(), pronCodesActualSrc.end(), std::back_inserter(wordsActual), [&pronCodeToWordWellFormed](boost::wstring_ref pronCode)
			{
//				auto wordIt = pronCodeToWordWellFormed.find(pronCode);
//				if (wordIt != pronCodeToWordWellFormed.end())
//				{
//					boost::wstring_ref word = wordIt->second;
//					return word;
//				}
//				return pronCode;
				boost::wstring_ref baseWord;
				parsePronId(pronCode, baseWord);
				return baseWord;
			});

			// compute word error

			WordErrorCosts<boost::wstring_ref> c;
			float distWord = findEditDistance(wordsExpected.begin(), wordsExpected.end(), wordsActual.begin(), wordsActual.end(), c);

			// compute edit distances between whole utterances (char distance)
			std::wostringstream strBuff;
			PticaGovorun::join(wordsExpected.begin(), wordsExpected.end(), boost::wstring_ref(L" "), strBuff);
			std::wstring wordsExpectedStr = strBuff.str();
			strBuff.str(L"");
			PticaGovorun::join(wordsActual.begin(), wordsActual.end(), boost::wstring_ref(L" "), strBuff);
			std::wstring wordsActualStr = strBuff.str();

			WordErrorCosts<wchar_t> charCost;
			float distChar = findEditDistance(std::cbegin(wordsExpectedStr), std::cend(wordsExpectedStr), std::cbegin(wordsActualStr), std::cend(wordsActualStr), charCost);

			// do phonetic expansion

			auto expandPronCodeFun = [&](boost::wstring_ref pronCode) -> const PronunciationFlavour*
			{
				auto fillerIt = pronCodeToPronFiller.find(pronCode);
				if (fillerIt != pronCodeToPronFiller.end())
					return &fillerIt->second;

				auto testIt = pronCodeToPronTest.find(pronCode);
				if (testIt != pronCodeToPronTest.end())
					return &testIt->second;

				return nullptr;
			};

			auto expandPronCodesFun = [&](const std::vector<boost::wstring_ref>& pronCodes, std::vector<PhoneId>& phones)
			{
				for (boost::wstring_ref pronCode : pronCodes)
				{
					const PronunciationFlavour* pron = expandPronCodeFun(pronCode);
					PG_Assert2(pron != nullptr, "The pronCode used in transcription must exist in primary/filler phonetic dictionary");

					std::copy(pron->Phones.begin(), pron->Phones.end(), std::back_inserter(phones));
				}
			};

			std::vector<PhoneId> expectedPhones;
			expandPronCodesFun(pronCodesExpectedSrc, expectedPhones);

			std::vector<PhoneId> actualPhones;
			expandPronCodesFun(pronCodesActualSrc, actualPhones);

			phonesEditDist.estimateAllDistances(expectedPhones, actualPhones, editCost);

			std::vector<EditStep> phoneEditRecipe;
			phonesEditDist.minCostRecipe(phoneEditRecipe);

			// update confusion matrix
			for (const EditStep& step : phoneEditRecipe)
			{
				int phoneIdExpect = -1;
				int phoneIdActual = -1;
				if (step.Change == StringEditOp::RemoveOp)
				{
					phoneIdExpect = expectedPhones[step.Remove.FirstRemoveInd].Id;
					phoneIdActual = 0;
				}
				else if (step.Change == StringEditOp::InsertOp)
				{
					phoneIdExpect = 0;
					phoneIdActual = actualPhones[step.Insert.SecondSourceInd].Id;
				}
				else if (step.Change == StringEditOp::SubstituteOp)
				{
					phoneIdExpect = expectedPhones[step.Substitute.FirstInd].Id;
					phoneIdActual = actualPhones[step.Substitute.SecondInd].Id;
				}
				PG_DbgAssert(phoneIdExpect != -1);
				PG_DbgAssert(phoneIdActual != -1);
				int row = phoneIdExpect;
				int col = phoneIdActual;
				phoneConfusionMat[col + phonesCount*row]++;
			}

			std::function<void(PhoneId, std::vector<char>&)> ph2StrFun = [&phoneReg](PhoneId ph, std::vector<char>& phVec)
			{
				std::string phStr;
				bool toStrOp = phoneToStr(phoneReg, ph, phStr);
				PG_Assert(toStrOp);
				std::copy(phStr.begin(), phStr.end(), std::back_inserter(phVec));
			};

			std::vector<char> align1;
			std::vector<char> align2;
			char padChar = '_';
			alignWords(wv::make_view(expectedPhones), wv::make_view(actualPhones), ph2StrFun, phoneEditRecipe, padChar, align1, align2, boost::make_optional(padChar));
			std::string expectedPhonesStr(align1.begin(), align1.end());
			std::string actualPhonesStr(align2.begin(), align2.end());

			//
			TwoUtterances utter;
			utter.RelFilePathNoExt = seg.RelFilePathNoExt;
			utter.ErrorWord = distWord;
			utter.ErrorChars = distChar;
			utter.Segment = seg;
			utter.TextActual = hypWStr;
			utter.PronIdsExpected = pronCodesExpected;
			utter.PronIdsActualRaw = pronIdsActualRaw;
			utter.WordsExpected = wordsExpected;
			utter.WordsActual = toStdWStringVec(wordsActual);
			utter.WordProbs = wordsActualProbs;
			utter.WordProbsMerged = pronCodesActualProbsMerged;
			utter.PhonesExpected = expectedPhones;
			utter.PhonesActual = actualPhones;
			utter.PhonesExpectedAlignedStr = expectedPhonesStr;
			utter.PhonesActualAlignedStr = actualPhonesStr;
			recogUtterances.push_back(utter);

			wordErrorTotalCount += (int)distWord;
			wordTotalCount += (int)wordsExpected.size();
			phoneErrorTotalCount += (int)distChar;
			phoneTotalCount += (int)expectedPhones.size();
			if (distWord > 0) sentErrorTotalCount += 1;
		}
	}

	void dumpConfusionMatrix(const std::vector<int>& phoneConfusionMat, int phonesCount, boost::wstring_ref outFilePath, const PhoneRegistry& phoneReg)
	{
		// find pivotal phone of high confusion
		std::vector<int> confData;
		for (int col = 1; col < phonesCount; ++col) // start index=1 to skip Nil phone
			for (int row = 1; row < phonesCount; ++row)
			{
				if (col == row) continue; // skip no confusion at diagonal
				int conflict = phoneConfusionMat[col + row*phonesCount];
				confData.push_back(conflict);
			}
		int kHighest = 7;
		std::nth_element(confData.begin(), confData.begin() + kHighest, confData.end(), std::greater<int>());
		int highConfusionThresh = confData[kHighest];
		

		QFile dumpFile(toQString(outFilePath));
		if (!dumpFile.open(QIODevice::WriteOnly | QIODevice::Text))
		{
			std::cerr << "Can't open ouput file" << std::endl;
			return;
		}

		QTextStream dumpFileStream(&dumpFile);
		dumpFileStream.setCodec("UTF-8");

		auto ph2StrEx = [&phoneReg](int phoneId, std::string& phStr) -> bool
		{
			if (phoneId == 0)
			{
				phStr = "nil";
				return true;
			}
			phoneReg.assumeSequentialPhoneIdsWithoutGaps();
			return phoneToStr(phoneReg, phoneId, phStr);
		};
		{
			dumpFileStream << "<table>";
			dumpFileStream << "<th>Conf</th>"; // top-left corner cell header

			// phone id goes [0..phonesCount)  Phone with id=0 corresponds to nil
			// print header
			std::string phStr;
			for (int col = 0; col < phonesCount; ++col)
			{
				bool phOp = ph2StrEx(col, phStr);
				PG_Assert(phOp);
				dumpFileStream <<"<th>" << QString::fromStdString(phStr) << "</th>";
			}

			// print content
			for (int row = 0; row < phonesCount; ++row)
			{
				dumpFileStream << "<tr>";

				// print row header
				bool phOp = ph2StrEx(row, phStr);
				PG_Assert(phOp);
				dumpFileStream << "<td>" << QString::fromStdString(phStr) << "</td>";

				// print the row's body
				for (int col = 0; col < phonesCount; ++col)
				{
					int conflict = phoneConfusionMat[col + row*phonesCount];

					dumpFileStream << "<td";
					if (conflict >= highConfusionThresh)
					{
						dumpFileStream << " style='color:red'";
					}

					dumpFileStream << ">";
					dumpFileStream << conflict << "</td>";
				}
				
				dumpFileStream << "</tr>";
			}
			dumpFileStream << "</table>";
		}
	}

	// Loads transcription and audio data in Sphinx format.
	bool loadSpeechSegmentsToDecode(int trunc, boost::wstring_ref fileIdPath, boost::wstring_ref transcrPath, boost::wstring_ref audioDir,
		std::vector<TranscribedAudioSegment>& segments)
	{
		std::vector<std::wstring> audioRelFilePathesNoExt;
		bool op = readSphinxFileFileId(fileIdPath, audioRelFilePathesNoExt);
		if (!op)
		{
			std::wcerr << "can't read Sphinx fileId file" << fileIdPath << std::endl;
			return false;
		}

		std::vector<SphinxTranscriptionLine> speechTranscr;
		op = readSphinxFileTranscription(transcrPath, speechTranscr);
		if (!op)
		{
			std::wcerr << "can't read Sphinx transcr file" << transcrPath << std::endl;
			return false;
		}

		op = checkFileIdTranscrConsistency(audioRelFilePathesNoExt, speechTranscr);
		if (!op)
		{
			std::wcerr << "fileId and transcr files do not match. fileId=" << fileIdPath << " transcr=" << transcrPath << std::endl;
			return false;
		}

		std::vector<AudioData> audioDataList;
		op = loadSphinxAudio(audioDir, audioRelFilePathesNoExt, L"wav", audioDataList);
		if (!op)
		{
			std::wcerr << "can't read audio data. audioDir=" << audioDir << std::endl;
			return false;
		}
		PG_DbgAssert(audioDataList.size() == speechTranscr.size());

		//
		if (trunc != -1 && audioDataList.size() > trunc)
		{
			audioDataList.resize(trunc);
			speechTranscr.resize(trunc);
		}

		segments.resize(speechTranscr.size());
		for (size_t i = 0; i<segments.size(); ++i)
		{
			TranscribedAudioSegment& seg = segments[i];
			seg.RelFilePathNoExt = audioRelFilePathesNoExt[i];
			seg.Transcription = speechTranscr[i].Transcription;
			seg.Frames = std::move(audioDataList[i].Frames);
			seg.FrameRate = audioDataList[i].FrameRate;
		}
		return true;
	}

	std::tuple<bool, const char*> dumpDecoderExpectActualResults(boost::wstring_ref filePath, const std::vector<TwoUtterances>& recogUtterances)
	{
		QFile dumpFile(toQString(filePath));
		if (!dumpFile.open(QIODevice::WriteOnly | QIODevice::Text))
			return std::make_tuple(false, "Can't open ouput file");

		QTextStream dumpFileStream(&dumpFile);
		dumpFileStream.setCodec("UTF-8");

		boost::wstring_ref separ(L" ");
		std::wostringstream buff;
		CharPhonationGroupCosts c;
		EditDistance<wchar_t, CharPhonationGroupCosts> editDist;
		for (int i = 0; i < recogUtterances.size(); ++i)
		{
			const TwoUtterances& utter = recogUtterances[i];
			if (utter.ErrorWord == 0) // skip correct utterances
				continue;
			dumpFileStream << "WordError=" << utter.ErrorWord << "/" << utter.WordsExpected.size() << " " << "CharError=" << utter.ErrorChars << " RelWavPath=" << QString::fromStdWString(utter.Segment.RelFilePathNoExt) << "\n";

			dumpFileStream << "ExpectProns=" << QString::fromStdWString(utter.Segment.Transcription) << "\n";

			buff.str(L"");
			PticaGovorun::join(utter.PronIdsActualRaw.cbegin(), utter.PronIdsActualRaw.cend(), separ, buff);
			dumpFileStream << "ActualRaw=" << QString::fromStdWString(buff.str()) << "\n";

			dumpFileStream << "WordProbs=";
			for (int wordInd = 0; wordInd < utter.PronIdsActualRaw.size(); ++wordInd)
			{
				dumpFileStream << QString::fromStdWString(utter.PronIdsActualRaw[wordInd]) << " ";
				dumpFileStream << QString("%1").arg(utter.WordProbs[wordInd], 0, 'f', 2) << " ";
			}
			dumpFileStream << "\n"; // end after last prob

			buff.str(L"");
			PticaGovorun::join(utter.WordsExpected.cbegin(), utter.WordsExpected.cend(), separ, buff);
			std::wstring expectWords = buff.str();
			//dumpFileStream << "ExpectWords=" << QString::fromStdWString(expectWords) << "\n";

			buff.str(L"");
			PticaGovorun::join(utter.WordsActual.cbegin(), utter.WordsActual.cend(), separ, buff);
			std::wstring actualWords = buff.str();
			//dumpFileStream << "ActualWords=" << QString::fromStdWString(actualWords) << "\n";

			//
			editDist.estimateAllDistances(expectWords, actualWords, c);
			std::vector<EditStep> editRecipe;
			editDist.minCostRecipe(editRecipe);
			std::vector<wchar_t> alignWord1;
			std::vector<wchar_t> alignWord2;
			alignWords(wv::make_view(expectWords), wv::make_view(actualWords), editRecipe, L'_', alignWord1, alignWord2);
			dumpFileStream << "ExpectWords=" << QString::fromStdWString(std::wstring(alignWord1.begin(), alignWord1.end())) << "\n";
			dumpFileStream << "ActualWords=" << QString::fromStdWString(std::wstring(alignWord2.begin(), alignWord2.end())) << "\n";


			// expected-actual phones
			dumpFileStream << "ExpectPhones=" << QString::fromStdString(utter.PhonesExpectedAlignedStr) << "\n";
			dumpFileStream << "ActualPhones=" << QString::fromStdString(utter.PhonesActualAlignedStr) << "\n";

			dumpFileStream << "\n"; // line between utterances
		}
		return std::make_tuple(true, nullptr);
	}

	std::tuple<bool, const char*> dumpDecoderExpectActualResultsXml(boost::wstring_ref filePath, const std::vector<TwoUtterances>& recogUtterances, const std::wstring& speechModelVerStr)
	{
		QFile file(toQString(filePath));
		if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
			return std::make_tuple(false, "Can't open file for writing");

		QXmlStreamWriter xmlWriter(&file);
		xmlWriter.setCodec("utf-8");
		xmlWriter.setAutoFormatting(true);
		xmlWriter.setAutoFormattingIndent(3);

		xmlWriter.writeStartDocument("1.0");
		xmlWriter.writeStartElement("decRecogResult");

		boost::wstring_ref separ(L" ");
		std::wostringstream buff;
		CharPhonationGroupCosts c;
		EditDistance<wchar_t, CharPhonationGroupCosts> editDist;
		for (int i = 0; i < recogUtterances.size(); ++i)
		{
			const TwoUtterances& utter = recogUtterances[i];

			// note, dump utterances with utter.ErrorWord == 0 too

			xmlWriter.writeStartElement("utter");

			xmlWriter.writeAttribute("relWavPath", QString::fromStdWString(utter.Segment.RelFilePathNoExt));

			xmlWriter.writeAttribute("speechVer", QString::fromStdWString(speechModelVerStr));
			xmlWriter.writeAttribute("wordDist", QString::number(utter.ErrorWord));
			xmlWriter.writeAttribute("charDist", QString::number(utter.ErrorChars));
			xmlWriter.writeAttribute("expWordsNum", QString::number(utter.WordsExpected.size()));

			xmlWriter.writeTextElement("pronsExpect", QString::fromStdWString(utter.Segment.Transcription));

			buff.str(L"");
			PticaGovorun::join(utter.PronIdsActualRaw.cbegin(), utter.PronIdsActualRaw.cend(), separ, buff);
			xmlWriter.writeTextElement("pronsActual", QString::fromStdWString(buff.str()));

			buff.str(L"");
			for (int wordInd = 0; wordInd < utter.PronIdsActualRaw.size(); ++wordInd)
			{
				buff << utter.PronIdsActualRaw[wordInd] <<" ";
				buff << QString("%1").arg(utter.WordProbs[wordInd], 0, 'f', 2).toStdWString() << " ";
			}
			xmlWriter.writeTextElement("wordProbs", QString::fromStdWString(buff.str()));

			buff.str(L"");
			PticaGovorun::join(utter.WordsExpected.cbegin(), utter.WordsExpected.cend(), separ, buff);
			std::wstring expectWords = buff.str();
			xmlWriter.writeTextElement("wordsExpect", QString::fromStdWString(expectWords));

			buff.str(L"");
			PticaGovorun::join(utter.WordsActual.cbegin(), utter.WordsActual.cend(), separ, buff);
			std::wstring actualWords = buff.str();
			xmlWriter.writeTextElement("wordsActual", QString::fromStdWString(actualWords));

			//
			editDist.estimateAllDistances(expectWords, actualWords, c);
			std::vector<EditStep> editRecipe;
			editDist.minCostRecipe(editRecipe);
			std::vector<wchar_t> alignWord1;
			std::vector<wchar_t> alignWord2;
			alignWords(wv::make_view(expectWords), wv::make_view(actualWords), editRecipe, L'_', alignWord1, alignWord2);
			xmlWriter.writeTextElement("alignWordsExpect", QString::fromStdWString(std::wstring(alignWord1.begin(), alignWord1.end())));
			xmlWriter.writeTextElement("alignWordsActual", QString::fromStdWString(std::wstring(alignWord2.begin(), alignWord2.end())));

			// expected-actual phones
			xmlWriter.writeTextElement("alignPhonesExpect", QString::fromStdString(utter.PhonesExpectedAlignedStr));
			xmlWriter.writeTextElement("alignPhonesActual", QString::fromStdString(utter.PhonesActualAlignedStr));

			xmlWriter.writeEndElement(); // "utter"
		}
		xmlWriter.writeEndElement(); // "decRecogCache"
		return std::make_tuple(true, nullptr);
	}

	void testSphincDecoder()
	{
		QString decodeSpeechModelDefault = AppHelpers::mapPath("data/TrainSphinx/persian");
		QString decodeSpeechModel = AppHelpers::configParamQString("decodeSpeechModel", decodeSpeechModelDefault);
		QDir decodeSpeechModelDir = QDir(decodeSpeechModel);
		std::wcout << "decodeSpeechModelDir=" << decodeSpeechModel.toStdWString() << std::endl;

		// NOTE: must have the trailing slash to correctly find the speechModelVerStr below
		QString hmmPath =       decodeSpeechModelDir.filePath("model_parameters/persian.cd_cont_200/");
		QString langModelPath = decodeSpeechModelDir.filePath("etc/persian_test.lm.DMP");
		QString dictPath =      decodeSpeechModelDir.filePath("etc/persian_test.dic");
		QString dictPathFiller =decodeSpeechModelDir.filePath(QString::fromWCharArray(L"etc/persian.filler"));

		QString fileIdPath =    decodeSpeechModelDir.filePath(QString::fromWCharArray(L"etc/persian_test.fileids"));
		QString transcrPath =   decodeSpeechModelDir.filePath(QString::fromWCharArray(L"etc/persian_test.transcription"));
		QString audioDir =      decodeSpeechModelDir.filePath(QString::fromWCharArray(L"wav"));

		QDir baseDir = QFileInfo(hmmPath).dir();
		baseDir.cdUp();
		baseDir.cdUp();
		std::wstring speechModelVerStr = sphinxModelVersionStr(baseDir.absolutePath().toStdWString());
		PG_DbgAssert(!speechModelVerStr.empty() && "Can't recognize speech model version")
		std::wcout << "speechModelVerStr=" << speechModelVerStr << std::endl;

		//
		int segsMax = AppHelpers::configParamInt("decode.maxSegsToProcess", -1);
		std::vector<TranscribedAudioSegment> segments;
		bool op = loadSpeechSegmentsToDecode(segsMax, fileIdPath.toStdWString(), transcrPath.toStdWString(), audioDir.toStdWString(), segments);
		if (!op)
		{
			std::wcerr << L"can't read Sphinx fileId file" << fileIdPath.toStdWString() << std::endl;
			return;
		}

		QString speechProjDirQStr = AppHelpers::configParamQString("speechProjDir", "speechProjDir_ERROR");
		QDir speechProjDir = QDir(speechProjDirQStr);
		if (!speechProjDir.exists())
		{
			std::wcerr << L"can't find default speech lang project directory " << speechProjDirQStr.toStdWString() << std::endl;
			return;
		}

		//

		bool excludeSilFromDecOutput = true; // true to match Sphinx behaviour of excluding silence from decoder's output
		PhoneRegistry phoneReg;
		bool allowSoftHardConsonant = true;
		bool allowVowelStress = true;
		phoneReg.setPalatalSupport(PalatalSupport::AsHard);
		initPhoneRegistryUk(phoneReg, allowSoftHardConsonant, allowVowelStress);

		// map pronunciation as word (pronId) to corresponding word
		GrowOnlyPinArena<wchar_t> stringArena(10000);
		std::vector<PhoneticWord> phoneticDictKnown;
		bool loadPhoneDict;
		const char* errMsg = nullptr;
		std::wstring persianDictPathK = speechProjDir.filePath("PhoneticDict/phoneticDictUkKnown.xml").toStdWString();
		std::tie(loadPhoneDict, errMsg) = loadPhoneticDictionaryXml(persianDictPathK, phoneReg, phoneticDictKnown, stringArena);
		if (!loadPhoneDict)
		{
			std::wcerr << "Can't load phonetic dictionary " << errMsg << std::endl;
			return;
		}

		QTextCodec* pTextCodec = QTextCodec::codecForName("utf8");
		std::vector<std::string> brokenLines;
		std::vector<PhoneticWord> phoneticDictTest;
		std::tie(loadPhoneDict, errMsg) = loadPhoneticDictionaryPronIdPerLine(dictPath.toStdWString(), phoneReg, *pTextCodec, phoneticDictTest, brokenLines, stringArena);
		if (!loadPhoneDict)
		{
			std::cerr << errMsg << std::endl;
			return;
		}

		std::vector<PhoneticWord> phoneticDictFiller;
		std::tie(loadPhoneDict, errMsg) = loadPhoneticDictionaryPronIdPerLine(dictPathFiller.toStdWString(), phoneReg, *pTextCodec, phoneticDictFiller, brokenLines, stringArena);
		if (!loadPhoneDict)
		{
			std::cerr << errMsg << std::endl;
			return;
		}

		auto populatePhoneDict = [](const std::vector<PhoneticWord>& phoneticDict, std::map<boost::wstring_ref, boost::wstring_ref>& pronIdToWord, std::map<boost::wstring_ref, PronunciationFlavour>& pronIdToPronObj)
		{
			for (const PhoneticWord& phWord : phoneticDict)
			{
				for (const PronunciationFlavour& pronFlav : phWord.Pronunciations)
				{
					pronIdToPronObj.insert({ pronFlav.PronCode, pronFlav });

					// for multiple pronunciations there are cases when pronId != word
					//if (phWord.Pronunciations.size() > 1)
					pronIdToWord.insert({ pronFlav.PronCode, phWord.Word });
				}
			}
		};
		std::map<boost::wstring_ref, boost::wstring_ref> pronCodeToWordWellFormed; // used to map pronCode->word (eg slova(1)->slova)
		std::map<boost::wstring_ref, PronunciationFlavour> pronCodeToObjWellFormed;
		populatePhoneDict(phoneticDictKnown, pronCodeToWordWellFormed, pronCodeToObjWellFormed);

		//
		std::vector<boost::wstring_ref> duplicatePronCodes;

		std::map<boost::wstring_ref, PronunciationFlavour> pronCodeToObjTest;
		populatePronCodes(phoneticDictTest, pronCodeToObjTest, duplicatePronCodes);
		
		std::map<boost::wstring_ref, PronunciationFlavour> pronCodeToPronObjFiller;
		populatePronCodes(phoneticDictFiller, pronCodeToPronObjFiller, duplicatePronCodes);

		//
		std::wstring timeStampStr;
		appendTimeStampNow(timeStampStr);

		std::wstringstream dumpFileName;

		//
		dumpFileName.str(L"");
		dumpFileName << L"wordErrorDump_" << speechModelVerStr << "_" << timeStampStr << L"_logfn.txt";

		cmd_ln_t *config = SphinxConfig::pg_init_cmd_ln_t(hmmPath.toStdString(),
			langModelPath.toStdString().c_str(), 
			dictPath.toStdString().c_str(),
			true, false, true, AppHelpers::mapPathStdString(toQString(dumpFileName.str())));
		if (config == nullptr)
			return;

		//
		ps_decoder_t *ps = ps_init(config);
		if (ps == nullptr)
			return;

		//
		PhoneProximityCosts phoneCosts(phoneReg);
		EditDistance<PhoneId, PhoneProximityCosts> phonesEditDist;
		int regPhonesCount = phoneReg.phonesCount() + 1; // +1 to keep NIL phone in the first row/column
		phoneReg.assumeSequentialPhoneIdsWithoutGaps();
		std::vector<int> phoneConfusionMat(regPhonesCount*regPhonesCount, 0);

		int sentErrorTotalCount = 0;
		int wordErrorTotalCount = 0;
		int wordTotalCount = 0;
		int phoneErrorTotalCount = 0;
		int phoneTotalCount = 0;
		float targetFrameRate = CmuSphinxFrameRate;
		std::vector<TwoUtterances> recogUtterances;
		decodeSpeechSegments(segments, ps, targetFrameRate, recogUtterances,
			phoneConfusionMat, regPhonesCount, 
			sentErrorTotalCount, wordErrorTotalCount, wordTotalCount, phoneErrorTotalCount, phoneTotalCount,
			phonesEditDist, phoneCosts,
			pronCodeToWordWellFormed,
			pronCodeToObjTest, pronCodeToPronObjFiller,
			phoneReg, excludeSilFromDecOutput);

		// the call may crash if phonetic dictionary was not correctly initialized (eg .dict file is empty)
		ps_free(ps);

		// output brief statistics
		double sentErrAvg = sentErrorTotalCount / (double)segments.size();
		double wordErrAvg = wordErrorTotalCount / (double)wordTotalCount;
		double phoneErrAvg = phoneErrorTotalCount / (double)phoneTotalCount;
		std::wcout << "SentErr=" << sentErrAvg << std::endl;
		std::wcout << "WordErr=" << wordErrAvg << std::endl;
		std::wcout << "PhonErr=" << phoneErrAvg << std::endl;

		// brief info

		{
			dumpFileName.str(L"");
			dumpFileName << L"wordErrorDump_" << speechModelVerStr << "_" << timeStampStr << L"_brief.txt";

			QFile dumpFile(toQString(dumpFileName.str()));
			if (!dumpFile.open(QIODevice::WriteOnly | QIODevice::Text))
			{
				std::cerr << "Can't open ouput file" << std::endl;
				return;
			}

			QTextStream dumpFileStream(&dumpFile);
			dumpFileStream.setCodec("UTF-8");
			dumpFileStream << QString("SentErr=%1 (%2/%3)").arg(sentErrAvg, 0, 'f', 5).arg(sentErrorTotalCount).arg(segments.size()) << "\n";
			dumpFileStream << QString("WordErr=%1 (%2/%3)").arg(wordErrAvg, 0, 'f', 5).arg(wordErrorTotalCount).arg(wordTotalCount) << "\n";
			dumpFileStream << QString("PhonErr=%1 (%2/%3)").arg(phoneErrAvg, 0, 'f', 5).arg(phoneErrorTotalCount).arg(phoneTotalCount) << "\n";
		}

		// order by word error large to small
		
		std::sort(std::begin(recogUtterances), std::end(recogUtterances), [](TwoUtterances& a, TwoUtterances& b)
		{
			return a.ErrorWord > b.ErrorWord;
		});

		dumpFileName.str(L"");
		dumpFileName << L"wordErrorDump_" << speechModelVerStr <<"_" << timeStampStr << L"_byWordErr.txt";
		std::tie(op,errMsg) = dumpDecoderExpectActualResults(dumpFileName.str(), recogUtterances);
		if (!op)
		{
			std::cerr << errMsg << std::endl;
			return;
		}

		dumpFileName.str(L"");
		dumpFileName << L"wordErrorDump_" << speechModelVerStr << "_" << timeStampStr << L"_byWordErr.xml";
		std::tie(op, errMsg) = dumpDecoderExpectActualResultsXml(dumpFileName.str(), recogUtterances, speechModelVerStr);
		if (!op)
		{
			std::cerr << errMsg << std::endl;
			return;
		}

		// order by file path
		
		std::sort(std::begin(recogUtterances), std::end(recogUtterances), [](TwoUtterances& a, TwoUtterances& b)
		{
			return a.RelFilePathNoExt < b.RelFilePathNoExt;
		});

		dumpFileName.str(L"");
		dumpFileName << L"wordErrorDump_" << speechModelVerStr << "_" << timeStampStr << L"_byFile.txt";
		std::tie(op,errMsg) = dumpDecoderExpectActualResults(dumpFileName.str(), recogUtterances);
		if (!op)
		{
			std::cerr << errMsg << std::endl;
			return;
		}

		dumpFileName.str(L"");
		dumpFileName << L"wordErrorDump_" << speechModelVerStr << "_" << timeStampStr << L"_byFile.xml";
		std::tie(op, errMsg) = dumpDecoderExpectActualResultsXml(dumpFileName.str(), recogUtterances, speechModelVerStr);
		if (!op)
		{
			std::cerr << errMsg << std::endl;
			return;
		}

		// print confusion matrix
		dumpFileName.str(L"");
		dumpFileName << L"wordErrorDump_" << speechModelVerStr << "_" << timeStampStr << "_confusionMat.htm";
		dumpConfusionMatrix(phoneConfusionMat, regPhonesCount, dumpFileName.str(), phoneReg);
	}

	void run()
	{
		//recognizeWav();
		testSphincDecoder();
	}
}