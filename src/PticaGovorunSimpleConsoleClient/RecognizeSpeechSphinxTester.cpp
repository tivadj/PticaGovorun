#include "stdafx.h"
#include <vector>
#include <array>
#include <iostream>
#include <locale>

#include <windows.h>

#include <pocketsphinx.h>
#include <QTextCodec>
#include <QDebug>

#include "ps_alignment.h"
#include "state_align_search.h"

#include <samplerate.h>

#include "WavUtils.h" // readAllSamples
#include "SpeechProcessing.h"
#include "StringUtils.h" // findEditDistance
#include "CoreUtils.h" // timeStamp
#include "ClnUtils.h"
#include "PhoneticService.h"

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
		cmd_ln_t *config = cmd_ln_init(nullptr, ps_args(), true,
			"-hmm", hmmPath,
			"-lm", langModelPath,
			"-dict", dictPath,
			nullptr);
		if (config == nullptr)
			return;

		ps_decoder_t *ps = ps_init(config);
		if (ps == nullptr)
			return;

		int rv = ps_start_utt(ps, "goforward");
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
		__int32 score;
		const char* uttid;
		const char* hyp = ps_get_hyp(ps, &score, &uttid);
		if (hyp == nullptr)
		{
			std::cout << "No hypothesis is available" << std::endl;
			return;
		}

		QTextCodec* textCodec = QTextCodec::codecForName("utf8");
		std::wstring hypWStr = textCodec->toUnicode(hyp).toStdWString();

		printf("Recognized: %s\n", hyp);
		std::cout << "Recognized: " << hyp << std::endl;
		std::wcout << "Recognized: " << hypWStr << std::endl;

		// print word segmentation
		ps_seg_t *seg;
		for (seg = ps_seg_iter(ps, &score); seg; seg = ps_seg_next(seg))
		{
			const char* word = ps_seg_word(seg);
			std::wstring wordWStr = textCodec->toUnicode(word).toStdWString();
			
			int sf, ef;
			ps_seg_frames(seg, &sf, &ef);

			int32 lscr, ascr, lback;
			int32 post = ps_seg_prob(seg, &ascr, &lscr, &lback); // posterior probability?

			float64 prob = logmath_exp(ps_get_logmath(ps), post);
			std::wcout << wordWStr.c_str();
			std::cout 
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
		inline CostType getInsertSymbolCost(Letter x) {
			return 1;
		}
		inline CostType getRemoveSymbolCost(Letter x) {
			return 1;
		}
		inline CostType getSubstituteSymbolCost(Letter x, Letter y) {
			return x == y ? 0 : 1;
		}
	};
	
	struct SphinxFileIdLine
	{
		std::wstring Line;
		std::wstring OriginalFileNamePart;
		int StartMarkerId;
		int EndMarkerId;
	};

	bool readFileId(const wchar_t* fileIdPath, std::vector<SphinxFileIdLine>& fileIdLines)
	{
		QFile file(QString::fromWCharArray(fileIdPath));
		if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
			return false;
		
		QTextStream stream(&file);
		while (!stream.atEnd())
		{
			QString line = stream.readLine();
			int slashInd = line.lastIndexOf(QChar('/'));
			PG_Assert(slashInd != -1);

			int underscoreInd = line.indexOf(QChar('_'), slashInd);
			PG_Assert(underscoreInd != -1);
			int dashInd = line.indexOf(QChar('-'), underscoreInd);
			PG_Assert(dashInd != -1);

			QString origSpeechFileNamePart = line.mid(slashInd + 1, underscoreInd - slashInd - 1);
			QString startMarkerIdStr = line.mid(underscoreInd + 1, dashInd - underscoreInd - 1);
			QString endMarkerIdStr = line.mid(dashInd + 1);

			SphinxFileIdLine part;
			part.Line = line.toStdWString();
			part.OriginalFileNamePart = origSpeechFileNamePart.toStdWString();
			part.StartMarkerId = startMarkerIdStr.toInt();
			part.EndMarkerId = endMarkerIdStr.toInt();
			fileIdLines.push_back(part);
		}
		return true;
	}

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
		inline int getInsertSymbolCost(wchar_t x) {
			return 1;
		}
		inline int getRemoveSymbolCost(wchar_t x) {
			return 1;
		}
		inline int getSubstituteSymbolCost(wchar_t x, wchar_t y)
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
	public:
		typedef float CostType;
		static CostType getZeroCosts() {
			return 0;
		}
		inline CostType getInsertSymbolCost(UkrainianPhoneId x) {
			return 3;
		}
		inline CostType getRemoveSymbolCost(UkrainianPhoneId x) {
			return 3;
		}
		inline CostType getSubstituteSymbolCost(UkrainianPhoneId x, UkrainianPhoneId y)
		{
			if (x == y) return 0;
			
			const CostType SimilarSound = 1;
			if (setTwoEq(x, y, UkrainianPhoneId::P_B, UkrainianPhoneId::P_P))
				return SimilarSound;
			if (setTwoEq(x, y, UkrainianPhoneId::P_V, UkrainianPhoneId::P_F))
				return SimilarSound;
			if (setTwoEq(x, y, UkrainianPhoneId::P_H, UkrainianPhoneId::P_KH))
				return SimilarSound;
			if (setTwoEq(x, y, UkrainianPhoneId::P_H, UkrainianPhoneId::P_G))
				return SimilarSound;
			if (setTwoEq(x, y, UkrainianPhoneId::P_D, UkrainianPhoneId::P_T))
				return SimilarSound;
			if (setTwoEq(x, y, UkrainianPhoneId::P_Z, UkrainianPhoneId::P_S))
				return SimilarSound;
			if (setTwoEq(x, y, UkrainianPhoneId::P_ZH, UkrainianPhoneId::P_SH))
				return SimilarSound;

			// DZ DZH?

			boost::optional<CharGroup> xClass = classifyPhoneUk((int)x);
			boost::optional<CharGroup> yClass = classifyPhoneUk((int)y);
			if (xClass && xClass == yClass) // chars from the same group
				return 2;
			return 99; // chars from different groups
		}
	};

	// Result of comparing expected text with decoded speech.
	struct TwoUtterances
	{
		float ErrorWord; // edit distance between utterances as a sequence of words
		float ErrorChars; // edit distance between utterances as a sequence of chars
		AnnotatedSpeechSegment Segment;
		std::wstring TextActual;
		std::vector<std::wstring> PronIdsExpected; // слова(2)
		std::vector<std::wstring> WordsExpected; // слова without any braces
		std::vector<std::wstring> WordsActual;
		std::vector<std::wstring> WordsActualMerged;
		std::vector<float> WordProbs; // confidence of each recognized word
		std::vector<float> WordProbsMerged;
		std::vector<UkrainianPhoneId> PhonesExpected;
		std::vector<UkrainianPhoneId> PhonesActual;
		std::string PhonesExpectedAlignedStr;
		std::string PhonesActualAlignedStr;
	};

	void decodeSpeechSegments(const std::vector<AnnotatedSpeechSegment>& segments, ps_decoder_t *ps, float targetFrameRate, std::vector<TwoUtterances>& recogUtterances,
		std::vector<int>& phoneConfusionMat, int phonesCount,
		int& wordErrorTotalCount, int& wordTotalCount,
		EditDistance<UkrainianPhoneId, PhoneProximityCosts>& phonesEditDist,
		const std::unordered_map<std::wstring, std::wstring>& pronIdToWord,
		const std::unordered_map<std::wstring, PronunciationFlavour>& pronIdToPronObj)
	{
		for (int i = 0; i < segments.size(); ++i)
		{
			const AnnotatedSpeechSegment& seg = segments[i];

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
			}

			int rv = ps_start_utt(ps, "goforward");
			if (rv < 0)
			{
				std::cerr << "Error: Can't start utterance";
				break;
			}

			bool fullUtterance = true;
			rv = ps_process_raw(ps, speechFramesResamp.data(), speechFramesResamp.size(), false, fullUtterance);
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
			__int32 score;
			const char* uttid;
			const char* hyp = ps_get_hyp(ps, &score, &uttid);
			if (hyp == nullptr)
			{
				// "No hypothesis is available"
				hyp = "";
			}

			QTextCodec* textCodec = QTextCodec::codecForName("utf8");
			std::wstring hypWStr = textCodec->toUnicode(hyp).toStdWString();

			// find words and word probabilities
			std::vector<std::wstring> wordsActual;
			std::vector<float> wordsActualProbs;
			for (ps_seg_t *recogSeg = ps_seg_iter(ps, &score); recogSeg; recogSeg = ps_seg_next(recogSeg))
			{
				const char* word = ps_seg_word(recogSeg);
				std::wstring wordWStr = textCodec->toUnicode(word).toStdWString();
				wordsActual.push_back(wordWStr);

				int startFrame, endFrame;
				ps_seg_frames(recogSeg, &startFrame, &endFrame);

				int32 lscr, ascr, lback;
				int32 post = ps_seg_prob(recogSeg, &ascr, &lscr, &lback); // posterior probability?

				float64 prob = logmath_exp(ps_get_logmath(ps), post);
				wordsActualProbs.push_back((float)prob);
			}

			// merge word parts

			std::vector<std::wstring> pronIdsActualMerged;
			std::vector<float> pronIdsActualProbsMerged;
			if (!wordsActual.empty())
			{
				pronIdsActualMerged.push_back(wordsActual.front());
				pronIdsActualProbsMerged.push_back(wordsActualProbs.front());
			}
			for (size_t wordInd = 1; wordInd < wordsActual.size(); ++wordInd)
			{
				const std::wstring& prev = pronIdsActualMerged.back();
				const std::wstring& cur = wordsActual[wordInd];
				float prevProb = pronIdsActualProbsMerged.back();
				float curProb = wordsActualProbs[wordInd];
				if (prev.back() == L'~' && cur.front() == L'~')
				{
					std::wstring merged = prev;
					merged.pop_back();
					merged.append(cur.data() + 1, cur.size() - 1);

					pronIdsActualMerged.back() = merged;
					pronIdsActualProbsMerged.back() = prevProb * curProb;
				}
				else
				{
					pronIdsActualMerged.push_back(cur);
					pronIdsActualProbsMerged.push_back(curProb);
				}
			}

			// split text into words

			std::vector<wv::slice<const wchar_t>> pronIdSlicesExpected;
			splitUtteranceIntoWords(seg.TranscriptText, pronIdSlicesExpected);

			auto textSliceToString = [](wv::slice<const wchar_t> pronSlice) -> std::wstring
			{
				std::wstring pronId;
				pronId.assign(pronSlice.begin(), pronSlice.end());
				return pronId;
			};

			std::vector<std::wstring> pronIdsExpected;
			std::transform(std::begin(pronIdSlicesExpected), std::end(pronIdSlicesExpected), std::back_inserter(pronIdsExpected), textSliceToString);

			// map pronId -> word (eg. слова(2) -> слова)
			std::vector<std::wstring> wordsExpected;
			std::transform(pronIdsExpected.begin(), pronIdsExpected.end(), std::back_inserter(wordsExpected), [&pronIdToWord](const std::wstring& pronId)
			{
				auto wordIt = pronIdToWord.find(pronId);
				if (wordIt != pronIdToWord.end())
				{
					const std::wstring& word = wordIt->second;
					return word;
				}
				return pronId;
			});

			// skip silences for word-error computation
			std::vector<std::wstring> wordsActualNoSil;
			std::copy_if(std::begin(pronIdsActualMerged), std::end(pronIdsActualMerged), std::back_inserter(wordsActualNoSil), [](std::wstring& word)
			{
				return word != std::wstring(L"<s>") && word != std::wstring(L"</s>") && word != std::wstring(L"<sil>");
			});

			// compute word error
			WordErrorCosts<std::wstring> c;
			float distWord = findEditDistance(wordsExpected.begin(), wordsExpected.end(), wordsActualNoSil.begin(), wordsActualNoSil.end(), c);

			// compute edit distances between whole utterances
			std::wstringstream strBuff;
			PticaGovorun::join(wordsExpected.begin(), wordsExpected.end(), std::wstring(L" "), strBuff);
			std::wstring wordsExpectedStr = strBuff.str();
			strBuff.str(L"");
			PticaGovorun::join(wordsActualNoSil.begin(), wordsActualNoSil.end(), std::wstring(L" "), strBuff);
			std::wstring wordsActualStr = strBuff.str();

			WordErrorCosts<wchar_t> charCost;
			float distChar = findEditDistance(std::cbegin(wordsExpectedStr), std::cend(wordsExpectedStr), std::cbegin(wordsActualStr), std::cend(wordsActualStr), charCost);

			// do phonetic expansion
			std::vector<UkrainianPhoneId> expectedPhones;
			for (const std::wstring& pronId : pronIdsExpected)
			{
				auto pronIdIt = pronIdToPronObj.find(pronId);
				PG_Assert(pronIdIt != pronIdToPronObj.end() && "The pronId used in transcription must exist in manual phonetic dictionary");
				const PronunciationFlavour& pronObj = pronIdIt->second;
				std::copy(pronObj.PhoneIds.begin(), pronObj.PhoneIds.end(), std::back_inserter(expectedPhones));
			}

			std::vector<UkrainianPhoneId> actualPhones;
			std::vector<UkrainianPhoneId> phonesTmp;
			for (const std::wstring& actualWord : wordsActual)
			{
				bool spellOp;
				const char* msg;
				phonesTmp.clear();
				std::tie(spellOp, msg) = spellWord(actualWord, phonesTmp);
				std::copy(phonesTmp.begin(), phonesTmp.end(), std::back_inserter(actualPhones));
			}
			phonesEditDist.estimateAllDistances(expectedPhones, actualPhones, PhoneProximityCosts());
			std::vector<EditStep> phoneEditRecipe;
			phonesEditDist.minCostRecipe(phoneEditRecipe);

			// update confusion matrix
			for (const EditStep& step : phoneEditRecipe)
			{
				UkrainianPhoneId phoneExpect;
				UkrainianPhoneId phoneActual;
				if (step.Change == StringEditOp::RemoveOp)
				{
					phoneExpect = expectedPhones[step.Remove.FirstRemoveInd];
					phoneActual = UkrainianPhoneId::Nil;
				}
				else if (step.Change == StringEditOp::InsertOp)
				{
					phoneExpect = UkrainianPhoneId::Nil;
					phoneActual = actualPhones[step.Insert.SecondSourceInd];
				}
				else if (step.Change == StringEditOp::SubstituteOp)
				{
					phoneExpect = expectedPhones[step.Substitute.FirstInd];
					phoneActual = actualPhones[step.Substitute.SecondInd];
				}
				int row = (int)phoneExpect;
				int col = (int)phoneActual;
				phoneConfusionMat[col + phonesCount*row]++;
			}

			std::function<void(UkrainianPhoneId, std::vector<char>&)> ph2StrFun = [](UkrainianPhoneId ph, std::vector<char>& phVec)
			{
				std::string phStr;
				bool toStrOp = phoneToStr(ph, phStr);
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
			utter.ErrorWord = distWord;
			utter.ErrorChars = distChar;
			utter.Segment = seg;
			utter.TextActual = hypWStr;
			utter.PronIdsExpected = pronIdsExpected;
			utter.WordsExpected = wordsExpected;
			utter.WordsActual = wordsActual;
			utter.WordsActualMerged = pronIdsActualMerged;
			utter.WordProbs = wordsActualProbs;
			utter.WordProbsMerged = pronIdsActualProbsMerged;
			utter.PhonesExpected = expectedPhones;
			utter.PhonesActual = actualPhones;
			utter.PhonesExpectedAlignedStr = expectedPhonesStr;
			utter.PhonesActualAlignedStr = actualPhonesStr;
			recogUtterances.push_back(utter);

			wordErrorTotalCount += (int)distWord;
			wordTotalCount += (int)wordsExpected.size();
		}
	}

	void dumpConfusionMatrix(const std::vector<int>& phoneConfusionMat, int phonesCount, const std::string& outFilePath)
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
		

		QFile dumpFile(outFilePath.c_str());
		if (!dumpFile.open(QIODevice::WriteOnly | QIODevice::Text))
		{
			std::cerr << "Can't open ouput file" << std::endl;
			return;
		}

		QTextStream dumpFileStream(&dumpFile);
		dumpFileStream.setCodec("UTF-8");

		auto ph2StrEx = [](UkrainianPhoneId phId, std::string& phStr) -> bool
		{
			if (phId == UkrainianPhoneId::Nil)
			{
				phStr = "nil";
				return true;
			}
			return phoneToStr(phId, phStr);
		};
		{
			dumpFileStream << "<table>";
			dumpFileStream << "<th>Conf</th>"; // top-left corner cell header

			// print header
			std::string phStr;
			for (int col = 0; col < phonesCount; ++col)
			{
				UkrainianPhoneId ph2 = (UkrainianPhoneId)col;
				bool phOp = ph2StrEx(ph2, phStr);
				PG_Assert(phOp);
				dumpFileStream <<"<th>" << QString::fromStdString(phStr) << "</th>";
			}

			// print content
			for (int row = 0; row < phonesCount; ++row)
			{
				dumpFileStream << "<tr>";

				// print row header
				UkrainianPhoneId ph1 = (UkrainianPhoneId)row;
				bool phOp = ph2StrEx(ph1, phStr);
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

	void testSphincDecoder()
	{
		//
		const wchar_t* testFileIdPath = LR"path(C:\devb\PticaGovorunProj\data\TrainSphinx\persian\etc\persian_test.fileids)path";
		std::vector<SphinxFileIdLine> fileIdLines;
		readFileId(testFileIdPath, fileIdLines);

		// map pronunciation as word (pronId) to corresponding word
		std::vector<PhoneticWord> phoneticDict;
		bool loadPhoneDict;
		const char* errMsg = nullptr;
		const wchar_t* persianDictPathK = LR"path(C:\devb\PticaGovorunProj\data\TrainSphinx\SpeechModels\phoneticKnown.yml)path";
		std::tie(loadPhoneDict, errMsg) = loadPhoneticDictionaryYaml(persianDictPathK, phoneticDict);
		if (!loadPhoneDict)
		{
			std::cerr << "Can't load phonetic dictionary " << errMsg << std::endl;
			return;
		}
		const wchar_t* persianDictPathB = LR"path(C:\devb\PticaGovorunProj\data\TrainSphinx\SpeechModels\phoneticBroken.yml)path";
		std::tie(loadPhoneDict, errMsg) = loadPhoneticDictionaryYaml(persianDictPathB, phoneticDict);
		if (!loadPhoneDict)
		{
			std::cerr << "Can't load phonetic dictionary " << errMsg << std::endl;
			return;
		}

		std::unordered_map<std::wstring, std::wstring> pronIdToWord;
		std::unordered_map<std::wstring, PronunciationFlavour> pronIdToPronObj;
		for (const PhoneticWord& phWord : phoneticDict)
		{
			for (const PronunciationFlavour& pronFlav : phWord.Pronunciations)
			{
				pronIdToPronObj.insert({ pronFlav.PronAsWord, pronFlav });

				if (phWord.Pronunciations.size() > 1)
					pronIdToWord.insert({ pronFlav.PronAsWord, phWord.Word });
			}
		}

		//
		const wchar_t* wavRootDir       = LR"path(C:\devb\PticaGovorunProj\srcrep\data\SpeechAudio\)path";
		const wchar_t* annotRootDir     = LR"path(C:\devb\PticaGovorunProj\srcrep\data\SpeechAnnot\)path";
		const wchar_t* wavDirToAnalyze  = LR"path(C:\devb\PticaGovorunProj\srcrep\data\SpeechAudio\)path";

		std::vector<AnnotatedSpeechSegment> segments;
		auto segPredBeforeFun = [](const AnnotatedSpeechSegment& seg) -> bool
		{
			if (seg.FilePath.find(L"pynzenykvm") != std::wstring::npos)
				return false;
			if (seg.Language != SpeechLanguage::Ukrainian)
				return false;
			return true;
		};
		bool loadOp;
		std::tie(loadOp, errMsg) = loadSpeechAndAnnotation(QFileInfo(QString::fromWCharArray(wavDirToAnalyze)), wavRootDir, annotRootDir, MarkerLevelOfDetail::Word, true, segPredBeforeFun, segments);

		// process only test speech segments

		std::remove_if(segments.begin(), segments.end(), [&fileIdLines](const AnnotatedSpeechSegment& seg)
		{
			auto fileIdIt = std::find_if(fileIdLines.begin(), fileIdLines.end(), [&seg](const SphinxFileIdLine& fileIdLine)
			{
				return seg.FilePath.find(fileIdLine.OriginalFileNamePart) != std::wstring::npos &&
					seg.StartMarkerId == fileIdLine.StartMarkerId &&
					seg.EndMarkerId == fileIdLine.EndMarkerId;
			});
			bool isTestSeg = fileIdIt != fileIdLines.end();
			return !isTestSeg;
		});
		
		int trunc = 51;
		if (trunc != -1 && segments.size() > trunc)
			segments.resize(trunc);

		//
		const char* hmmPath       = R"path(C:/devb/PticaGovorunProj/data/TrainSphinx/persian/model_parameters/persian.cd_cont_200/)path";
		//const char* langModelPath = R"path(C:/devb/PticaGovorunProj/data/TrainSphinx/persian/etc/persian.lm.DMP)path";
		//const char* dictPath      = R"path(C:/devb/PticaGovorunProj/data/TrainSphinx/persian/etc/persian.dic)path";
		//const char* langModelPath = R"path(C:\devb\PticaGovorunProj\srcrep\build\x64\Release\persian.lm.DMP)path";
		const char* langModelPath = R"path(C:\devb\PticaGovorunProj\srcrep\build\x64\Release\persianLM.txt)path";
		const char* dictPath = R"path(C:\devb\PticaGovorunProj\srcrep\build\x64\Release\persianDic.txt)path";
		cmd_ln_t *config = cmd_ln_init(nullptr, ps_args(), true,
			"-hmm", hmmPath,
			"-lm", langModelPath, // accepts both TXT and DMP formats
			"-dict", dictPath,
			nullptr);
		if (config == nullptr)
			return;

		ps_decoder_t *ps = ps_init(config);
		if (ps == nullptr)
			return;

		//
		PhoneProximityCosts phoneCosts;
		EditDistance<UkrainianPhoneId, PhoneProximityCosts> phonesEditDist;
		const static int PhonesCount = (int)UkrainianPhoneId::P_LAST;
		std::vector<int> phoneConfusionMat(PhonesCount*PhonesCount, 0);

		int wordErrorTotalCount = 0;
		int wordTotalCount = 0;
		float targetFrameRate = CmuSphinxFrameRate;
		std::vector<TwoUtterances> recogUtterances;
		decodeSpeechSegments(segments, ps, targetFrameRate, recogUtterances,
			phoneConfusionMat, PhonesCount, wordErrorTotalCount, wordTotalCount, phonesEditDist, pronIdToWord, pronIdToPronObj);

		// the call may crash if phonetic dictionary was not correctly initialized (eg .dict file is empty)
		ps_free(ps);

		// order error, large to small
		std::sort(std::begin(recogUtterances), std::end(recogUtterances), [](TwoUtterances& a, TwoUtterances& b)
		{
			return a.ErrorWord > b.ErrorWord;
		});

		std::string timeStampStr;
		appendTimeStampNow(timeStampStr);

		std::stringstream dumpFileName;
		dumpFileName << "wordErrorDump." << timeStampStr << ".txt";

		QFile dumpFile(dumpFileName.str().c_str());
		if (!dumpFile.open(QIODevice::WriteOnly | QIODevice::Text))
		{
			std::cerr << "Can't open ouput file" <<std::endl;
			return;
		}

		QTextStream dumpFileStream(&dumpFile);
		dumpFileStream.setCodec("UTF-8");
		
		double wordErrorAvg = wordErrorTotalCount / (double)wordTotalCount;
		dumpFileStream << "WordErrorAvg=" << wordErrorAvg << " UtterCount=" << segments.size() <<"\n";

		const std::wstring separ(L" ");
		std::wstringstream buff;
		CharPhonationGroupCosts c;
		EditDistance<wchar_t, CharPhonationGroupCosts> editDist;
		for (int i = 0; i < recogUtterances.size(); ++i)
		{
			const TwoUtterances& utter = recogUtterances[i];
			if (utter.ErrorWord == 0) // skip correct utterances
				break;
			dumpFileStream << "WordError=" << utter.ErrorWord << "/" <<utter.WordsExpected.size() << " " << "CharError=" << utter.ErrorChars << " SegId=" << utter.Segment.SegmentId << " " << QString::fromStdWString(utter.Segment.FilePath) <<"\n";

			dumpFileStream << "ExpectProns=" << QString::fromStdWString(utter.Segment.TranscriptText) << "\n";

			buff.str(L"");
			PticaGovorun::join(utter.WordsActual.cbegin(), utter.WordsActual.cend(), separ, buff);
			dumpFileStream << "ActualRaw=" << QString::fromStdWString(buff.str()) << "\n";

			dumpFileStream << "WordProbs=";
			for (int wordInd = 0; wordInd < utter.WordsActual.size(); ++wordInd)
			{
				dumpFileStream << QString::fromStdWString(utter.WordsActual[wordInd]) << " ";
				dumpFileStream << QString("%1").arg(utter.WordProbs[wordInd], 0, 'f', 2) << " ";
			}
			dumpFileStream << "\n"; // end after last prob

			buff.str(L"");
			PticaGovorun::join(utter.WordsExpected.cbegin(), utter.WordsExpected.cend(), separ, buff);
			std::wstring expectWords = buff.str();
			//dumpFileStream << "ExpectWords=" << QString::fromStdWString(expectWords) << "\n";

			buff.str(L"");
			PticaGovorun::join(utter.WordsActualMerged.cbegin(), utter.WordsActualMerged.cend(), separ, buff);
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

		dumpFileName.str("");
		dumpFileName << "wordErrorDump." << timeStampStr << ".ConfusionMat.htm";

		dumpConfusionMatrix(phoneConfusionMat, PhonesCount, dumpFileName.str());
		dumpFileStream << "\n"; // line after confusion matrix		
	}

	void run()
	{
		//recognizeWav();
		testSphincDecoder();
	}
}