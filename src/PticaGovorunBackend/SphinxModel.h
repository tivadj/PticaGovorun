#include <string>
#include <vector>
#include <chrono> // std::chrono::system_clock
#include <set>
#include <map>
#include <QString>
#include <QVariant>
#include <QDir>
#include <pocketsphinx.h> // cmd_ln_init
#include "PhoneticService.h"
#include "SpeechProcessing.h"
#include "SpeechDataValidation.h"

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

	struct PG_EXPORTS SphinxConfig
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

		/// @acousticModelDir hmm
		/// @langModelFile accepts both TXT and DMP formats
		/// @logFile or empty reference (null)
		static cmd_ln_t* pg_init_cmd_ln_t(boost::string_ref acousticModelDir, boost::string_ref langModelFile,
		                                 boost::string_ref phoneticModelFile, bool verbose, bool debug, bool backTrace,
		                                 boost::string_ref logFile);
	};

	/// Maps pronCode to Sphinx display name in .dic, .arpa and .transcript files.
	struct PronCodeToDisplayNameMap
	{
		explicit PronCodeToDisplayNameMap(GrowOnlyPinArena<wchar_t>* stringArena)
			: stringArena_(stringArena)
		{
		}

		std::map<boost::wstring_ref, std::vector<std::pair<boost::wstring_ref, boost::wstring_ref>>> map;
		GrowOnlyPinArena<wchar_t>* stringArena_;
	};

	struct PronCodeToDisplayNameMapTmp
	{
		explicit PronCodeToDisplayNameMapTmp(GrowOnlyPinArena<wchar_t>& stringArena)
			: stringArena_(stringArena)
		{
		}

		std::map<boost::wstring_ref, std::vector<std::pair<boost::wstring_ref, boost::wstring_ref>>> map;
		std::reference_wrapper<GrowOnlyPinArena<wchar_t>> stringArena_;
	};

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
		const PhoneticWord* findWellKnownWord(boost::wstring_ref word, bool useBrokenDict) const;

		void chooseSeedUnigramsNew(const UkrainianPhoneticSplitter& phoneticSplitter, int minWordPartUsage, int maxUnigramsCount, bool allowPhoneticWordSplit,
			const PhoneRegistry& phoneReg,
			std::function<auto (boost::wstring_ref word) -> const PhoneticWord*> findWellFormedWord,
			const std::set<PhoneId>& trainPhoneIds,
			WordPhoneticTranscriber& phoneticTranscriber,
			ResourceUsagePhase phase,
			std::vector<PhoneticWord>& result);

		void buildPhaseSpecificParts(ResourceUsagePhase phase, int maxWordPartUsage, int maxUnigramsCount, bool allowPhoneticWordSplit, 
			const std::set<PhoneId>& trainPhoneIds, int gramDim,
			std::function<auto (boost::wstring_ref)->boost::wstring_ref> pronCodeDisplay,
			WordPhoneticTranscriber& phoneticTranscriber,
			int* dictWordsCount = nullptr, int* phonesCount = nullptr);

		//
		void loadDeclinationDictionary(std::unordered_map<std::wstring, std::unique_ptr<WordDeclensionGroup>>& declinedWordDict);
		void phoneticSplitterBootstrapOnDeclinedWords(UkrainianPhoneticSplitter& phoneticSplitter);
		void phoneticSplitterCollectWordUsageInText(UkrainianPhoneticSplitter& phoneticSplitter, int maxFilesToProcess, bool outputCorpus, boost::filesystem::path corpusFilePath);
		void phoneticSplitterRegisterWordsFromPhoneticDictionary(UkrainianPhoneticSplitter& phoneticSplitter);
		void phoneticSplitterLoad(UkrainianPhoneticSplitter& phoneticSplitter, int maxFilesToProcess, bool outputCorpus, boost::filesystem::path corpusFilePath);

		// phonetic dict
		std::tuple<bool, const char*> buildPhoneticDictionary(const std::vector<const WordPart*>& seedUnigrams, 
			std::function<auto (boost::wstring_ref pronCode) -> const PronunciationFlavour*> expandWellKnownPronCode,
			std::vector<PhoneticWord>& phoneticDictWords);

		void buildPhoneticDictionaryNew(const std::vector<PhoneticWord>& seedUnigrams, 
			std::vector<PhoneticWord>& phoneticDictWords);

		std::tuple<bool, const char*> createPhoneticDictionaryFromUsedWords(wv::slice<const WordPart*> seedWordParts, const UkrainianPhoneticSplitter& phoneticSplitter,
			std::function<auto (boost::wstring_ref, PronunciationFlavour&) -> bool> expandPronCodeFun,
			std::vector<PhoneticWord>& phoneticDictWords);

		bool writePhoneList(const std::vector<std::string>& phoneList, const QString& phoneListFile) const;

		// language model
		void langModelRecoverUsageOfUnusedWords(const std::vector<PhoneticWord> vocabWords, UkrainianPhoneticSplitter& phoneticSplitter, bool includeBrokenWords, std::map<int, ptrdiff_t>& wordPartIdToRecoveredUsage);

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
			const QString& fileIdsFilePath,
			std::function<auto (boost::wstring_ref)->boost::wstring_ref> pronCodeDisplay,
			const QString& transcriptionFilePath);
		std::tuple<bool, const char*>  loadSilenceSegment(std::vector<short>& frames, float framesFrameRate) const;
		void buildWavSegments(const std::vector<details::AssignedPhaseAudioSegment>& segRefs, float targetFrameRate, bool padSilence, const std::vector<short>& silenceFrames);

		void generateDataStat(const std::vector<details::AssignedPhaseAudioSegment>& phaseAssignedSegs);
		
		// Prints data statistics (speech segments, dictionaries).
		void printDataStat(QDateTime genDate, const std::map<std::string,QVariant> speechModelConfig, const QString& statFilePath);

	public:
		QString errMsg_;
	private:
		QString dbName_;
		QDir outDir_;
		QDir speechProjDir_;
		std::shared_ptr<GrowOnlyPinArena<wchar_t>> stringArena_;
		PhoneRegistry phoneReg_;
		std::shared_ptr<SpeechData> speechData_;

		//std::vector<PhoneticWord> phoneticDictWordsWellFormed_;
		//std::vector<PhoneticWord> phoneticDictWordsBroken_;
		//std::vector<PhoneticWord> phoneticDictWordsFiller_;
		std::vector<PhoneticWord> phoneticDictWordsBrownBear_;

		std::map<boost::wstring_ref, PhoneticWord> phoneticDictWellFormed_;
		std::map<boost::wstring_ref, PhoneticWord> phoneticDictBroken_;
		std::map<boost::wstring_ref, PhoneticWord> phoneticDictFiller_;

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

	/// Writes phonetic dictionary to file.
	/// Each line has one pronunciation.
	bool writePhoneticDictSphinx(const std::vector<PhoneticWord>& phoneticDictWords, const PhoneRegistry& phoneReg, const QString& filePath, std::function<auto (boost::wstring_ref)->boost::wstring_ref> pronCodeDisplay = nullptr, QString* errMsg = nullptr);

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

	bool loadWordList(boost::wstring_ref filePath, std::unordered_set<std::wstring>& words);
	bool writeWordList(const std::vector<boost::wstring_ref>& words, boost::filesystem::path filePath, QString* errMsg);

	// Consistency rule for created speech model.
	// Broken pronunciations are allowed in training (but not in test) dataset.
	bool rulePhoneticDicHasNoBrokenPronCodes(const std::vector<PhoneticWord>& phoneticDictPronCodes, const std::map<boost::wstring_ref, PhoneticWord>& phoneticDictBroken, std::vector<boost::wstring_ref>* brokenPronCodes = nullptr);

	/// clothes(2) -> clothes2
	void manglePronCodeSphinx(boost::wstring_ref pronCode, std::wstring& result);
}
