#include <string>
#include <vector>
#include <chrono> // std::chrono::system_clock
#include <set>
#include <QString>
#include <QDir>
#include "PhoneticService.h"
#include "SpeechProcessing.h"

namespace PticaGovorun
{
	namespace details
	{
		// Component of audio file path, used in Sphinx .fieldIds and .transcription files.
		struct AudioFileRelativePathComponents
		{
			QString SegFileNameNoExt; // file name of the output audio segment without extension
			QString AudioSegFilePathNoExt; // abs file path to the output audio segment
			QString WavOutRelFilePathNoExt; // wavBaseOutDir concat this=wavOutFilePath
		};

		struct AssignedPhaseAudioSegment
		{
			const AnnotatedSpeechSegment* Seg;
			ResourceUsagePhase Phase;
			AudioFileRelativePathComponents OutAudioSegPathParts;
		};
	}

	// Gets the string which can be used as a version of a trained speech model.
	// Returns the name of the directory where the sphinx model resides.
	PG_EXPORTS std::wstring sphinxModelVersionStr(boost::wstring_ref modelDir);

	// Creates data required to train Sphinx engine.
	class PG_EXPORTS SphinxTrainDataBuilder
	{
		typedef std::chrono::system_clock Clock;
	public:
		void run();
		void loadKnownPhoneticDicts(bool includeBrownBear);
	private:
		QString filePath(const QString& relFilePath) const;
		bool hasPhoneticExpansion(boost::wstring_ref word, bool useBroken) const;
		bool isBrokenUtterance(boost::wstring_ref text) const;
		const PronunciationFlavour* expandWellKnownPronCode(boost::wstring_ref pronCode, bool useBrokenDict) const;

		std::tuple<bool, const char*> buildPhaseSpecificParts(ResourceUsagePhase phase, int maxWordPartUsage, int maxUnigramsCount, bool allowPhoneticWordSplit, const std::set<PhoneId>& trainPhoneIds,
			int* dictWordsCount = nullptr, int* phonesCount = nullptr);

		void findWordsWithoutPronunciation(const std::vector<AnnotatedSpeechSegment>& segments, bool useBroken, std::vector<boost::wstring_ref>& unkWords) const;

		//
		void loadDeclinationDictionary(std::unordered_map<std::wstring, std::unique_ptr<WordDeclensionGroup>>& declinedWordDict);
		void loadPhoneticSplitter(const std::unordered_map<std::wstring, std::unique_ptr<WordDeclensionGroup>>& declinedWordDict, UkrainianPhoneticSplitter& phoneticSplitter);

		// phonetic dict
		std::tuple<bool, const char*> buildPhoneticDictionary(const std::vector<const WordPart*>& seedUnigrams, 
			std::function<auto (boost::wstring_ref pronCode) -> const PronunciationFlavour*> expandWellKnownPronCode,
			std::vector<PhoneticWord>& phoneticDictWords);

		std::tuple<bool, const char*> createPhoneticDictionaryFromUsedWords(wv::slice<const WordPart*> seedWordParts, const UkrainianPhoneticSplitter& phoneticSplitter,
			std::function<auto (boost::wstring_ref, PronunciationFlavour&) -> bool> expandPronCodeFun,
			std::vector<PhoneticWord>& phoneticDictWords);

		bool writePhoneList(const std::vector<std::string>& phoneList, const QString& phoneListFile) const;

		//
		void loadAudioAnnotation(const wchar_t* wavRootDir, const wchar_t* annotRootDir, const wchar_t* wavDirToAnalyze, bool includeBrownBear);
		std::tuple<bool, const char*> partitionTrainTestData(const std::vector<AnnotatedSpeechSegment>& segments, double trainCasesRatio, bool swapTrainTestData, bool useBrokenPronsInTrainOnly, int randSeed,
			std::vector<details::AssignedPhaseAudioSegment>& outSegRefs, std::set<PhoneId>& trainPhoneIds) const;

		// Ensures that the test phoneset is a subset of train phoneset.
		std::tuple<bool, const char*> putSentencesWithRarePhonesInTrain(std::vector<details::AssignedPhaseAudioSegment>& segments, std::set<PhoneId>& trainPhoneIds) const;
		
		void fixWavSegmentOutputPathes(const QString& audioSrcRootDir,
			const QString& wavBaseOutDir,
			ResourceUsagePhase targetPhase,
			const QString& wavDirForRelPathes,
			std::vector<details::AssignedPhaseAudioSegment>& outSegRefs);

