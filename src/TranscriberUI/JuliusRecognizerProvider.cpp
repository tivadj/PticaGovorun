#include "JuliusRecognizerProvider.h"

#ifdef PG_HAS_JULIUS
#include "AppHelpers.h"

namespace PticaGovorun
{
	bool initRecognizerConfiguration(const std::string& recogName, RecognizerSettings& rs)
	{
		if (recogName == "shrekky")
		{
			rs.FrameSize = FrameSize;
			rs.FrameShift = FrameShift;
			rs.SampleRate = SampleRate;
			rs.UseWsp = false;

			rs.DictionaryFilePath = AppHelpers::mapPathStdString("data/Julius/shrekky/shrekkyDic.voca");
			rs.LanguageModelFilePath = AppHelpers::mapPathStdString("data/Julius/shrekky/shrekkyLM.blm");
			rs.AcousticModelFilePath = AppHelpers::mapPathStdString("data/Julius/shrekky/shrekkyAM.bam");

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

			rs.DictionaryFilePath =    R"path(C:\progs\cygwin\home\mmore\voxforge_p111-117\lexicon\voxforge_lexicon)path";
			rs.LanguageModelFilePath = R"path(C:\progs\cygwin\home\mmore\voxforge_p111-117\auto\persian.ArpaLM.bi.bin)path";
			rs.AcousticModelFilePath = R"path(C:\progs\cygwin\home\mmore\voxforge_p111-117\auto\acoustic_model_files\hmmdefs)path";
			rs.TiedListFilePath =      R"path(C:\progs\cygwin\home\mmore\voxforge_p111-117\auto\acoustic_model_files\tiedlist)path";

			rs.LogFile = recogName + "-LogFile.txt";
			rs.FileListFileName = recogName + "-FileList.txt";
			rs.TempSoundFile = recogName + "-TmpAudioFile.wav";
			rs.CfgFileName = recogName + "-Config.txt";
			rs.CfgHeaderFileName = recogName + "-ConfigHeader.txt";
		}

		return true;
	}

	bool JuliusRecognizerProvider::hasError() const
	{
		return !errorString_.empty();
	}

	JuliusToolWrapper* JuliusRecognizerProvider::instance(const std::string& recogNameHint)
	{
		if (hasError())
			return nullptr;
		ensureRecognizerIsCreated(recogNameHint);
		if (hasError())
			return nullptr;
		return recognizer_.get();
	}

	void JuliusRecognizerProvider::ensureRecognizerIsCreated(const std::string& recogNameHint)
	{
		// initialize the recognizer lazily
		if (recognizer_ == nullptr)
		{
			RecognizerSettings rs;
			if (!initRecognizerConfiguration(recogNameHint, rs))
			{
				errorString_ = "Can't find speech recognizer configuration";
				return;
			}

			QTextCodec* pTextCodec = QTextCodec::codecForName(PGEncodingStr);
			auto textCodec = std::unique_ptr<QTextCodec, NoDeleteFunctor<QTextCodec>>(pTextCodec);

			std::tuple<bool, std::string, std::unique_ptr<JuliusToolWrapper>> newRecogOp =
				createJuliusRecognizer(rs, std::move(textCodec));
			if (!std::get<0>(newRecogOp))
			{
				errorString_ = std::get<1>(newRecogOp);
				return;
			}

			recognizer_ = std::move(std::get<2>(newRecogOp));
		}
	}
}
#endif