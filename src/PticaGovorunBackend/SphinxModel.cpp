#include "stdafx.h"
#include "SphinxModel.h"
#include <vector>
#include <random>
#include <chrono> // std::chrono::system_clock
#include <QDir>
#include <QDateTime>
#include "SpeechProcessing.h"
#include "PhoneticService.h"
#include "WavUtils.h"
#include "ClnUtils.h"
#include "ArpaLanguageModel.h"
#include "assertImpl.h"
#include "AppHelpers.h"

namespace PticaGovorun
{
	std::wstring sphinxModelVersionStr(boost::wstring_ref modelDir)
	{
		QDir modelDirQ(toQString(modelDir));
		if (!modelDirQ.cd("etc"))
			return L"";

		QString trainConfigPath = modelDirQ.absoluteFilePath("sphinx_train.cfg");
		QFile configFile(trainConfigPath);
		if (!configFile.open(QIODevice::ReadOnly))
			return L"";

		// find the line with Sphinx model base path

		// eg the line
		// $CFG_BASE_DIR = "C:/TrainSphinx/persian.v39";
		QString baseDirLine;
		QTextStream txt(&configFile);
		while (!txt.atEnd())
		{
			QString line = txt.readLine();
			int ind1 = line.indexOf("$CFG_BASE_DIR");
			if (ind1 != -1)
			{
				baseDirLine = line;
				break;
			}
		}

		if (baseDirLine.isNull())
			return L"";

		const QChar doubleQuoute('\"');
		int i1 = baseDirLine.indexOf(doubleQuoute);
		int i2 = baseDirLine.lastIndexOf(doubleQuoute);
		if (i1 == -1 || i2 == -1)
			return L"";

		QString baseDirath = baseDirLine.mid(i1 + 1, i2 - i1 - 1);
		QDir dirInfo(baseDirath);
		QString sphinxDirName = dirInfo.dirName(); // name of the deepest dir
		return sphinxDirName.toStdWString();
	}

	// Collects all phones used in the set of phonetic dictionaries.
	class PhoneAccumulator
	{
		std::unordered_set<PhoneId, IdWithDebugStrTraits<PhoneId>::hasher> phoneIdsSet_;
	public:
		void addPhonesFromPhoneticDict(const std::vector<PhoneticWord>& phoneticDictWords)
		{
			for (const PhoneticWord& word : phoneticDictWords)
			{
				for (const PronunciationFlavour& pron : word.Pronunciations)
					addPhones(pron.Phones);
			}
		}
		void addPhones(const std::vector<PhoneId>& phones)
		{
			for (PhoneId ph : phones)
				phoneIdsSet_.insert(ph);
		}
		void buildPhoneList(const PhoneRegistry& phoneReg, std::vector<std::string>& phones)
		{
			phones.clear();
			std::transform(phoneIdsSet_.begin(), phoneIdsSet_.end(), std::back_inserter(phones), [this,&phoneReg](PhoneId phId)
			{
				std::string result;
				bool getPhone = phoneToStr(phoneReg, phId, result);
				PG_Assert(getPhone);
				return result;
			});
			std::sort(phones.begin(), phones.end());
		}
		void copyPhoneList(std::function<void(PhoneId)> result)
		{
			for (const auto& ph : phoneIdsSet_) result(ph);
		}
	};

	// Finds the usage of pronCode using statistics of the corresponding word.
	void inferPhoneticDictPronIdsUsage(UkrainianPhoneticSplitter& phoneticSplitter, const std::map<boost::wstring_ref, PhoneticWord>& phoneticDict)
	{
		WordsUsageInfo& wordUsage = phoneticSplitter.wordUsage();

		// step1: insert pronIds
		for (const auto& pair : phoneticDict)
		{
			const PhoneticWord& word = pair.second;
			std::wstring wordW = toStdWString(word.Word);

			// find word usage
			long wordTimesUsed = -1;
			const WordPart* wordPart = wordUsage.wordPartByValue(wordW, WordPartSide::WholeWord);
			if (wordPart != nullptr)
			{
				// assert: wordPart is not denied
				WordSeqKey key = { wordPart->id() };
				wordTimesUsed = wordUsage.getWordSequenceUsage(key);
			}

			// propogate word usage to corresponding pronCodes

			for (const PronunciationFlavour& pron : word.Pronunciations)
			{
				bool wasAdded = false;
				WordPart* pronPart = wordUsage.getOrAddWordPart(toStdWString(pron.PronCode), WordPartSide::WholeWord, &wasAdded);

				// here we, kind of, create pronCode
				// eg. word1='b' expands into pronunciations PronCode1='B' and PronCode2='P'
				// we may deny word='p' and set it's removed flag to true
				// but PronCode2='p' should be allowed
				pronPart->setRemoved(false);

				// update usage
				if (wasAdded && wordTimesUsed != -1)
				{
					// evenly propogate word usage to pronIds
					WordSeqKey key = { pronPart->id() };
					WordSeqUsage* u = wordUsage.getOrAddWordSequence(key, &wasAdded);
					u->UsedCount = wordTimesUsed / (long)word.Pronunciations.size();
				}
			}
		}

		// propogate word usage on pronIds for bigram

		std::vector<const WordSeqUsage*> wordSeqUsages;
		wordUsage.copyWordSeq(wordSeqUsages);
		for (const WordSeqUsage* seqUsage : wordSeqUsages)
		{
			if (seqUsage->Key.PartCount != 2) // 2=bigram
				continue;

			int wordPart1Id = seqUsage->Key.PartIds[0];
			int wordPart2Id = seqUsage->Key.PartIds[1];

			const WordPart* wordPart1 = wordUsage.wordPartById(wordPart1Id);
			PG_Assert(wordPart1 != nullptr);

			const WordPart* wordPart2 = wordUsage.wordPartById(wordPart2Id);
			PG_Assert(wordPart2 != nullptr);

			// the wordPart may point to a word which can be expanded to pronId in multiple ways
			auto wordIt1 = phoneticDict.find(wordPart1->partText());
			auto wordIt2 = phoneticDict.find(wordPart2->partText());
			bool noExpansion = wordIt1 == phoneticDict.end() && wordIt2 == phoneticDict.end();
			if (noExpansion)
				continue;

			auto collectPronIds = [&phoneticDict, &wordUsage](const PhoneticWord& word, std::vector<const WordPart*>& parts)
			{
				for (const PronunciationFlavour& pron : word.Pronunciations)
				{
					const WordPart* pronPart = wordUsage.wordPartByValue(toStdWString(pron.PronCode), WordPartSide::WholeWord);
					PG_Assert2(pronPart != nullptr, "PronIds were added in step1");
					parts.push_back(pronPart);
				}
			};

			std::vector<const WordPart*> parts1;
			if (wordIt1 != phoneticDict.end())
			{
				const PhoneticWord& word1 = wordIt1->second;
				collectPronIds(word1, parts1);
			}
			else parts1.push_back(wordPart1);

			std::vector<const WordPart*> parts2;
			if (wordIt2 != phoneticDict.end())
			{
				const PhoneticWord& word2 = wordIt2->second;
				collectPronIds(word2, parts2);
			}
			else parts2.push_back(wordPart2);

			// evenly distribute usage across all pronIds
			long usagePerPronId = (long)seqUsage->UsedCount / (parts1.size() * parts2.size());
			if (usagePerPronId == 0) // keep min usage > 0
				usagePerPronId = 1;
			for (size_t i1 = 0; i1 < parts1.size(); ++i1)
				for (size_t i2 = 0; i2 < parts2.size(); ++i2)
				{
					const WordPart* p1 = parts1[i1];
					const WordPart* p2 = parts2[i2];

					WordSeqKey biKey = { p1->id(), p2->id() };
					bool wasAdded = false;
					WordSeqUsage* u = wordUsage.getOrAddWordSequence(biKey, &wasAdded);
					u->UsedCount = usagePerPronId;
				}
		}
	}

	void rejectDeniedWords(const std::unordered_set<std::wstring>& denyWords, WordsUsageInfo& wordUsage)
	{
		std::vector<WordPart*> allWordParts;
		wordUsage.copyWordParts(allWordParts);

		for (WordPart* wordPart : allWordParts)
		{
			if (denyWords.find(wordPart->partText()) != std::end(denyWords))
			{
				// ignore deny words
				wordPart->setRemoved(true);
				std::wcout << L"Ignore deny word=" << wordPart->partText() << std::endl; // debug
			}
		}
	}

	void addFillerWords(WordsUsageInfo& wordUsage)
	{
		bool isAdded = false;
		wordUsage.getOrAddWordPart(toStdWString(fillerInhale()), WordPartSide::WholeWord, &isAdded);
		PG_Assert(isAdded);
	}