		bool writeFileIdAndTranscription(const std::vector<details::AssignedPhaseAudioSegment>& segRefs, ResourceUsagePhase targetPhase,
			const QString& fileIdsFilePath, const QString& transcriptionFilePath) const;
		std::tuple<bool, const char*>  loadSilenceSegment(std::vector<short>& frames, float framesFrameRate) const;
		void buildWavSegments(const std::vector<details::AssignedPhaseAudioSegment>& segRefs, float targetFrameRate, bool padSilence, const std::vector<short>& silenceFrames);

		void generateDataStat(const std::vector<details::AssignedPhaseAudioSegment>& phaseAssignedSegs);
		
		// Prints data statistics (speech segments, dictionaries).
		void printDataStat(QDateTime genDate, const QString& statFilePath);

	public:
		QString errMsg_;
	private:
		QString dbName_;
		QDir outDir_;
		std::unique_ptr<GrowOnlyPinArena<wchar_t>> stringArena_;
		PhoneRegistry phoneReg_;

		std::vector<PhoneticWord> phoneticDictWordsWellFormed_;
		std::vector<PhoneticWord> phoneticDictWordsBrownBear_;
		std::vector<PhoneticWord> phoneticDictWordsBroken_;
		std::vector<PhoneticWord> phoneticDictWordsFiller_;

		std::map<boost::wstring_ref, PhoneticWord> phoneticDictWellFormed_;
		std::map<boost::wstring_ref, PhoneticWord> phoneticDictBroken_;

		std::map<boost::wstring_ref, PronunciationFlavour> pronCodeToObjWellFormed_;
		std::map<boost::wstring_ref, PronunciationFlavour> pronCodeToObjBroken_;
		std::map<boost::wstring_ref, PronunciationFlavour> pronCodeToObjFiller_;

		//
		UkrainianPhoneticSplitter phoneticSplitter_;

		std::vector<AnnotatedSpeechSegment> segments_;

		// statistics
		long long dataGenerationDurSec_ = -1; // duration (sec) was taken to generate data
		int dictWordsCountTrain_ = -1;
		int dictWordsCountTest_ = -1;
		int utterCountTrain_ = -1;
		int utterCountTest_ = -1;
		int wordCountTrain_ = -1;
		int wordCountTest_ = -1;
		int phonesCountTrain_ = -1;
		int phonesCountTest_ = -1;
		double audioDurationSecTrain_ = 0; // duration (in seconds) of speech with padded silence
		double audioDurationSecTest_ = 0;
		double audioDurationNoPaddingSecTrain_ = 0; // duration (in seconds) of speech only (no padded silence)
		double audioDurationNoPaddingSecTest_ = 0;
		std::map<std::wstring, double> speakerIdToAudioDurSec_;
	};

	// Writes phonetic dictionary to file.
	// Each line has one pronunciation.
	std::tuple<bool,const char*> writePhoneticDictSphinx(const std::vector<PhoneticWord>& phoneticDictWords, const PhoneRegistry& phoneReg, const QString& filePath);

	PG_EXPORTS bool readSphinxFileFileId(boost::wstring_ref fileIdPath, std::vector<std::wstring>& fileIds);

	// The line in Sphinx *.transcriptino file.
	struct SphinxTranscriptionLine
	{
		std::wstring Transcription;
		std::wstring FileNameWithoutExtension; // fileId
	};

	PG_EXPORTS bool readSphinxFileTranscription(boost::wstring_ref transcriptionFilePath, std::vector<SphinxTranscriptionLine>& transcriptions);
	PG_EXPORTS bool checkFileIdTranscrConsistency(const std::vector<std::wstring>& dataFilePathNoExt, const std::vector<SphinxTranscriptionLine>& dataTranscr);

	// move to Sphinx
	struct PG_EXPORTS AudioData
	{
		std::vector<short> Frames;
		float FrameRate;
	};

	PG_EXPORTS bool loadSphinxAudio(boost::wstring_ref audioDir, const std::vector<std::wstring>& audioRelPathesNoExt, boost::wstring_ref audioFileSuffix, std::vector<AudioData>& audioDataList);


}

struct SphinxConfig
{
	// the values are from etc/sphinx_train.cfg
	// and are used in sphinxtrain/scripts/decode/psdecode.pl in RunTool('pocketsphinx_batch') call.
	// these parameters are smaller, making search longer and speech recognition performance improves

	static const char* DEC_CFG_LANGUAGEWEIGHT()
	{
		return "10"; // default=6.5
	}

	static const char* DEC_CFG_BEAMWIDTH()
	{
		return "1e-80"; // default=1e-48
	}

	static const char* DEC_CFG_WORDBEAM()
	{
		return "1e-40"; // default=7e-29
	}
};