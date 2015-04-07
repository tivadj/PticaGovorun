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

	void testSphincDecoder()
	{
		const wchar_t* testFileIdPath = LR"path(C:\devb\PticaGovorunProj\data\TrainSphinx\persian\etc\persian_test.fileids)path";
		std::vector<SphinxFileIdLine> fileIdLines;
		readFileId(testFileIdPath, fileIdLines);

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
		const char* errMsg;
		std::tie(loadOp, errMsg) = loadSpeechAndAnnotation(QFileInfo(QString::fromWCharArray(wavDirToAnalyze)), wavRootDir, annotRootDir, MarkerLevelOfDetail::Word, true, segPredBeforeFun, segments);

		//
		const char* hmmPath       = R"path(C:/devb/PticaGovorunProj/data/TrainSphinx/persian/model_parameters/persian.cd_cont_200/)path";
		//const char* langModelPath = R"path(C:/devb/PticaGovorunProj/data/TrainSphinx/persian/etc/persian.lm.DMP)path";
		//const char* dictPath      = R"path(C:/devb/PticaGovorunProj/data/TrainSphinx/persian/etc/persian.dic)path";
		const char* langModelPath = R"path(C:\devb\PticaGovorunProj\srcrep\build\x64\Release\persian.lm.DMP)path";
		const char* dictPath = R"path(C:\devb\PticaGovorunProj\srcrep\build\x64\Release\persianDic.txt)path";
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

		//
		struct TwoUtterances
		{
			float ErrorWord; // edit distance between utterances as a sequence of words
			float ErrorChars; // edit distance between utterances as a sequence of chars
			AnnotatedSpeechSegment Segment;
			std::wstring TextActual;
			std::vector<std::wstring> WordsExpected;
			std::vector<std::wstring> WordsActual;
			std::vector<std::wstring> WordsActualMerged;
			std::vector<float> WordProbs; // confidence of each recognized word
			std::vector<float> WordProbsMerged;
		};

		//
		std::vector<TwoUtterances> recogUtterances;
		float targetFrameRate = CmuSphinxFrameRate;
		for (int i = 0; i < segments.size(); ++i)
		{
			//if (i > 90)
			//	break;
			const AnnotatedSpeechSegment& seg = segments[i];
			
			// process only test speech segments

			auto fileIdIt = std::find_if(fileIdLines.begin(), fileIdLines.end(), [&seg](const SphinxFileIdLine& fileIdLine)
			{
				return seg.FilePath.find(fileIdLine.OriginalFileNamePart) != std::wstring::npos &&
					seg.StartMarkerId == fileIdLine.StartMarkerId &&
					seg.EndMarkerId == fileIdLine.EndMarkerId;
			});
			bool isTestSeg = fileIdIt != fileIdLines.end();
			if (!isTestSeg)
				continue;

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

			std::vector<std::wstring> wordsActualMerged;
			std::vector<float> wordsActualProbsMerged;
			if (!wordsActual.empty())
			{
				wordsActualMerged.push_back(wordsActual.front());
				wordsActualProbsMerged.push_back(wordsActualProbs.front());
			}
			for (size_t wordInd = 1; wordInd < wordsActual.size(); ++wordInd)
			{
				const std::wstring& prev = wordsActualMerged.back();
				const std::wstring& cur  = wordsActual[wordInd];
				float prevProb = wordsActualProbsMerged.back();
				float curProb = wordsActualProbs[wordInd];
				if (prev.back() == L'~' && cur.front() == L'~')
				{
					std::wstring merged = prev;
					merged.pop_back();
					merged.append(cur.data() + 1, cur.size() - 1);

					wordsActualMerged.back() = merged;
					wordsActualProbsMerged.back() = prevProb * curProb;
				}
				else
				{
					wordsActualMerged.push_back(cur);
					wordsActualProbsMerged.push_back(curProb);
				}
			}

			// split text into words

			std::vector<wv::slice<const wchar_t>> wordSlicesExpected;
			splitUtteranceIntoWords(seg.TranscriptText, wordSlicesExpected);
			
			auto textSliceToString = [](wv::slice<const wchar_t> wordSlice) -> std::wstring
			{
				std::wstring word;
				word.assign(wordSlice.begin(), wordSlice.end());
				return word;
			};
			
			std::vector<std::wstring> wordsExpected;
			std::transform(std::begin(wordSlicesExpected), std::end(wordSlicesExpected), std::back_inserter(wordsExpected), textSliceToString);

			// skip silences for word-error computation
			std::vector<std::wstring> wordsActualNoSil;
			std::copy_if(std::begin(wordsActualMerged), std::end(wordsActualMerged), std::back_inserter(wordsActualNoSil), [](std::wstring& word)
			{
				return word != std::wstring(L"<s>") && word != std::wstring(L"</s>") && word != std::wstring(L"<sil>");
			});


			// compute word error
			WordErrorCosts<std::wstring> c;
			float distWord = findEditDistance(wordsExpected.begin(), wordsExpected.end(), wordsActualNoSil.begin(), wordsActualNoSil.end(), c);

			// compute edit distances between whole utterances
			WordErrorCosts<wchar_t> charCost;
			float distChar = findEditDistance(std::cbegin(hypWStr), std::cend(hypWStr), std::cbegin(seg.TranscriptText), std::cend(seg.TranscriptText), charCost);

			TwoUtterances utter;
			utter.ErrorWord = distWord;
			utter.ErrorChars = distChar;
			utter.Segment = seg;
			utter.TextActual = hypWStr;
			utter.WordsExpected = wordsExpected;
			utter.WordsActual = wordsActual;
			utter.WordsActualMerged = wordsActualMerged;
			utter.WordProbs = wordsActualProbs;
			utter.WordProbsMerged = wordsActualProbsMerged;
			recogUtterances.push_back(utter);
		}

		// the call may crash if phonetic dictionary was not correctly initialized (eg .dict file is empty)
		ps_free(ps);

		// order error, large to small
		std::sort(std::begin(recogUtterances), std::end(recogUtterances), [](TwoUtterances& a, TwoUtterances& b)
		{
			return a.ErrorWord > b.ErrorWord;
		});


		std::stringstream dumpFileName;
		dumpFileName << "wordErrorDump.";
		appendTimeStampNow(dumpFileName);
		dumpFileName << ".txt";

		QFile dumpFile(dumpFileName.str().c_str());
		if (!dumpFile.open(QIODevice::WriteOnly | QIODevice::Text))
		{
			std::cerr << "Can't open ouput file" <<std::endl;
			return;
		}

		QTextStream dumpFileStream(&dumpFile);
		dumpFileStream.setCodec("UTF-8");
		std::wstringstream buff;
		for (int i = 0; i < recogUtterances.size(); ++i)
		{
			const TwoUtterances& utter = recogUtterances[i];
			if (utter.ErrorWord == 0) // skip correct utterances
				break;
			dumpFileStream << "WordError=" << utter.ErrorWord << " " << "CharError=" << utter.ErrorChars << " SegId=" << utter.Segment.SegmentId << " " << QString::fromStdWString(utter.Segment.FilePath) <<"\n";
			dumpFileStream << "Expect=" << QString::fromStdWString(utter.Segment.TranscriptText) << "\n";

			const std::wstring separ(L" ");
			buff.str(L"");
			PticaGovorun::join(utter.WordsActualMerged.cbegin(), utter.WordsActualMerged.cend(), separ, buff);
			dumpFileStream << "Actual=" << QString::fromStdWString(buff.str()) << "\n";

			buff.str(L"");
			PticaGovorun::join(utter.WordsActual.cbegin(), utter.WordsActual.cend(), separ, buff);
			dumpFileStream << "ActualRaw=" << QString::fromStdWString(buff.str()) << "\n";
			
			dumpFileStream << "WordProbs=";
			for (int wordInd = 0; wordInd < utter.WordsActual.size(); ++wordInd)
			{
				dumpFileStream << QString::fromStdWString(utter.WordsActual[wordInd]) << " ";
				dumpFileStream << QString("%1").arg(utter.WordProbs[wordInd], 0, 'f', 2) << " ";
			}
			dumpFileStream << "\n"; // end of last line

			dumpFileStream << "\n"; // line between utterances
		}
	}

	void run()
	{
		//recognizeWav();
		testSphincDecoder();
	}
}