	void chooseSeedUnigrams(const UkrainianPhoneticSplitter& phoneticSplitter, int minWordPartUsage, int maxUnigramsCount, bool allowPhoneticWordSplit, const PhoneRegistry& phoneReg,
		std::function<auto (boost::wstring_ref pronCode) -> const PronunciationFlavour*> expandWellKnownPronCode,
		const std::set<PhoneId>& trainPhoneIds,
		std::vector<const WordPart*>& result)
	{
		const WordsUsageInfo& wordUsage = phoneticSplitter.wordUsage();

		std::vector<const WordPart*> allWordParts;
		wordUsage.copyWordParts(allWordParts);

		std::vector<const WordPart*> wordPartSureInclude;
		std::vector<const WordPart*> wordPartCandidates;
		std::vector<PhoneId> phonesTmp;

		for (const WordPart* wordPart : allWordParts)
		{
			if (wordPart->removed())
				continue;

			// include <s>, </s> and ~Right parts
			if (wordPart == phoneticSplitter.sentStartWordPart() ||
				wordPart == phoneticSplitter.sentEndWordPart() ||
				(allowPhoneticWordSplit && wordPart->partSide() == WordPartSide::RightPart) ||
				wordPart->partText().compare(fillerInhale().data()) == 0
				)
			{
				wordPartSureInclude.push_back(wordPart);
				continue;
			}

			// the pronCode must expand in phones from the training set
			auto coverTrainPhonesOnlyFun = [](const std::vector<PhoneId>& pronPhones, const std::set<PhoneId>& trainPhoneIds)
			{
				return std::all_of(pronPhones.begin(), pronPhones.end(), [&trainPhoneIds](PhoneId phId) -> bool
				{
					return trainPhoneIds.find(phId) != trainPhoneIds.end();
				});
			};

			// always include pronIds
			bool includeTrainWordsInLM = true;
			if (includeTrainWordsInLM)
			{
				const PronunciationFlavour* pron = expandWellKnownPronCode(wordPart->partText());
				if (pron != nullptr)
				{
					bool useTrainPhones = coverTrainPhonesOnlyFun(pron->Phones, trainPhoneIds);
					if (useTrainPhones)
					{
						wordPartSureInclude.push_back(wordPart);
						continue;
					}
				}
			}

			// add word as is
			auto canSpellFun = [&wordUsage, &phoneticSplitter, &phonesTmp, &phoneReg](const WordPart* part)
			{
				PG_Assert(part != phoneticSplitter.sentStartWordPart() && part != phoneticSplitter.sentEndWordPart());

				phonesTmp.clear();
				bool spellOp;
				const char* errMsg;
				const std::wstring& partTxt = part->partText();
				std::tie(spellOp, errMsg) = spellWordUk(phoneReg, partTxt, phonesTmp);
				return spellOp;
			};
			WordSeqKey seqKey({ wordPart->id() });
			long wordSeqUsage = wordUsage.getWordSequenceUsage(seqKey);
			bool isFreqUsage = minWordPartUsage == -1 || wordSeqUsage >= minWordPartUsage;

			if (isFreqUsage && canSpellFun(wordPart))
			{
				bool useTrainPhones = coverTrainPhonesOnlyFun(phonesTmp, trainPhoneIds);
				if (useTrainPhones)
					wordPartCandidates.push_back(wordPart);
			}
		}

		//
		if (maxUnigramsCount > 0)
		{
			int takeUnigrams = maxUnigramsCount - (int)wordPartSureInclude.size();
			if (takeUnigrams < 0)
				takeUnigrams = 0;

			// sort descending by UsageCount
			std::sort(wordPartCandidates.begin(), wordPartCandidates.end(), [&wordUsage](const WordPart* a, const WordPart* b)
			{
				WordSeqKey aSeqKey({ a->id() });
				long aUsageCount = wordUsage.getWordSequenceUsage(aSeqKey);

				WordSeqKey bSeqKey({ b->id() });
				long bUsageCount = wordUsage.getWordSequenceUsage(bSeqKey);

				return aUsageCount > bUsageCount;
			});

			// take N most used unigrams
			if (wordPartCandidates.size() > takeUnigrams)
				wordPartCandidates.resize(takeUnigrams);
		}

		wordPartCandidates.insert(wordPartCandidates.end(), wordPartSureInclude.begin(), wordPartSureInclude.end());

		// sort lexicographically
		std::sort(wordPartCandidates.begin(), wordPartCandidates.end(), [&wordUsage](const WordPart* a, const WordPart* b)
		{
			return a->partText() < b->partText();
		});

		result.insert(result.end(), std::begin(wordPartCandidates), std::end(wordPartCandidates));
	}

