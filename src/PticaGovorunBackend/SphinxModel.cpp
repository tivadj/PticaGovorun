#include "stdafx.h"
#include "SphinxModel.h"
#include <vector>
#include <random>
#include <QDir>
#include "SpeechProcessing.h"
#include "PhoneticService.h"
#include "WavUtils.h"
#include "ClnUtils.h"
#include "ArpaLanguageModel.h"


namespace PticaGovorun
{
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

			long wordTimesUsed = -1;
			const WordPart* wordPart = wordUsage.wordPartByValue(wordW, WordPartSide::WholeWord);
			if (wordPart != nullptr)
			{
				WordSeqKey key = { wordPart->id() };
				wordTimesUsed = wordUsage.getWordSequenceUsage(key);
			}

			for (const PronunciationFlavour& pron : word.Pronunciations)
			{
				bool wasAdded = false;
				const WordPart* pronPart = wordUsage.getOrAddWordPart(toStdWString(pron.PronCode), WordPartSide::WholeWord, &wasAdded);
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
					PG_Assert(pronPart != nullptr && "PronIds were added in step1");
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
			// include <s>, </s> and ~Right parts
			if (wordPart == phoneticSplitter.sentStartWordPart() ||
				wordPart == phoneticSplitter.sentEndWordPart() ||
				(allowPhoneticWordSplit && wordPart->partSide() == WordPartSide::RightPart))
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
		dbName_ = "persian";

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

		bool includeBrownBear = false;
		bool outputWav = false;
		bool outputPhoneticDictAndLangModel = true;
		bool padSilence = true; // pad the audio segment with the silence segment
		bool allowSoftHardConsonant = true;
		bool allowVowelStress = true;
		PalatalSupport palatalSupport = PalatalSupport::AsHard;
		bool useBrokenPronsInTrainOnly = true;
		const double trainCasesRatio = 0.7;
		const float outFrameRate = 16000; // Sphinx requires 16k
		// note, the positive threshold may evict the rare words
		int maxWordPartUsage = 10;
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
		const wchar_t* speechWavRootDir = LR"path(C:\devb\PticaGovorunProj\srcrep\data\SpeechAudio\)path";
		const wchar_t* speechAnnotRootDir = LR"path(C:\devb\PticaGovorunProj\srcrep\data\SpeechAnnot\)path";
		const wchar_t* wavDirToAnalyze = LR"path(C:\devb\PticaGovorunProj\srcrep\data\SpeechAudio\)path";

		loadAudioAnnotation(speechWavRootDir, speechAnnotRootDir, wavDirToAnalyze, includeBrownBear);

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

		std::vector<details::AssignedPhaseAudioSegment> phaseAssignedSegs;
		std::set<PhoneId> trainPhoneIds;
		const char* errMsg;
		std::tie(op, errMsg) = partitionTrainTestData(segments_, trainCasesRatio, useBrokenPronsInTrainOnly, phaseAssignedSegs, trainPhoneIds);
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

			// ensure that LM covers all pronunciations in phonetic dictionary
			inferPhoneticDictPronIdsUsage(phoneticSplitter_, phoneticDictWellFormed_);
			inferPhoneticDictPronIdsUsage(phoneticSplitter_, phoneticDictBroken_);

			// .dic and .arpa files are different
			// broken words goes in dict and lang model in train phase only
			// alternatively we can always ignore broken words, but it is a pity to not use the markup
			std::tie(op, errMsg) = buildPhaseSpecificParts(ResourceUsagePhase::Train, maxWordPartUsage, maxUnigramsCount, allowPhoneticWordSplit, trainPhoneIds);
			if (!op)
			{
				errMsg_ = QString("Can't write phonetic dictionary file. %1").arg(errMsg);
				return;
			}
			std::tie(op, errMsg) = buildPhaseSpecificParts(ResourceUsagePhase::Test, maxWordPartUsage, maxUnigramsCount, allowPhoneticWordSplit, trainPhoneIds);
			if (!op)
			{
				errMsg_ = QString("Can't write phonetic dictionary file. %1").arg(errMsg);
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

		auto audioSrcRootDir = QString::fromWCharArray(speechWavRootDir);

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
	}

	void SphinxTrainDataBuilder::loadKnownPhoneticDicts(bool includeBrownBear)
	{
		// load phonetic vocabularies
		bool loadOp;
		const char* errMsg = nullptr;
		const wchar_t* persianDictPathKnown = LR"path(C:\devb\PticaGovorunProj\srcrep\data\phoneticDictUkKnown.xml)path";
		std::tie(loadOp, errMsg) = loadPhoneticDictionaryXml(persianDictPathKnown, phoneReg_, phoneticDictWordsWellFormed_, *stringArena_);
		if (!loadOp)
		{
			errMsg_ = QString("Can't load phonetic dictionary. %1").arg(errMsg);
			return;
		}
		const wchar_t* persianDictPathBroken = LR"path(C:\devb\PticaGovorunProj\srcrep\data\phoneticDictUkBroken.xml)path";
		std::tie(loadOp, errMsg) = loadPhoneticDictionaryXml(persianDictPathBroken, phoneReg_, phoneticDictWordsBroken_, *stringArena_);
		if (!loadOp)
		{
			errMsg_ = QString("Can't load phonetic dictionary. %1").arg(errMsg);
			return;
		}
		const wchar_t* persianDictPathFiller = LR"path(C:\devb\PticaGovorunProj\srcrep\data\phoneticDictFiller.xml)path";
		std::tie(loadOp, errMsg) = loadPhoneticDictionaryXml(persianDictPathFiller, phoneReg_, phoneticDictWordsFiller_, *stringArena_);
		if (!loadOp)
		{
			errMsg_ = QString("Can't load phonetic dictionary. %1").arg(errMsg);
			return;
		}

		if (includeBrownBear)
		{
			const wchar_t* brownBearDictPath = LR"path(C:\devb\PticaGovorunProj\srcrep\data\phoneticBrownBear.xml)path";
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

	std::tuple<bool, const char*> SphinxTrainDataBuilder::buildPhaseSpecificParts(ResourceUsagePhase phase, int maxWordPartUsage, int maxUnigramsCount, bool allowPhoneticWordSplit, const std::set<PhoneId>& trainPhoneIds)
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
		chooseSeedUnigrams(phoneticSplitter_, maxWordPartUsage, maxUnigramsCount, allowPhoneticWordSplit, phoneReg_, expandWellKnownPronCodeFun, trainPhoneIds, seedUnigrams);
		std::cout << "phase=" << (phase == ResourceUsagePhase::Train ? "train" : "test")
			<< " seed unigrams count=" << seedUnigrams.size() << std::endl;

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

		std::tie(op, errMsg) = writePhoneticDictSphinx(phoneticDictWords, phoneReg_, filePath(dbName_ + fileSuffix + ".dic"));
		if (!op)
			return std::make_tuple(false, "Can't write phonetic dictionary");

		// create the list of used phones

		PhoneAccumulator phoneAcc;
		phoneAcc.addPhonesFromPhoneticDict(phoneticDictWords);
		phoneAcc.addPhonesFromPhoneticDict(phoneticDictWordsFiller_);

		std::vector<std::string> phoneList;
		phoneAcc.buildPhoneList(phoneReg_, phoneList);

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

	bool loadWordList(const std::wstring& filePath, std::unordered_set<std::wstring>& words)
	{
		QFile file(QString::fromStdWString(filePath));
		if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
			return false;

		QTextCodec* textCodec = QTextCodec::codecForName("utf-8");
		std::array<char, 1024> lineBuff;
		while (true)
		{
			int readBytes = file.readLine(lineBuff.data(), lineBuff.size());
			if (readBytes == -1) // EOF
				return true;
			QString word = textCodec->toUnicode(lineBuff.data(), readBytes).trimmed();
			words.insert(word.toStdWString());
		}

		return true;
	}

	void SphinxTrainDataBuilder::loadDeclinationDictionary(std::unordered_map<std::wstring, std::unique_ptr<WordDeclensionGroup>>& declinedWordDict)
	{
		auto dictPathArray = {
			//LR"path(C:\devb\PticaGovorunProj\srcrep\build\x64\Debug\tmp2.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk01-�-�����-����������).htm.20150228220948.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk02-����������-�������-���������).htm.20150228213543.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk03-���������-����������-��������).htm.20150228141620.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk04-��������-�������������-�����������).htm.20150228220515.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk05-�����������-���������-��������).htm.20150301145100.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk06-��������-������������-�������).htm.20150302001636.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk07-�������-�����������-��������).htm.20150316142905.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk08-��������-�'��������-�).htm.20150325111235.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk09-�-������������-�����������������).htm.20150301230606.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk10-�����������������-���������������-�������������)8.htm.20150301230840.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk11-�������������-���������-��������).htm.20150325000544.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk12-��������-�'���-�).htm.20150408175213.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk13-�-����������-�).htm.20150407185103.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk14-�-��������-���������).htm.20150407185337.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk15-���������-���-�).htm.20150407185443.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk16-�-����-�).htm.20150301231717.xml)path",
			LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk17-�-��������-end).htm.20150301232344.xml)path",
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
		std::cout << "loaded declination dict in " << elapsedSec << "s" << std::endl;
	}

	void SphinxTrainDataBuilder::loadPhoneticSplitter(const std::unordered_map<std::wstring, std::unique_ptr<WordDeclensionGroup>>& declinedWordDict, UkrainianPhoneticSplitter& phoneticSplitter)
	{
		//
		wchar_t* processedWordsFile = LR"path(C:\devb\PticaGovorunProj\data\declinationDictUk\uk-done.txt)path";
		std::unordered_set<std::wstring> processedWords;
		if (!loadWordList(processedWordsFile, processedWords))
			return;

		std::wstring targetWord;

		std::chrono::time_point<Clock> now1 = Clock::now();
		phoneticSplitter.bootstrap(declinedWordDict, targetWord, processedWords);
		std::chrono::time_point<Clock> now2 = Clock::now();
		auto elapsedSec = std::chrono::duration_cast<std::chrono::seconds>(now2 - now1).count();
		std::cout << "phonetic split of declinartion dict took=" << elapsedSec << "s" << std::endl;

		int uniqueDeclWordsCount = uniqueDeclinedWordsCount(declinedWordDict);
		std::cout << "wordGroupsCount=" << declinedWordDict.size() << " uniqueDeclWordsCount=" << uniqueDeclWordsCount << std::endl;

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
		std::cout << "gatherWordPartsSequenceUsage took=" << elapsedSec << "s" << std::endl;

		std::array<long, 2> wordSeqSizes;
		wordUsage.wordSeqCountPerSeqSize(wordSeqSizes);
		for (int seqDim = 0; seqDim < wordSeqSizes.size(); ++seqDim)
		{
			long wordsPerNGram = wordSeqSizes[seqDim];
			std::wcout << L"wordsPerNGram[" << seqDim + 1 << "]=" << wordsPerNGram << std::endl;
		}
		std::wcout << L" wordParts=" << wordUsage.wordPartsCount() << std::endl;

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
				seg.TranscriptText.find(L"inhale") != std::wstring::npos)
				return false;
			if (seg.FilePath.find(L"pynzenykvm") != std::wstring::npos)
				return false;
			if (seg.Language != SpeechLanguage::Ukrainian)
				return false;
			if (!includeBrownBear && seg.StartMarker.SpeakerBriefId == L"BrownBear1")
				return false;
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

	std::tuple<bool, const char*> SphinxTrainDataBuilder::partitionTrainTestData(const std::vector<AnnotatedSpeechSegment>& segments, double trainCasesRatio, bool useBrokenPronsInTrainOnly, std::vector<details::AssignedPhaseAudioSegment>& outSegRefs, std::set<PhoneId>& trainPhoneIds) const
	{
		// partition train-test data
		std::vector<std::reference_wrapper<const AnnotatedSpeechSegment>> rejectedSegments;
		std::mt19937 g(1933);
		std::uniform_real_distribution<float> rnd(0, 1);
		for (const AnnotatedSpeechSegment& seg : segments)
		{
			bool isAlwaysTest = seg.StartMarker.ExcludePhase == ResourceUsagePhase::Train;
			bool isAlwaysTrain = seg.StartMarker.ExcludePhase == ResourceUsagePhase::Test;

			bool broken = isBrokenUtterance(seg.TranscriptText);
			if (useBrokenPronsInTrainOnly)
			{
				isAlwaysTrain |= broken;
			}
			isAlwaysTrain |= seg.StartMarker.SpeakerBriefId == L"BrownBear1";

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
				segRef.Phase = phase;
			}
			outSegRefs.push_back(segRef);
		}
		if (!rejectedSegments.empty())
			std::cout << "Rejecting " << rejectedSegments.size() << " segments" << std::endl; // info

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
				PG_Assert(pron != nullptr && "PronCode can't be expanded into phones, but has been successfully expanded before");
				
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
			std::cout << "Moved " << testToTrainMove << " sentences with rare phones to Train portion" << std::endl; // info

		return std::make_tuple(true, nullptr);
	}


	QString generateSegmentFileNameNoExt(const QString& baseFileName, const TimePointMarker& start, const TimePointMarker& end)
	{
		QLatin1Char filler('0');
		int width = 5;
		return QString("%1_%2-%3").arg(baseFileName).arg(start.Id, width, 10, filler).arg(end.Id, width, 10, filler);
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
		const QString& wavSrcFilePathAbs, const TimePointMarker& start, const TimePointMarker& end,
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
		QString segFileNameNoExt = generateSegmentFileNameNoExt(QFileInfo(wavOutRelFilePath).completeBaseName(), start, end);
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
			segRef.OutAudioSegPathParts = audioFilePathComponents(audioSrcRootDirPath, wavFilePath, seg.StartMarker, seg.EndMarker, wavBaseOutDirPath, wavDirPathForRelPathes);
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

		for (const details::AssignedPhaseAudioSegment& segRef : segsRefs)
		{
			if (segRef.Phase != targetPhase)
				continue;

			// fileIds
			txtFileIds << segRef.OutAudioSegPathParts.WavOutRelFilePathNoExt << "\n";

			// determine if segment requires padding with silence
			bool startsSil = false;
			bool endsSil = false;
			boost::optional<std::tuple<bool, bool>> isPadded = isSilenceFlankedSegment(segRef.Seg->StartMarker);
			if (isPadded != nullptr)
				std::tie(startsSil, endsSil) = isPadded.get();

			// transcription
			if (targetPhase == ResourceUsagePhase::Train && !startsSil)
				txtTranscription << "<s> ";
			txtTranscription << QString::fromStdWString(segRef.Seg->TranscriptText) <<" ";
			if (targetPhase == ResourceUsagePhase::Train && !endsSil)
				txtTranscription << "</s> ";
			txtTranscription << "(" << segRef.OutAudioSegPathParts.SegFileNameNoExt << ")" << "\n";
		}
		return true;
	}

	std::tuple<bool, const char*>  SphinxTrainDataBuilder::loadSilenceSegment(std::vector<short>& frames, float framesFrameRate) const
	{
		std::vector<short> silenceFrames;
		float silenceFrameRate = -1;
		const char* silenceWavPath = R"path(C:\devb\PticaGovorunProj\data\TrainSphinx\SpeechModels\pynzenyk-background-200ms.wav)path";
		auto readOp = readAllSamples(silenceWavPath, silenceFrames, &silenceFrameRate);
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
			std::string srcWavPath = QString::fromStdWString(wavFilePath).toStdString();
			std::cout << "wav=" << srcWavPath <<std::endl;

			srcAudioFrames.clear();
			auto readOp = readAllSamples(srcWavPath, srcAudioFrames, &srcAudioFrameRate);
			if (!std::get<0>(readOp))
			{
				errMsg_ = "Can't read wav file";
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
				bool startsSil = false;
				bool endsSil = false;
				if (padSilence)
				{
					boost::optional<std::tuple<bool, bool>> isPadded = isSilenceFlankedSegment(seg.StartMarker);
					if (isPadded != nullptr)
						std::tie(startsSil, endsSil) = isPadded.get();
				}

				// pad frames with silence
				paddedSegFramesOut.clear();

				if (!startsSil)
					std::copy(silenceFrames.begin(), silenceFrames.end(), std::back_inserter(paddedSegFramesOut));

				std::copy(segFrames.begin(), segFrames.end(), std::back_inserter(paddedSegFramesOut));

				if (!endsSil)
					std::copy(silenceFrames.begin(), silenceFrames.end(), std::back_inserter(paddedSegFramesOut));

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
}