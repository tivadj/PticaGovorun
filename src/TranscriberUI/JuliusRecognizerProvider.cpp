#include "JuliusRecognizerProvider.h"

#ifdef PG_HAS_JULIUS
#include "AppHelpers.h"

namespace PticaGovorun
{
	bool initRecognizerConfiguration(boost::string_view recogName, RecognizerSettings& rs)
	{
		if (recogName == "shrekky")
		{
			rs.FrameSize = FrameSize;
			rs.FrameShift = FrameShift;
			rs.SampleRate = SampleRate;
			rs.UseWsp = false;

			rs.DictionaryFilePath = AppHelpers::mapPathStdString("pgdata/Julius/shrekky/shrekkyDic.voca");
			rs.LanguageModelFilePath = AppHelpers::mapPathStdString("pgdata/Julius/shrekky/shrekkyLM.blm");
			rs.AcousticModelFilePath = AppHelpers::mapPathStdString("pgdata/Julius/shrekky/shrekkyAM.bam");

			rs.LogFile = toStdString(recogName) + "-LogFile.txt";
			rs.FileListFileName = toStdString(recogName) + "-FileList.txt";
			rs.TempSoundFile = toStdString(recogName) + "-TmpAudioFile.wav";
			rs.CfgFileName = toStdString(recogName) + "-Config.txt";
			rs.CfgHeaderFileName = toStdString(recogName) + "-ConfigHeader.txt";
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

			rs.LogFile = toStdString(recogName) + "-LogFile.txt";
			rs.FileListFileName = toStdString(recogName) + "-FileList.txt";
			rs.TempSoundFile = toStdString(recogName) + "-TmpAudioFile.wav";
			rs.CfgFileName = toStdString(recogName) + "-Config.txt";
			rs.CfgHeaderFileName = toStdString(recogName) + "-ConfigHeader.txt";
		}

		return true;
	}

	bool JuliusRecognizerProvider::hasError() const
	{
		return !errorString_.empty();
	}

	JuliusToolWrapper* JuliusRecognizerProvider::instance(boost::string_view recogNameHint)
	{
		if (hasError())
			return nullptr;
		ensureRecognizerIsCreated(recogNameHint);
		if (hasError())
			return nullptr;
		return recognizer_.get();
	}

	void JuliusRecognizerProvider::ensureRecognizerIsCreated(boost::string_view recogNameHint)
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