	void SphinxTrainDataBuilder::run()
	{
		// start timer
		typedef std::chrono::system_clock Clock;
		std::chrono::time_point<Clock> now1 = Clock::now();

		dbName_ = "persian";

		QDateTime generationDate = QDateTime::currentDateTime();
		std::string timeStamp;
		appendTimeStampNow(timeStamp);

		auto  outDirPath = QString(R"path(C:\devb\PticaGovorunProj\data\TrainSphinx\SphinxModel_%1)path").arg(QString::fromStdString(timeStamp));
		outDir_ = QDir(outDirPath);
		bool op = outDir_.mkpath(".");
		if (!op)
		{
			errMsg_ = "Can't create output directory";
			return;
		}

		const char* ConfigRandSeed = "randSeed";
		const char* ConfigSwapTrainTestData = "swapTrainTestData";
		const char* ConfigIncludeBrownBear = "includeBrownBear";
		const char* ConfigOutputWav = "outputWav";
		const char* ConfigPadSilence = "padSilence";
		const char* ConfigTrainCasesRatio = "trainCasesRatio";
		const char* ConfigUseBrokenPronsInTrainOnly = "useBrokenPronsInTrainOnly";
		const char* ConfigAllowSoftHardConsonant = "allowSoftHardConsonant";
		const char* ConfigAllowVowelStress = "allowVowelStress";
		const char* ConfigMinWordPartUsage = "minWordPartUsage";
		const char* ConfigMaxUnigramsCount = "maxUnigramsCount";

		int randSeed = AppHelpers::configParamInt(ConfigRandSeed, 1932);
		bool swapTrainTestData = AppHelpers::configParamBool(ConfigSwapTrainTestData, false); // swaps train/test portions of data, so that opposite data can be tested when random generator's seed is fixed
		bool includeBrownBear = AppHelpers::configParamBool(ConfigIncludeBrownBear, false);
		bool outputWav = AppHelpers::configParamBool(ConfigOutputWav, true);
		bool outputPhoneticDictAndLangModel = true;
		bool padSilence = AppHelpers::configParamBool(ConfigPadSilence, true); // pad the audio segment with the silence segment
		bool allowSoftHardConsonant = true;
		bool allowVowelStress = true;
		PalatalSupport palatalSupport = PalatalSupport::AsHard;
		bool useBrokenPronsInTrainOnly = true;
		const double trainCasesRatio = AppHelpers::configParamDouble(ConfigTrainCasesRatio, 0.7);
		const float outFrameRate = 16000; // Sphinx requires 16k
		
		// words with greater usage counter are inlcuded in language model
		// note, the positive threshold may evict the rare words
		// -1=to ignore the parameter
		int minWordPartUsage = 10;
		//int maxWordPartUsage = -1;
		//int maxUnigramsCount = 10000;
		int maxUnigramsCount = -1;

		stringArena_ = std::make_unique<GrowOnlyPinArena<wchar_t>>(10000);
		phoneReg_.setPalatalSupport(palatalSupport);
		initPhoneRegistryUk(phoneReg_, allowSoftHardConsonant, allowVowelStress);

		//
		loadKnownPhoneticDicts(includeBrownBear);
		if (!errMsg_.isEmpty()) return;

		// load annotation
		std::wstring speechWavRootDir = AppHelpers::mapPath("data/SpeechAudio").toStdWString();
		std::wstring speechAnnotRootDir = AppHelpers::mapPath("data/SpeechAnnot").toStdWString();
		std::wstring wavDirToAnalyze = AppHelpers::mapPath("data/SpeechAudio").toStdWString();

		loadAudioAnnotation(speechWavRootDir.c_str(), speechAnnotRootDir.c_str(), wavDirToAnalyze.c_str(), includeBrownBear);
		std::wcout << "Found annotated segments: " << segments_.size() << std::endl; // debug

		// do checks

		std::vector<boost::wstring_ref> unkWords;
		findWordsWithoutPronunciation(segments_, true, unkWords);
		if (!unkWords.empty())
		{
			errMsg_ = "Found words without phonetic specification";
			for (const boost::wstring_ref word : unkWords)
				errMsg_ += ", " + toQString(word);
			return;
		}

		//
		QString denyWordsFilePath = AppHelpers::mapPath("data/LM/denyWords.txt");
		std::unordered_set<std::wstring> denyWords;
		if (!loadWordList(denyWordsFilePath.toStdWString(), denyWords))
		{
			errMsg_ = "Can't load 'denyWords.txt' dictionary";
			return;
		}

		// TODO: this check must be added to model's consistency checker (used in UI)
		// consistency check: denied words can't be in transcription
		std::vector<std::reference_wrapper<PhoneticWord>> usedWords;
		std::copy(std::begin(phoneticDictWordsWellFormed_), std::end(phoneticDictWordsWellFormed_), std::back_inserter(usedWords));
		std::copy(std::begin(phoneticDictWordsBroken_), std::end(phoneticDictWordsBroken_), std::back_inserter(usedWords));
		if (includeBrownBear)
			std::copy(std::begin(phoneticDictWordsBrownBear_), std::end(phoneticDictWordsBrownBear_), std::back_inserter(usedWords));

		std::vector<boost::wstring_ref> usedDeniedWords;
		for (const PhoneticWord& word : usedWords)
		{
			if (denyWords.find(toStdWString(word.Word)) != std::end(denyWords))
			{
				usedDeniedWords.push_back(word.Word);
			}
		}

		if (!usedDeniedWords.empty())
		{
			errMsg_ = "Found used words which are denied: ";
			for (const boost::wstring_ref word : usedDeniedWords)
				errMsg_ += ", " + toQString(word);
			return;
		}

		std::vector<details::AssignedPhaseAudioSegment> phaseAssignedSegs;
		std::set<PhoneId> trainPhoneIds;
		const char* errMsg;
		std::tie(op, errMsg) = partitionTrainTestData(segments_, trainCasesRatio, swapTrainTestData, useBrokenPronsInTrainOnly, randSeed, phaseAssignedSegs, trainPhoneIds);
		if (!op)
		{
			errMsg_ = "Can't fix train/test partitioning";
			return;
		}

		//
		if (outputPhoneticDictAndLangModel)
		{
			bool allowPhoneticWordSplit = false;
			phoneticSplitter_.setAllowPhoneticWordSplit(allowPhoneticWordSplit);

			std::unordered_map<std::wstring, std::unique_ptr<WordDeclensionGroup>> declinedWordDict;
			loadDeclinationDictionary(declinedWordDict);
			loadPhoneticSplitter(declinedWordDict, phoneticSplitter_);

			// we call it before propogating pronCodes to words
			rejectDeniedWords(denyWords, phoneticSplitter_.wordUsage());

			// ensure that LM covers all pronunciations in phonetic dictionary
			addFillerWords(phoneticSplitter_.wordUsage());
			inferPhoneticDictPronIdsUsage(phoneticSplitter_, phoneticDictWellFormed_);
			inferPhoneticDictPronIdsUsage(phoneticSplitter_, phoneticDictBroken_);

			// .dic and .arpa files are different
			// broken words goes in dict and lang model in train phase only
			// alternatively we can always ignore broken words, but it is a pity to not use the markup
			std::tie(op, errMsg) = buildPhaseSpecificParts(ResourceUsagePhase::Train, minWordPartUsage, maxUnigramsCount, allowPhoneticWordSplit, trainPhoneIds, &dictWordsCountTrain_, &phonesCountTrain_);
			if (!op)
			{
				errMsg_ = QString("Can't build phase specific parts. %1").arg(errMsg);
				return;
			}
			std::tie(op, errMsg) = buildPhaseSpecificParts(ResourceUsagePhase::Test, minWordPartUsage, maxUnigramsCount, allowPhoneticWordSplit, trainPhoneIds, &dictWordsCountTest_, &phonesCountTest_);
			if (!op)
			{
				errMsg_ = QString("Can't build phase specific parts. %1").arg(errMsg);
				return;
			}

			// filler dictionary
			std::tie(op, errMsg) = writePhoneticDictSphinx(phoneticDictWordsFiller_, phoneReg_, filePath(dbName_ + ".filler"));
			if (!op)
			{
				errMsg_ = "Can't write phonetic dictionary file";
				return;
			}
		}

		//
		QString audioOutDirPath = outDir_.absolutePath() + "/wav/";

		QString dataPartNameTrain = dbName_ + "_train";
		QString dataPartNameTest = dbName_ + "_test";

		QString outDirWavTrain = audioOutDirPath + dataPartNameTrain + "/";
		QString outDirWavTest = audioOutDirPath + dataPartNameTest + "/";

		auto audioSrcRootDir = QString::fromStdWString(speechWavRootDir);

		// find where to put output audio segments
		fixWavSegmentOutputPathes(audioSrcRootDir, audioOutDirPath, ResourceUsagePhase::Train, outDirWavTrain, phaseAssignedSegs);
		fixWavSegmentOutputPathes(audioSrcRootDir, audioOutDirPath, ResourceUsagePhase::Test, outDirWavTest, phaseAssignedSegs);

		writeFileIdAndTranscription(phaseAssignedSegs, ResourceUsagePhase::Train, filePath(dataPartNameTrain + ".fileids"), filePath(dataPartNameTrain + ".transcription"));
		writeFileIdAndTranscription(phaseAssignedSegs, ResourceUsagePhase::Test, filePath(dataPartNameTest + ".fileids"), filePath(dataPartNameTest + ".transcription"));

		// write wavs

		if (outputWav)
		{
			if (!QDir(outDirWavTrain).mkpath(".") || !QDir(outDirWavTest).mkpath("."))
			{
				errMsg_ = "Can't create wav train/test subfolder";
				return;
			}

			std::vector<short> silenceFrames;
			if (padSilence)
			{
				std::tie(op, errMsg) = loadSilenceSegment(silenceFrames, outFrameRate);
				if (!op)
				{
					errMsg_ = QString("Can't create wav train/test subfolder. %1").arg(errMsg);
					return;
				}
			}

			buildWavSegments(phaseAssignedSegs, outFrameRate, padSilence, silenceFrames);
			if (!errMsg_.isEmpty())
				return;
		}

		// generate statistics
		generateDataStat(phaseAssignedSegs);

		// stop timer
		std::chrono::time_point<Clock> now2 = Clock::now();
		dataGenerationDurSec_ = std::chrono::duration_cast<std::chrono::seconds>(now2 - now1).count();

		std::map<std::string, QVariant> speechModelConfig;
		speechModelConfig[ConfigRandSeed] = QVariant::fromValue(randSeed);
		speechModelConfig[ConfigSwapTrainTestData] = QVariant::fromValue(swapTrainTestData);
		speechModelConfig[ConfigIncludeBrownBear] = QVariant::fromValue(includeBrownBear);
		speechModelConfig[ConfigOutputWav] = QVariant::fromValue(outputWav);
		speechModelConfig[ConfigPadSilence] = QVariant::fromValue(padSilence);
		speechModelConfig[ConfigTrainCasesRatio] = QVariant::fromValue(trainCasesRatio);
		speechModelConfig[ConfigUseBrokenPronsInTrainOnly] = QVariant::fromValue(useBrokenPronsInTrainOnly);
		speechModelConfig[ConfigAllowSoftHardConsonant] = QVariant::fromValue(allowSoftHardConsonant);
		speechModelConfig[ConfigAllowVowelStress] = QVariant::fromValue(allowVowelStress);
		speechModelConfig[ConfigMinWordPartUsage] = QVariant::fromValue(minWordPartUsage);
		speechModelConfig[ConfigMaxUnigramsCount] = QVariant::fromValue(maxUnigramsCount);
		printDataStat(generationDate, speechModelConfig, filePath("dataStats.txt"));
	}

	void SphinxTrainDataBuilder::loadKnownPhoneticDicts(bool includeBrownBear)
	{
		// load phonetic vocabularies
		bool loadOp;
		const char* errMsg = nullptr;
		std::wstring persianDictPathKnown = AppHelpers::mapPath("data/PhoneticDict/phoneticDictUkKnown.xml").toStdWString();
		std::tie(loadOp, errMsg) = loadPhoneticDictionaryXml(persianDictPathKnown, phoneReg_, phoneticDictWordsWellFormed_, *stringArena_);
		if (!loadOp)
		{
			errMsg_ = QString("Can't load phonetic dictionary. %1").arg(errMsg);
			return;
		}
		std::wstring persianDictPathBroken = AppHelpers::mapPath("data/PhoneticDict/phoneticDictUkBroken.xml").toStdWString();
		std::tie(loadOp, errMsg) = loadPhoneticDictionaryXml(persianDictPathBroken, phoneReg_, phoneticDictWordsBroken_, *stringArena_);
		if (!loadOp)
		{
			errMsg_ = QString("Can't load phonetic dictionary. %1").arg(errMsg);
			return;
		}
		std::wstring persianDictPathFiller = AppHelpers::mapPath("data/PhoneticDict/phoneticDictFiller.xml").toStdWString();
		std::tie(loadOp, errMsg) = loadPhoneticDictionaryXml(persianDictPathFiller, phoneReg_, phoneticDictWordsFiller_, *stringArena_);
		if (!loadOp)
		{
			errMsg_ = QString("Can't load phonetic dictionary. %1").arg(errMsg);
			return;
		}

		if (includeBrownBear)
		{
			std::wstring brownBearDictPath = AppHelpers::mapPath("data/PhoneticDict/phoneticBrownBear.xml").toStdWString();
			std::tie(loadOp, errMsg) = loadPhoneticDictionaryXml(brownBearDictPath, phoneReg_, phoneticDictWordsBrownBear_, *stringArena_);
			if (!loadOp)
			{
				errMsg_ = QString("Can't load phonetic dictionary. %1").arg(errMsg);
				return;
			}
		}

		// make composite word->wordObj dictionary
		reshapeAsDict(phoneticDictWordsWellFormed_, phoneticDictWellFormed_);
		if (includeBrownBear)
			mergePhoneticDictOnlyNew(phoneticDictWellFormed_, phoneticDictWordsBrownBear_);
		
		reshapeAsDict(phoneticDictWordsBroken_, phoneticDictBroken_);

		// make composite pronCode->pronObj map
		std::vector<boost::wstring_ref> duplicatePronCodes;
		populatePronCodes(phoneticDictWordsWellFormed_, pronCodeToObjWellFormed_, duplicatePronCodes);
		
		populatePronCodes(phoneticDictWordsBroken_, pronCodeToObjBroken_, duplicatePronCodes);
		
		populatePronCodes(phoneticDictWordsFiller_, pronCodeToObjFiller_, duplicatePronCodes);

		if (!duplicatePronCodes.empty())
		{
			errMsg_ = "Phonetic dict contains duplicate pronCodes";
			for (boost::wstring_ref pronCode : duplicatePronCodes)
				errMsg_ += ", " + toQString(pronCode);
			return;
		}

		// duplicate pronCodes check is not done for BrownBear dict
		// duplicate pronCodes are ignored
		if (includeBrownBear)
			populatePronCodes(phoneticDictWordsBrownBear_, pronCodeToObjWellFormed_, duplicatePronCodes);
	}

