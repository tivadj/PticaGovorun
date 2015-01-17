#include "stdafx.h"
#include <vector>
#include <array>
#include <iostream>
#include <locale>
#include <windows.h>
#include <pocketsphinx.h>
#include "WavUtils.h" // readAllSamples
#include <QTextCodec>

#include "ps_alignment.h"
#include "state_align_search.h"

namespace RecognizeSpeechSphinxTester
{
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
		auto readOp = PticaGovorun::readAllSamples(wavFilePath, audioSamples);
		if (!std::get<0>(readOp))
		{
			std::cerr << "Can't read wav file" << std::endl;
			return;
		}

		const char* hmmPath = R"path(C:/devb/PticaGovorunProj/data/CMUSphinxTutorial/persian/model_parameters/persian.cd_cont_200/)path";
		const char* langModelPath = R"path(C:/devb/PticaGovorunProj/data/CMUSphinxTutorial/persian/etc/persian.lm.DMP)path";
		const char* dictPath = R"path(C:/devb/PticaGovorunProj/data/CMUSphinxTutorial/persian/etc/persian.dic)path";
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

	void run()
	{
		recognizeWav();
	}
}