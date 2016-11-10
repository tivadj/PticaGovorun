#include <vector>
#include <iostream>
#include "SpeechProcessing.h"
#include "PhoneticService.h"
#include "AppHelpers.h"
#include "WavUtils.h"

namespace SegmentsLoaderRunnerNS
{
	using namespace PticaGovorun;

	void loadSegmentsTest()
	{
		auto speechProjDir = toBfs(AppHelpers::configParamQString("speechProjDir", ""));
		auto wavRootDir = speechProjDir / "SpeechAudio";
		auto annotRootDir = speechProjDir / "SpeechAnnot";
		auto wavDirToAnalyze = speechProjDir / "SpeechAudio/ncru1-slovo/96391.flac";

		std::vector<AnnotatedSpeechSegment> segments;
		auto segPredBeforeFun = [](const AnnotatedSpeechSegment& seg) -> bool { return true; };
		ErrMsgList errMsg;
		bool removeSilenceAnnot = true;
		bool padSilStart = false;
		bool padSilEnd = false;
		bool loadAudio = true;
		bool removeInterSpeechSilence = true;
		if (!loadSpeechAndAnnotation(QFileInfo(toQStringBfs(wavDirToAnalyze)), wavRootDir.wstring(), annotRootDir.wstring(), MarkerLevelOfDetail::Word, loadAudio, removeSilenceAnnot, removeInterSpeechSilence, padSilStart, padSilEnd, 0, segPredBeforeFun, segments, &errMsg))
		{
			std::cerr << str(errMsg) << "\n";
			return;
		}
		int targId = 134;
		for (const auto& seg : segments)
		{
			if (seg.ContentMarker.Id == targId && 
				!writeAllSamplesWav(seg.Samples, 22050, "tmp_audioRegion.wav", &errMsg))
			{
				std::cerr << str(errMsg) << "\n";
				return;
			}
		}
	}

	void run()
	{
		loadSegmentsTest();
	}
}