	std::tuple<bool, const char*> SphinxTrainDataBuilder::buildPhaseSpecificParts(ResourceUsagePhase phase, int minWordPartUsage, int maxUnigramsCount, bool allowPhoneticWordSplit, const std::set<PhoneId>& trainPhoneIds,
		int* dictWordsCount, int* phonesCount)
	{
		// well known dictionaries are WellFormed, Broken and Filler
		auto expandWellKnownPronCodeFun = [phase, this](boost::wstring_ref pronCode) -> const PronunciationFlavour*
		{
			// include broken words in train phase only
			// ignore broken words in test phase
			bool useBrokenDict = phase == ResourceUsagePhase::Train;
			return expandWellKnownPronCode(pronCode, useBrokenDict);
		};

		std::vector<const WordPart*> seedUnigrams;
		chooseSeedUnigrams(phoneticSplitter_, minWordPartUsage, maxUnigramsCount, allowPhoneticWordSplit, phoneReg_, expandWellKnownPronCodeFun, trainPhoneIds, seedUnigrams);
		std::wcout << L"phase=" << (phase == ResourceUsagePhase::Train ? L"train" : L"test")
			<< L" seed unigrams count=" << seedUnigrams.size() << std::endl;

		//
		ArpaLanguageModel langModel;
		langModel.setGramMaxDimensions(2);
		langModel.generate(seedUnigrams, phoneticSplitter_);
		QString fileSuffix = phase == ResourceUsagePhase::Train ? "_train" : "_test";
		writeArpaLanguageModel(langModel, filePath(dbName_ + fileSuffix + ".arpa").toStdWString().c_str());

		// phonetic dictionary
		std::vector<PhoneticWord> phoneticDictWords;
		bool op;
		const char* errMsg;
		std::tie(op, errMsg) = buildPhoneticDictionary(seedUnigrams, expandWellKnownPronCodeFun, phoneticDictWords);
		if (!op)
			return std::make_tuple(false, "Can't build phonetic dictionary");

		if (dictWordsCount != nullptr)
			*dictWordsCount = phoneticDictWords.size();

		std::tie(op, errMsg) = writePhoneticDictSphinx(phoneticDictWords, phoneReg_, filePath(dbName_ + fileSuffix + ".dic"));
		if (!op)
			return std::make_tuple(false, "Can't write phonetic dictionary");

		// create the list of used phones

		PhoneAccumulator phoneAcc;
		phoneAcc.addPhonesFromPhoneticDict(phoneticDictWords);
		phoneAcc.addPhonesFromPhoneticDict(phoneticDictWordsFiller_);

		std::vector<std::string> phoneList;
		phoneAcc.buildPhoneList(phoneReg_, phoneList);

		if (phonesCount != nullptr)
			*phonesCount = phoneList.size();

		// phone list contains all phones used in phonetic dictionary, including SIL
		op = writePhoneList(phoneList, filePath(dbName_ + fileSuffix + ".phone"));
		if (!op)
			return std::make_tuple(true, "Can't write phone list file");

		return std::make_tuple(true, nullptr);
	}

	void SphinxTrainDataBuilder::findWordsWithoutPronunciation(const std::vector<AnnotatedSpeechSegment>& segments, bool useBroken, std::vector<boost::wstring_ref>& unkWords) const
	{
		std::vector<boost::wstring_ref> textWords;

		for (const AnnotatedSpeechSegment& seg : segments)
		{
			const std::wstring& text = seg.TranscriptText;

			textWords.clear();
			splitUtteranceIntoWords(text, textWords);

			std::copy_if(textWords.begin(), textWords.end(), std::back_inserter(unkWords), [this, useBroken](boost::wstring_ref word)
			{
				return !hasPhoneticExpansion(word, useBroken);
			});
		}
	}

	QString SphinxTrainDataBuilder::filePath(const QString& relFilePath) const
	{
		return QString("%1/%2").arg(outDir_.absolutePath()).arg(relFilePath);
	}

	bool SphinxTrainDataBuilder::hasPhoneticExpansion(boost::wstring_ref word, bool useBroken) const
	{
		auto it = pronCodeToObjWellFormed_.find(word);
		if (it != pronCodeToObjWellFormed_.end())
			return true;

		if (useBroken)
		{
			it = pronCodeToObjBroken_.find(word);
			if (it != pronCodeToObjBroken_.end())
				return true;
		}

		it = pronCodeToObjFiller_.find(word);
		if (it != pronCodeToObjFiller_.end())
			return true;

		return false;
	}

	bool SphinxTrainDataBuilder::isBrokenUtterance(boost::wstring_ref text) const
	{
		std::vector<boost::wstring_ref> textWords;
		splitUtteranceIntoWords(text, textWords);

		bool hasBrokenWord = std::any_of(textWords.begin(), textWords.end(), [this](boost::wstring_ref word)
		{
			auto it = pronCodeToObjBroken_.find(word);
			return it != pronCodeToObjBroken_.end();
		});
		return hasBrokenWord;
	}

	const PronunciationFlavour* SphinxTrainDataBuilder::expandWellKnownPronCode(boost::wstring_ref pronCode, bool useBrokenDict) const
	{
		auto it = pronCodeToObjWellFormed_.find(pronCode);
		if (it != pronCodeToObjWellFormed_.end())
			return &it->second;

		if (useBrokenDict)
		{
			it = pronCodeToObjBroken_.find(pronCode);
			if (it != pronCodeToObjBroken_.end())
				return &it->second;
		}

		it = pronCodeToObjFiller_.find(pronCode);
		if (it != pronCodeToObjFiller_.end())
			return &it->second;

		return nullptr;
	}

	bool SphinxTrainDataBuilder::writePhoneList(const std::vector<std::string>& phoneList, const QString& phoneListFile) const
	{
		QFile file(phoneListFile);
		if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
			return false;

		QTextStream txt(&file);
		txt.setCodec("UTF-8");
		txt.setGenerateByteOrderMark(false);
		for (const std::string& ph : phoneList)
		{
			txt << QLatin1String(ph.c_str()) << "\n";
		}
		return true;
	}

	// Reads words, one word per line.
	// Lines with double-pound ## (anywhere in the line) are comments.
	// Text from single pound to the end of line is a comment.
	// Empty lines are ignored.
	bool loadWordList(boost::wstring_ref filePath, std::unordered_set<std::wstring>& words)
	{
		QFile file(toQString(filePath));
		if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
			return false;

		QTextCodec* textCodec = QTextCodec::codecForName("utf-8");
		std::array<char, 1024> lineBuff;
		while (true)
		{
			int readBytes = file.readLine(lineBuff.data(), lineBuff.size());
			if (readBytes == -1) // EOF
				break;
			QString line = textCodec->toUnicode(lineBuff.data(), readBytes);
			if (line.contains(QLatin1String("##"))) // line=comment, ignore entire line
				continue;

			QStringRef lineRef(&line);
			int poundInd = line.indexOf(QLatin1String("#"));
			if (poundInd != -1)
				lineRef = line.leftRef(poundInd);

			lineRef = lineRef.trimmed();
			if (lineRef.isEmpty())
				continue;

			QString word = lineRef.toString();
			words.insert(word.toStdWString());
		}
		return true;
	}

	void SphinxTrainDataBuilder::loadDeclinationDictionary(std::unordered_map<std::wstring, std::unique_ptr<WordDeclensionGroup>>& declinedWordDict)
	{
		auto dictPathArray = {
			//LR"path(C:\devb\PticaGovorunProj\srcrep\build\x64\Debug\tmp2.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk01-а-вапно-вапнований).htm.20150228220948.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk02-вапнований-г≥берел≥н-г≥бернац≥€).htm.20150228213543.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk03-г≥бернац≥€-дипромон≥й-дипрофен).htm.20150228141620.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk04-дипрофен-запорошен≥сть-запорошенн€).htm.20150228220515.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk05-запорошенн€-≥онуванн€-≥онувати).htm.20150301145100.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk06-≥онувати-кластогенний-клатрат).htm.20150302001636.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk07-клатрат-макроцистис-макроцит).htm.20150316142905.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk08-макроцит-м'€шкурити-н).htm.20150325111235.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk09-н-нестравн≥сть-нестратиф≥кований).htm.20150301230606.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk10-нестратиф≥кований-однокенотронний-однокислотний)8.htm.20150301230840.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk11-однокислотний-поконанн€-поконати).htm.20150325000544.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk12-поконати-п'€ть-р).htm.20150408175213.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk13-р-р€цькувати-с).htm.20150407185103.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk14-с-строщити-строювати).htm.20150407185337.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk15-строювати-т€х-у).htm.20150407185443.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk16-у-чхун-ш).htm.20150301231717.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk17-ш-€щур€чий-end).htm.20150301232344.xml)path",
		};

		std::chrono::time_point<Clock> now1 = Clock::now();

		int declinationFileCount = 0;
		for (auto& dictPath : dictPathArray)
		{
			std::tuple<bool, const char*> loadOp = loadUkrainianWordDeclensionXml(dictPath, declinedWordDict);
			if (!std::get<0>(loadOp))
			{
				std::cerr << std::get<1>(loadOp) << std::endl;
				return;
			}
			OutputDebugStringW(dictPath);
			OutputDebugStringW(L"\n");
			declinationFileCount++;
			if (false && declinationFileCount <= 3)
				break;
		}
		std::chrono::time_point<Clock> now2 = Clock::now();
		auto elapsedSec = std::chrono::duration_cast<std::chrono::seconds>(now2 - now1).count();
		std::wcout << L"loaded declination dict in " << elapsedSec << L"s" << std::endl;
	}

	void SphinxTrainDataBuilder::loadPhoneticSplitter(const std::unordered_map<std::wstring, std::unique_ptr<WordDeclensionGroup>>& declinedWordDict, UkrainianPhoneticSplitter& phoneticSplitter)
	{
		wchar_t* processedWordsFile = LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk-done.txt)path";
		std::unordered_set<std::wstring> processedWords;
		if (!loadWordList(boost::wstring_ref(processedWordsFile), processedWords))
		{
			errMsg_ = "Can't load 'uk-done.txt' dictionary";
			return;
		}

		std::wstring targetWord;

		std::chrono::time_point<Clock> now1 = Clock::now();
		phoneticSplitter.bootstrap(declinedWordDict, targetWord, processedWords);
		std::chrono::time_point<Clock> now2 = Clock::now();
		auto elapsedSec = std::chrono::duration_cast<std::chrono::seconds>(now2 - now1).count();
		std::wcout << L"phonetic split of declinartion dict took=" << elapsedSec << L"s" << std::endl;

		int uniqueDeclWordsCount = uniqueDeclinedWordsCount(declinedWordDict);
		std::wcout << L"wordGroupsCount=" << declinedWordDict.size() << L" uniqueDeclWordsCount=" << uniqueDeclWordsCount << std::endl;

		WordsUsageInfo& wordUsage = phoneticSplitter.wordUsage();
		double uniquenessRatio = wordUsage.wordPartsCount() / (double)uniqueDeclWordsCount;
		std::wcout << L" wordParts=" << wordUsage.wordPartsCount()
			<< L" uniquenessRatio=" << uniquenessRatio
			<< std::endl;

		//
		const wchar_t* txtDir = LR"path(C:\devb\PticaGovorunProj\data\textWorld\)path";
		long totalPreSplitWords = 0;

		now1 = Clock::now();
		phoneticSplitter.gatherWordPartsSequenceUsage(txtDir, totalPreSplitWords, -1, false);
		now2 = Clock::now();
		elapsedSec = std::chrono::duration_cast<std::chrono::seconds>(now2 - now1).count();
		std::wcout << L"gatherWordPartsSequenceUsage took=" << elapsedSec << L"s" << std::endl;

		std::array<long, 2> wordSeqSizes;
		wordUsage.wordSeqCountPerSeqSize(wordSeqSizes);
		for (int seqDim = 0; seqDim < wordSeqSizes.size(); ++seqDim)
		{
			long wordsPerNGram = wordSeqSizes[seqDim];
			std::wcout << L"wordsPerNGram[" << seqDim + 1 << "]=" << wordsPerNGram << std::endl;
		}
		std::wcout << L"	wordParts=" << wordUsage.wordPartsCount() << std::endl;

		phoneticSplitter.printSuffixUsageStatistics();
	}

	std::tuple<bool, const char*> SphinxTrainDataBuilder::buildPhoneticDictionary(const std::vector<const WordPart*>& seedUnigrams, 
		std::function<auto (boost::wstring_ref pronCode) -> const PronunciationFlavour*> expandWellKnownPronCode,
		std::vector<PhoneticWord>& phoneticDictWords)
	{
		const wchar_t* stressDict = LR"path(C:\devb\PticaGovorunProj\data\stressDictUk.xml)path";
		std::unordered_map<std::wstring, int> wordToStressedSyllable;
		bool op;
		const char* errMsg;
		std::tie(op, errMsg) = loadStressedSyllableDictionaryXml(stressDict, wordToStressedSyllable);
		if (!op)
			return std::make_tuple(false, errMsg);

		auto getStressedSyllableIndFun = [&wordToStressedSyllable](boost::wstring_ref word, std::vector<int>& stressedSyllableInds) -> bool
		{
			auto it = wordToStressedSyllable.find(std::wstring(word.data(), word.size()));
			if (it == wordToStressedSyllable.end())
				return false;
			stressedSyllableInds.push_back(it->second);
			return true;
		};

		WordPhoneticTranscriber phoneticTranscriber;
		phoneticTranscriber.setStressedSyllableIndFun(getStressedSyllableIndFun);

		auto expandPronFun = [this, &phoneticTranscriber, expandWellKnownPronCode](boost::wstring_ref pronCode, PronunciationFlavour& result) -> bool
		{
			const PronunciationFlavour* pron = expandWellKnownPronCode(pronCode);
			if (pron != nullptr)
			{
				result = *pron;
				return true;
			}

			phoneticTranscriber.transcribe(phoneReg_, toStdWString(pronCode));
			if (phoneticTranscriber.hasError())
				return false;
			
			result.PronCode = pronCode;
			phoneticTranscriber.copyOutputPhoneIds(result.Phones);
			return true;
		};

		std::tie(op,errMsg) = createPhoneticDictionaryFromUsedWords(seedUnigrams, phoneticSplitter_, expandPronFun, phoneticDictWords);
		if (!op)
			return std::make_tuple(false, "Can't create phonetic dictionary from words usage");

		return std::make_tuple(true, nullptr);
	}

	std::tuple<bool, const char*> SphinxTrainDataBuilder::createPhoneticDictionaryFromUsedWords(wv::slice<const WordPart*> seedWordParts, const UkrainianPhoneticSplitter& phoneticSplitter,
		std::function<auto (boost::wstring_ref, PronunciationFlavour&) -> bool> expandPronCodeFun,
		std::vector<PhoneticWord>& phoneticDictWords)
	{
		std::wstring wordStr;
		for (const WordPart* wordPart : seedWordParts)
		{
			if (wordPart == phoneticSplitter.sentStartWordPart() || wordPart == phoneticSplitter.sentEndWordPart())
				continue;

			toStringWithTilde(*wordPart, wordStr);

			boost::wstring_ref wordRef;
			if (!registerWord(boost::wstring_ref(wordStr), *stringArena_, wordRef))
				return std::make_tuple(false, "Can't register word in arena");
			
			PronunciationFlavour pron;
			if (!expandPronCodeFun(wordPart->partText(),pron))
				return std::make_tuple(false, "Can't get pronunciation for word");

			PhoneticWord word;
			word.Word = wordRef;
			word.Pronunciations.push_back(pron);
			phoneticDictWords.push_back(word);
		}
		return std::make_tuple(true, nullptr);
	}

	void SphinxTrainDataBuilder::loadAudioAnnotation(const wchar_t* wavRootDir, const wchar_t* annotRootDir, const wchar_t* wavDirToAnalyze, bool includeBrownBear)
	{
		// load audio segments
		auto segPredBeforeFun = [includeBrownBear](const AnnotatedSpeechSegment& seg) -> bool
		{
			if (seg.TranscriptText.find(L"<unk>") != std::wstring::npos ||
				seg.TranscriptText.find(L"<air>") != std::wstring::npos ||
				seg.TranscriptText.find(L"[clk]") != std::wstring::npos)
				return false;
			
			// noisy speech
			if (seg.ContentMarker.SpeakerBriefId == L"pynzenykvm" ||
				seg.ContentMarker.SpeakerBriefId == L"grytsenkopj" ||
				seg.ContentMarker.SpeakerBriefId == L"chemerkinsh" ||
				seg.ContentMarker.SpeakerBriefId == L"sokolovaso" ||
				seg.ContentMarker.SpeakerBriefId == L"romanukjv" ||
				seg.ContentMarker.SpeakerBriefId == L"shapovalji" ||
				seg.ContentMarker.SpeakerBriefId == L"dergachdv")
				return false;
			if (!includeBrownBear && seg.ContentMarker.SpeakerBriefId == L"BrownBear1")
				return false;

			if (seg.TranscriptText.find(L"#") != std::wstring::npos) // segments to ignore
				return false;
			if (seg.Language != SpeechLanguage::Ukrainian)
			{
				if (seg.TranscriptText.compare(fillerInhale().data()) == 0) // lang(inhale)=null, but we allow it
					return true;

				return false;
			}
			return true;
		};

		bool loadOp;
		const char* errMsg;
		std::tie(loadOp, errMsg) = loadSpeechAndAnnotation(QFileInfo(QString::fromWCharArray(wavDirToAnalyze)), wavRootDir, annotRootDir, MarkerLevelOfDetail::Word, false, segPredBeforeFun, segments_);
		if (!loadOp)
		{
			errMsg_ = "Can't load audio and annotation";
			return;
		}
	}

	std::tuple<bool, const char*> SphinxTrainDataBuilder::partitionTrainTestData(const std::vector<AnnotatedSpeechSegment>& segments, double trainCasesRatio, bool swapTrainTestData, bool useBrokenPronsInTrainOnly, int randSeed, std::vector<details::AssignedPhaseAudioSegment>& outSegRefs, std::set<PhoneId>& trainPhoneIds) const
	{
		// partition train-test data
		std::vector<std::reference_wrapper<const AnnotatedSpeechSegment>> rejectedSegments;
		std::mt19937 g(randSeed);
		std::uniform_real_distribution<float> rnd(0, 1);
		for (const AnnotatedSpeechSegment& seg : segments)
		{
			bool isAlwaysTest = seg.ContentMarker.ExcludePhase == ResourceUsagePhase::Train;
			bool isAlwaysTrain = seg.ContentMarker.ExcludePhase == ResourceUsagePhase::Test;

			bool broken = isBrokenUtterance(seg.TranscriptText);
			if (useBrokenPronsInTrainOnly)
			{
				isAlwaysTrain |= broken;
			}
			// #includeBrownBear
			//isAlwaysTrain |= seg.ContentMarker.SpeakerBriefId == L"BrownBear1";

			if (isAlwaysTrain && isAlwaysTest)
			{
				// eg when excluded from training segment has broken words
				rejectedSegments.push_back(std::ref(seg));
				continue;
			}

			auto segRef = details::AssignedPhaseAudioSegment();
			segRef.Seg = &seg;

			if (isAlwaysTest)
				segRef.Phase = ResourceUsagePhase::Test;
			else if (isAlwaysTrain)
				segRef.Phase = ResourceUsagePhase::Train;
			else
			{
				PG_Assert(!isAlwaysTrain && !isAlwaysTest);

				float val = rnd(g);
				ResourceUsagePhase phase = val < trainCasesRatio ? ResourceUsagePhase::Train : ResourceUsagePhase::Test;

				if (swapTrainTestData)
				{
					if (phase == ResourceUsagePhase::Train)
						phase = ResourceUsagePhase::Test;
					else if (phase == ResourceUsagePhase::Test)
						phase = ResourceUsagePhase::Train;
				}

				segRef.Phase = phase;
			}
			outSegRefs.push_back(segRef);
		}
		if (!rejectedSegments.empty()) // TODO: print rejected segment: annot file path, segment IDs
			std::wcout << L"Rejecting " << rejectedSegments.size() << L" segments" << std::endl; // info

		return putSentencesWithRarePhonesInTrain(outSegRefs, trainPhoneIds);
	}

	// Fix train/test split so that if a (rare) phone exist only in one data set, then this is the Test data set.
	// This is necessary for the rare phones to end up in the train data set. Otherwise Sphinx emits warning and errors:
	// WARNING: "accum.c", line 633: The following senones never occur in the input data...
	// WARNING: "main.c", line 145: No triphones involving DZ1
	std::tuple<bool, const char*> SphinxTrainDataBuilder::putSentencesWithRarePhonesInTrain(std::vector<details::AssignedPhaseAudioSegment>& segments, std::set<PhoneId>& trainPhoneIds) const
	{
		PhoneAccumulator trainAcc;
		PhoneAccumulator testAcc;
		std::vector<boost::wstring_ref> pronCodes;
		for (const auto& segRef : segments)
		{
			PhoneAccumulator& acc = segRef.Phase == ResourceUsagePhase::Train ? trainAcc : testAcc;
			
			pronCodes.clear();
			splitUtteranceIntoWords(segRef.Seg->TranscriptText, pronCodes);
			
			for (auto pronCode : pronCodes)
			{
				const PronunciationFlavour* pron = expandWellKnownPronCode(pronCode, true);
				if (pron == nullptr)
					return std::make_tuple(false, "Can't map pronCode into phonelist");
				acc.addPhones(pron->Phones);
			}
		}

		//
		trainAcc.copyPhoneList(putInsert(trainPhoneIds));
		
		std::vector<PhoneId> trainPhones;
		std::vector<PhoneId> testPhones;
		trainAcc.copyPhoneList(putBack(trainPhones));
		testAcc.copyPhoneList(putBack(testPhones));

		// phones which exist only in test portion but not in train
		std::vector<PhoneId> testOnlyPhoneIds;
		std::sort(trainPhones.begin(), trainPhones.end());
		std::sort(testPhones.begin(), testPhones.end());
		std::set_difference(testPhones.begin(), testPhones.end(), trainPhoneIds.begin(), trainPhoneIds.end(), std::back_inserter(testOnlyPhoneIds));

		//
		auto sentHasRarePhone = [&pronCodes, this](boost::wstring_ref text, const std::vector<PhoneId>& rarePhoneIds) -> bool
		{
			pronCodes.clear();
			splitUtteranceIntoWords(text, pronCodes);

			return std::any_of(pronCodes.begin(), pronCodes.end(), [this, &rarePhoneIds](boost::wstring_ref pronCode) -> bool
			{
				const PronunciationFlavour* pron = expandWellKnownPronCode(pronCode, true);
				PG_Assert2(pron != nullptr, "PronCode can't be expanded into phones, but has been successfully expanded before");
				
				return std::any_of(pron->Phones.begin(), pron->Phones.end(), [&rarePhoneIds](PhoneId phId) -> bool
				{
					return std::find(rarePhoneIds.begin(), rarePhoneIds.end(), phId) != rarePhoneIds.end();
				});
			});
		};

		// put sentences with testOnly (rare) phones into train portion
		int testToTrainMove = 0;
		for (PhoneId phoneId : testOnlyPhoneIds)
		{
			for (auto& segRef : segments)
			{
				if (segRef.Phase != ResourceUsagePhase::Test)
					continue;
				if (sentHasRarePhone(segRef.Seg->TranscriptText, testOnlyPhoneIds))
				{
					segRef.Phase = ResourceUsagePhase::Train;
					testToTrainMove++;
				}
			}
		}
		if (testToTrainMove > 0)
			std::wcout << L"Moved " << testToTrainMove << L" sentences with rare phones to Train portion" << std::endl; // info

		return std::make_tuple(true, nullptr);
	}


	QString generateSegmentFileNameNoExt(const QString& baseFileName, const AnnotatedSpeechSegment& seg)
	{
		QLatin1Char filler('0');
		int width = 5;
		return QString("%1_%2-%3").arg(baseFileName).arg(seg.StartMarker.Id, width, 10, filler).arg(seg.EndMarker.Id, width, 10, filler);
	}

	// audioSrcRootDir   =C:\PG\data\SpeechAudio\
	// wavSrcFilePathAbs =C:\PG\data\SpeechAudio\ncru1-slovo\22229.WAV
	// wavBaseOutDir     =C:\PG\SphinxTrain\out123\wav\
	// wavDirForRelPathes=C:\PG\SphinxTrain\out123\wav\persian_train\
	// Output:
	// SegFileNameNoExt  =                                                       22229_00273-00032
	// WavOutRelFilePathNoExt=                         persian_train\ncru1-slovo\22229_00273-00032
	// WavOutFilePathNExt=C:\PG\SphinxTrain\out123\wav\persian_train\ncru1-slovo\22229_00273-00032
	details::AudioFileRelativePathComponents audioFilePathComponents(
		const QString& audioSrcRootDirPath,
		const QString& wavSrcFilePathAbs, const AnnotatedSpeechSegment& seg,
		const QString& wavBaseOutDirPath,
		const QString& wavDirPathForRelPathes)
	{
		QString wavFilePathRel = QDir(audioSrcRootDirPath).relativeFilePath(wavSrcFilePathAbs);

		// abs path to out wav
		QString wavOutAbsFilePath = QDir(wavDirPathForRelPathes).absoluteFilePath(wavFilePathRel);

		// rel path to out wav
		auto wavBaseOutDir = QDir(wavBaseOutDirPath);
		QString wavOutRelFilePath = wavBaseOutDir.relativeFilePath(wavOutAbsFilePath);

		// Sphinx FileId file name must have no extension, forward slashes
		QString segFileNameNoExt = generateSegmentFileNameNoExt(QFileInfo(wavOutRelFilePath).completeBaseName(), seg);
		QString wavOutRelFilePathNoExt = QFileInfo(wavOutRelFilePath).dir().filePath(segFileNameNoExt);
		QString wavOutFilePath = wavBaseOutDir.filePath(wavOutRelFilePathNoExt);

		details::AudioFileRelativePathComponents result;
		result.SegFileNameNoExt = segFileNameNoExt;
		result.WavOutRelFilePathNoExt = wavOutRelFilePathNoExt;
		result.AudioSegFilePathNoExt = wavOutFilePath;
		return result;
	}

	void SphinxTrainDataBuilder::fixWavSegmentOutputPathes(
		const QString& audioSrcRootDirPath,
		const QString& wavBaseOutDirPath,
		ResourceUsagePhase targetPhase,
		const QString& wavDirPathForRelPathes,
		std::vector<details::AssignedPhaseAudioSegment>& outSegRefs)
	{
		for (details::AssignedPhaseAudioSegment& segRef : outSegRefs)
		{
			const AnnotatedSpeechSegment& seg = *segRef.Seg;
			if (segRef.Phase != targetPhase)
				continue;
			QString wavFilePath = QString::fromStdWString(seg.FilePath);
			segRef.OutAudioSegPathParts = audioFilePathComponents(audioSrcRootDirPath, wavFilePath, seg, wavBaseOutDirPath, wavDirPathForRelPathes);
		}
	}
	
	bool SphinxTrainDataBuilder::writeFileIdAndTranscription(const std::vector<details::AssignedPhaseAudioSegment>& segsRefs, ResourceUsagePhase targetPhase,
		const QString& fileIdsFilePath, const QString& transcriptionFilePath) const
	{
		QFile fileFileIds(fileIdsFilePath);
		if (!fileFileIds.open(QIODevice::WriteOnly | QIODevice::Text))
			return false;

		QFile fileTranscription(transcriptionFilePath);
		if (!fileTranscription.open(QIODevice::WriteOnly | QIODevice::Text))
			return false;
		
		QTextStream txtFileIds(&fileFileIds);
		txtFileIds.setCodec("UTF-8");
		txtFileIds.setGenerateByteOrderMark(false);

		QTextStream txtTranscription(&fileTranscription);
		txtTranscription.setCodec("UTF-8");
		txtTranscription.setGenerateByteOrderMark(false);

		QString startSilQ = toQString(fillerStartSilence());
		QString endSilQ = toQString(fillerEndSilence());

		for (const details::AssignedPhaseAudioSegment& segRef : segsRefs)
		{
			if (segRef.Phase != targetPhase)
				continue;

			// fileIds
			txtFileIds << segRef.OutAudioSegPathParts.WavOutRelFilePathNoExt << "\n";

			// determine if segment requires padding with silence
			bool startsSil = segRef.Seg->TranscriptionStartsWithSilence;
			bool endsSil = segRef.Seg->TranscriptionEndsWithSilence;

			// transcription
			if (targetPhase == ResourceUsagePhase::Train && !startsSil)
				txtTranscription << startSilQ << " ";
			txtTranscription << QString::fromStdWString(segRef.Seg->TranscriptText) <<" ";
			if (targetPhase == ResourceUsagePhase::Train && !endsSil)
				txtTranscription << endSilQ << " ";
			txtTranscription << "(" << segRef.OutAudioSegPathParts.SegFileNameNoExt << ")" << "\n";
		}
		return true;
	}

	std::tuple<bool, const char*>  SphinxTrainDataBuilder::loadSilenceSegment(std::vector<short>& frames, float framesFrameRate) const
	{
		std::vector<short> silenceFrames;
		float silenceFrameRate = -1;
		std::string silenceWavPath = AppHelpers::mapPathStdString("data/Sphinx2/SpeechModels/pynzenyk-background-200ms.wav");
		auto readOp = readAllSamples(silenceWavPath.c_str(), silenceFrames, &silenceFrameRate);
		if (!std::get<0>(readOp))
			return std::make_tuple(false, "Can't read silence wav file");
		PG_DbgAssert(silenceFrameRate != -1);

		bool op;
		const char* errMsg;
		std::tie(op,errMsg) = resampleFrames(silenceFrames, silenceFrameRate, framesFrameRate, frames);
		if (!op)
			return std::make_tuple(false, "Can't resample silence frames");

		return std::make_tuple(true, nullptr);
	}

	void SphinxTrainDataBuilder::buildWavSegments(const std::vector<details::AssignedPhaseAudioSegment>& segsRefs, float targetFrameRate, bool padSilence, const std::vector<short>& silenceFrames)
	{
		// group segmets by source wav file, so wav files are read sequentially

		typedef std::vector<const details::AssignedPhaseAudioSegment*> VecPerFile;
		std::map<std::wstring, VecPerFile> srcFilePathToSegs;

		for (const details::AssignedPhaseAudioSegment& segRef : segsRefs)
		{
			auto it = srcFilePathToSegs.find(segRef.Seg->FilePath);
			if (it != srcFilePathToSegs.end())
				it->second.push_back(&segRef);
			else
				srcFilePathToSegs.insert({ segRef.Seg->FilePath, VecPerFile{ &segRef } });
		}

		// audio frames are lazy loaded
		float srcAudioFrameRate = -1;
		std::vector<short> srcAudioFrames;
		std::vector<short> segFramesResamp;
		std::vector<short> paddedSegFramesOut;

		for (const auto& pair : srcFilePathToSegs)
		{
			const std::wstring& wavFilePath = pair.first;
			const VecPerFile& segs = pair.second;
			
			// load wav file
			std::string srcAudioPath = QString::fromStdWString(wavFilePath).toStdString();
			std::wcout << L"wav=" << wavFilePath << std::endl;

			srcAudioFrames.clear();
			auto readOp = readAllSamplesFormatAware(srcAudioPath.c_str(), srcAudioFrames, &srcAudioFrameRate);
			if (!std::get<0>(readOp))
			{
				errMsg_ = "Can't read audio file. ";
				errMsg_ += std::get<1>(readOp);
				return;
			}

			std::vector<boost::wstring_ref> segPronCodes;
			std::vector<wchar_t> pronCodeBuff;

			// generate each audio segments
			for (const details::AssignedPhaseAudioSegment* segRef : segs)
			{
				const AnnotatedSpeechSegment& seg =  *segRef->Seg;

				// since each part may go to train or test output, ensure that output dir exist
				auto segOutDir = QFileInfo(segRef->OutAudioSegPathParts.AudioSegFilePathNoExt).dir();
				if (!segOutDir.exists())
					segOutDir.mkpath(".");

				//
				long segLen = seg.EndMarker.SampleInd - seg.StartMarker.SampleInd;
				wv::slice<short> segFrames = wv::make_view(srcAudioFrames.data() + seg.StartMarker.SampleInd, segLen);;

				bool op;

				bool requireResampling = srcAudioFrameRate != targetFrameRate;
				if (requireResampling)
				{
					const char* errMsg;
					std::tie(op, errMsg) = resampleFrames(segFrames, srcAudioFrameRate, targetFrameRate, segFramesResamp);
					if (!op)
					{
						errMsg_ = QString("Can't resample fraemes. %1").arg(errMsg);
						return;
					}
					segFrames = segFramesResamp;
				}

				// determine whether to pad the utterance with silence
				bool startsSil = seg.AudioStartsWithSilence;
				bool endsSil = seg.AudioEndsWithSilence;

				// pad frames with silence
				paddedSegFramesOut.clear();

				const float minSilLenMs = 100;
				long minSilLenFrames = static_cast<long>(minSilLenMs / 1000 * srcAudioFrameRate);
				
				if (padSilence && !startsSil)
				//if (padSilence && seg.StartSilenceFramesCount < minSilLenFrames)
					std::copy(silenceFrames.begin(), silenceFrames.end(), std::back_inserter(paddedSegFramesOut));

				std::copy(segFrames.begin(), segFrames.end(), std::back_inserter(paddedSegFramesOut));

				if (padSilence && !endsSil)
				//if (padSilence && seg.EndSilenceFramesCount < minSilLenFrames)
					std::copy(silenceFrames.begin(), silenceFrames.end(), std::back_inserter(paddedSegFramesOut));

				// statistics
				double durSec = paddedSegFramesOut.size() / (double)targetFrameRate; // speech + padded silence
				double& durCounter = segRef->Phase == ResourceUsagePhase::Train ? audioDurationSecTrain_ : audioDurationSecTest_;
				durCounter += durSec;

				double noPadDurSec = segFrames.size() / (double)targetFrameRate; // only speech (no padded silence)
				double& noPadDurCounter = segRef->Phase == ResourceUsagePhase::Train ? audioDurationNoPaddingSecTrain_ : audioDurationNoPaddingSecTest_;
				noPadDurCounter += noPadDurSec;

				speakerIdToAudioDurSec_[seg.ContentMarker.SpeakerBriefId] += durSec;

				// write output wav segment

				std::string wavSegOutPath = (segRef->OutAudioSegPathParts.AudioSegFilePathNoExt + ".wav").toStdString();
				std::string errMsg;
				std::tie(op, errMsg) = writeAllSamplesWav(paddedSegFramesOut.data(), (int)paddedSegFramesOut.size(), wavSegOutPath, targetFrameRate);
				if (!op)
				{
					errMsg_ = QString("Can't write output wav segment. %1").arg(errMsg.c_str());
					return;
				}
			}
		}
	}

	void SphinxTrainDataBuilder::generateDataStat(const std::vector<details::AssignedPhaseAudioSegment>& phaseAssignedSegs)
	{
		if (!errMsg_.isEmpty()) return;
		std::vector<boost::wstring_ref> words;
		for (const auto& segRef : phaseAssignedSegs)
		{
			int& utterCounter = segRef.Phase == ResourceUsagePhase::Train ? utterCountTrain_ : utterCountTest_;
			utterCounter += 1;

			words.clear();
			splitUtteranceIntoWords(segRef.Seg->TranscriptText, words);

			int& wordCounter = segRef.Phase == ResourceUsagePhase::Train ? wordCountTrain_ : wordCountTest_;
			wordCounter += (int)words.size();
		}
	}

	void SphinxTrainDataBuilder::printDataStat(QDateTime genDate, const std::map<std::string, QVariant> speechModelConfig, const QString& statFilePath)
	{
		if (!errMsg_.isEmpty()) return;

		QFile outFile(statFilePath);
		if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text))
		{
			errMsg_ = QString("Can't create file (%1)").arg(statFilePath);
			return;
		}

		QTextStream outStream(&outFile);
		outStream.setCodec("UTF-8");
		outStream.setGenerateByteOrderMark(false);

		const int TimePrec = 4;

		QString curDateStr = genDate.toString("dd.MM.yyyy HH:mm::ss");
		outStream << QString("generationDate=%1").arg(curDateStr) << "\n";
		outStream << QString("generationDurH=%1").arg(dataGenerationDurSec_ / 3600.0, 0, 'f', TimePrec) << "\n";

		for (const auto& pair : speechModelConfig)
		{
			outStream << QString::fromStdString(pair.first) << "=" << pair.second.toString() << "\n";
		}

		outStream << "\n";
		outStream << QString("audioDurH(Train, Test)=(%1, %2)").arg(audioDurationSecTrain_ / 3600, 0, 'f', TimePrec).arg(audioDurationSecTest_ / 3600, 0, 'f', TimePrec) << "\n";
		outStream << QString("audioDurNoPaddingH=(%1, %2)").arg(audioDurationNoPaddingSecTrain_ / 3600, 0, 'f', TimePrec).arg(audioDurationNoPaddingSecTest_ / 3600, 0, 'f', TimePrec) << "\n";
		outStream << QString("utterCount=(%1, %2)").arg(utterCountTrain_).arg(utterCountTest_) << "\n";
		outStream << QString("wordCount=(%1, %2)").arg(wordCountTrain_).arg(wordCountTest_) << "\n";
		outStream << QString("distSize=(%1, %2)").arg(dictWordsCountTrain_).arg(dictWordsCountTest_) << "\n";
		outStream << QString("phonesCount=(%1, %2)").arg(phonesCountTrain_).arg(phonesCountTest_) << "\n";

		// audio duration per speaker, order hi-lo
		std::vector<std::wstring> speakerIds;
		std::transform(std::begin(speakerIdToAudioDurSec_), std::end(speakerIdToAudioDurSec_), std::back_inserter(speakerIds),
			[](const std::pair<std::wstring, double>& pair) { return pair.first; });
		std::sort(std::begin(speakerIds), std::end(speakerIds),
			[&](const std::wstring& a, const std::wstring& b) { return speakerIdToAudioDurSec_[a] > speakerIdToAudioDurSec_[b]; });

		outStream << "\n#speakerId hours (hi to lo)" << "\n";
		for (const std::wstring& speakerId : speakerIds)
		{
			double durSec = speakerIdToAudioDurSec_[speakerId];
			outStream << QString("%1 %2").arg(toQString(speakerId)).arg(durSec / 3600, 0, 'f', TimePrec) << "\n";
		}
	}

	bool loadSphinxAudio(boost::wstring_ref audioDir, const std::vector<std::wstring>& audioRelPathesNoExt, boost::wstring_ref audioFileSuffix, std::vector<AudioData>& audioDataList)
	{
		QString ext = toQString(audioFileSuffix);
		auto audioDirQ = QDir(toQString(audioDir));

		bool readOp;
		std::string errMsg;
		for (const std::wstring& audioPathNoExt : audioRelPathesNoExt)
		{
			QString absPath = audioDirQ.filePath(QString("%1.%2").arg(toQString(audioPathNoExt)).arg(ext));

			AudioData audioData;

			std::tie(readOp, errMsg) = readAllSamples(absPath.toStdString(), audioData.Frames, &audioData.FrameRate);
			if (!readOp)
				return false;
			audioDataList.push_back(std::move(audioData));
		}
		return true;
	}

	std::tuple<bool, const char*> writePhoneticDictSphinx(const std::vector<PhoneticWord>& phoneticDictWords, const PhoneRegistry& phoneReg, const QString& filePath)
	{
		QFile file(filePath);
		if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
			return std::make_tuple(false, "Can't open file for writing");

		QTextStream txt(&file);
		txt.setCodec("UTF-8");
		txt.setGenerateByteOrderMark(false);

		for (const PhoneticWord& word : phoneticDictWords)
		{
			for (const PronunciationFlavour& pron : word.Pronunciations)
			{
				std::string phoneListStr;
				bool pronToStrOp = phoneListToStr(phoneReg, pron.Phones, phoneListStr);
				if (!pronToStrOp)
					return std::make_tuple(false, "Can't convert phone list to string");

				txt <<toQString(pron.PronCode) << "\t" << QLatin1String(phoneListStr.c_str()) << "\n";
			}
		}
		return std::make_tuple(true, nullptr);
	}

	bool readSphinxFileFileId(boost::wstring_ref fileIdPath, std::vector<std::wstring>& fileIds)
	{
		QFile file(toQString(fileIdPath));
		if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
			return false;

		QTextStream stream(&file);
		while (!stream.atEnd())
		{
			QString fileId = stream.readLine();
			fileIds.push_back(fileId.toStdWString());
		}
		return true;
	}

	bool readSphinxFileTranscription(boost::wstring_ref transcriptionFilePath, std::vector<SphinxTranscriptionLine>& transcriptions)
	{
		QFile file(toQString(transcriptionFilePath));
		if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
			return false;

		QTextStream stream(&file);
		stream.setCodec("UTF-8");
		while (!stream.atEnd())
		{
			QString line = stream.readLine();
			int openBracketInd = line.lastIndexOf(QChar('('));
			if (openBracketInd == -1)
				return false;

			// move to the *last* group separated by underscore
			int closeBracketInd = line.indexOf(QChar(')'), openBracketInd);
			if (closeBracketInd == -1)
				return false;

			// -1 to account for space after transcription and before the open bracket
			QString transcr = line.left(openBracketInd - 1);

			QString fileId = line.mid(openBracketInd + 1, closeBracketInd - openBracketInd - 1);

			SphinxTranscriptionLine part;
			part.Transcription = transcr.toStdWString();
			part.FileNameWithoutExtension = fileId.toStdWString();
			transcriptions.push_back(part);
		}
		return true;
	}

	bool checkFileIdTranscrConsistency(const std::vector<std::wstring>& dataFilePathNoExt, const std::vector<SphinxTranscriptionLine>& dataTranscr)
	{
		if (dataFilePathNoExt.size() != dataTranscr.size())
			return false;

		for (size_t i = 0; i < dataFilePathNoExt.size(); ++i)
		{
			const std::wstring& filePathNoExt = dataFilePathNoExt[i];
			const SphinxTranscriptionLine& transcrInfo = dataTranscr[i];

			// TODO: substring is not enough
			// here we actually need to extract exact file name without ext from filePath and compare with fileId from transcription
			if (filePathNoExt.find(transcrInfo.FileNameWithoutExtension) == std::wstring::npos)
				return false;
		}
		return true;
	};
}
