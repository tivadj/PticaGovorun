#include "KaldiModel.h"
#include <boost/format.hpp>

namespace PticaGovorun
{
	KaldiModelBuilder::KaldiModelBuilder(const boost::filesystem::path& outDirPath,
		std::function<auto (boost::wstring_view)->boost::wstring_view> pronCodeDisplayTrain,
		std::function<auto (boost::wstring_view)->boost::wstring_view> pronCodeDisplayTest)
		:outDirPath_(outDirPath),
		pronCodeDisplayTrain_(pronCodeDisplayTrain),
		pronCodeDisplayTest_(pronCodeDisplayTest)
	{
	}

	bool KaldiModelBuilder::generate(gsl::span<const AssignedPhaseAudioSegment> segRefs, ErrMsgList* errMsg) const
	{
		if (!boost::filesystem::create_directories(outDirPath_))
		{
			pushErrorMsg(errMsg, str(boost::format("Can't create output directory (%1%)") % outDirPath_.string()));
			return false;
		}
		std::vector<AssignedPhaseAudioSegmentAndUttId> segAndUttIds;
		segAndUttIds.reserve(segRefs.size());
		std::transform(segRefs.begin(), segRefs.end(), std::back_inserter(segAndUttIds), [](const auto& segRef)
		{
			std::string uttId;
			createUtteranceId(segRef, &uttId);
			return AssignedPhaseAudioSegmentAndUttId{ &segRef, std::move(uttId) };
		});

		// each line in wav.scp must be sorted by speakerId and then by uttId
		std::sort(segAndUttIds.begin(), segAndUttIds.end(), [](const auto& x, const auto& y)
		{
			return x.UttId < y.UttId;
		});

		if (!writeUttId2SpeakerId(segAndUttIds, ResourceUsagePhase::Train, outDirPath_ / "train_utt2spk", errMsg))
			return false;
		if (!writeUttId2SpeakerId(segAndUttIds, ResourceUsagePhase::Test, outDirPath_ / "test_utt2spk", errMsg))
			return false;
		if (!writeUttId2WavPathAbs(segAndUttIds, ResourceUsagePhase::Train, outDirPath_ / "train_wav.scp", errMsg))
			return false;
		if (!writeUttId2WavPathAbs(segAndUttIds, ResourceUsagePhase::Test, outDirPath_ / "test_wav.scp", errMsg))
			return false;
		if (!writeUttId2Transcription(segAndUttIds, ResourceUsagePhase::Train, pronCodeDisplayTrain_, outDirPath_ / "train_text", errMsg))
			return false;
		if (!writeUttId2Transcription(segAndUttIds, ResourceUsagePhase::Test, pronCodeDisplayTest_, outDirPath_ / "test_text", errMsg))
			return false;
		
		std::map<boost::string_view, std::vector<boost::string_view>> spkId2UttIdListTrain;
		createSpkIdToUttIdListMap(segAndUttIds, ResourceUsagePhase::Train, &spkId2UttIdListTrain);
		if (!writeSpkId2UttIdList(spkId2UttIdListTrain, outDirPath_ / "train_spk2uttId.map", errMsg))
			return false;

		std::map<boost::string_view, std::vector<boost::string_view>> spkId2UttIdListTest;
		createSpkIdToUttIdListMap(segAndUttIds, ResourceUsagePhase::Test, &spkId2UttIdListTest);
		if (!writeSpkId2UttIdList(spkId2UttIdListTest, outDirPath_ / "test_spk2uttId.map", errMsg))
			return false;

		return true;
	}
	void KaldiModelBuilder::createUtteranceId(const AssignedPhaseAudioSegment& segRef, std::string* uttId)
	{
		// Kaldi requires all utterances to be sorted my speakerId
		// if uttId has speakerId as the prefix then when all utterances are sorted, this condition is automatically satisfied
		uttId->append(segRef.Seg->SpeakerBriefId);
		uttId->push_back('_');

		const std::string& segName = toUtf8StdString(segRef.OutAudioSegPathParts.SegFileNameNoExt);
		uttId->append(segName.data(), segName.size());
	}

	void KaldiModelBuilder::createSpkIdToUttIdListMap(gsl::span<const AssignedPhaseAudioSegmentAndUttId> segsRefs, ResourceUsagePhase targetPhase, std::map<boost::string_view, std::vector<boost::string_view>>* spkId2UttIdList)
	{
		for (const auto& segAndUttId : segsRefs)
		{
			if (segAndUttId.SegRef->Phase != targetPhase)
				continue;

			auto& uttIdList = (*spkId2UttIdList)[segAndUttId.SegRef->Seg->SpeakerBriefId];
			uttIdList.push_back(segAndUttId.UttId);
		}
	}

	bool KaldiModelBuilder::writeUttId2SpeakerId(gsl::span<const AssignedPhaseAudioSegmentAndUttId> segsRefs, ResourceUsagePhase targetPhase,
		const boost::filesystem::path& filePath,
		ErrMsgList* errMsg) const
	{
		QFile file(toQStringBfs(filePath));
		if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
		{
			pushErrorMsg(errMsg, str(boost::format("Can't open file for writing (%1%)") % filePath.string()));
			return false;
		}

		QTextStream txtStream(&file);
		txtStream.setCodec("UTF-8");
		txtStream.setGenerateByteOrderMark(false);

		std::vector<boost::wstring_view> pronCodes;
		GrowOnlyPinArena<wchar_t> arena(1024);
		std::string uttId;
		for (const auto& segAndUttId : segsRefs)
		{
			if (segAndUttId.SegRef->Phase != targetPhase)
				continue;

			// utteranceId <space> speakerId
			txtStream << utf8ToQString(segAndUttId.UttId) << " " << utf8ToQString(segAndUttId.SegRef->Seg->SpeakerBriefId) << "\n";
		}
		return true;
	}

	bool KaldiModelBuilder::writeUttId2WavPathAbs(gsl::span<const AssignedPhaseAudioSegmentAndUttId> segsRefs, ResourceUsagePhase targetPhase,
		const boost::filesystem::path& filePath,
		ErrMsgList* errMsg) const
	{
		QFile file(toQStringBfs(filePath));
		if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
		{
			pushErrorMsg(errMsg, str(boost::format("Can't open file for writing (%1%)") % filePath.string()));
			return false;
		}

		QTextStream txtStream(&file);
		txtStream.setCodec("UTF-8");
		txtStream.setGenerateByteOrderMark(false);

		std::vector<boost::wstring_view> pronCodes;
		GrowOnlyPinArena<wchar_t> arena(1024);
		for (const auto& segAndUttId : segsRefs)
		{
			if (segAndUttId.SegRef->Phase != targetPhase)
				continue;

			// utteranceId <space> speakerId
			auto audioFilePathAbs = segAndUttId.SegRef->OutAudioSegPathParts.AudioSegFilePathNoExt + ".wav";
			txtStream << utf8ToQString(segAndUttId.UttId) << " " << audioFilePathAbs << "\n";
		}
		return true;
	}

	bool KaldiModelBuilder::writeUttId2Transcription(gsl::span<const AssignedPhaseAudioSegmentAndUttId> segsRefs, ResourceUsagePhase targetPhase,
		std::function<auto (boost::wstring_view)->boost::wstring_view> pronCodeDisplay,
		const boost::filesystem::path& filePath, ErrMsgList* errMsg) const
	{
		QFile file(toQStringBfs(filePath));
		if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
		{
			pushErrorMsg(errMsg, str(boost::format("Can't open file for writing (%1%)") % filePath.string()));
			return false;
		}

		QTextStream txtStream(&file);
		txtStream.setCodec("UTF-8");
		txtStream.setGenerateByteOrderMark(false);

		std::vector<boost::wstring_view> pronCodes;
		GrowOnlyPinArena<wchar_t> arena(1024);
		for (const auto& segAndUttId : segsRefs)
		{
			if (segAndUttId.SegRef->Phase != targetPhase)
				continue;

			// utteranceId <space> speakerId
			auto audioFilePathAbs = segAndUttId.SegRef->OutAudioSegPathParts.AudioSegFilePathNoExt + ".wav";
			txtStream << utf8ToQString(segAndUttId.UttId) << " ";

			// output TranscriptText mangling a pronCode if necessary (eg clothes(2)->clothes2)
			pronCodes.clear();
			arena.clear();
			splitUtteranceIntoPronuncList(segAndUttId.SegRef->Seg->TranscriptText, arena, pronCodes);
			for (size_t i = 0; i < pronCodes.size(); ++i)
			{
				boost::wstring_view pronCode = pronCodes[i];

				boost::wstring_view dispPronCode = pronCode;

				auto dispName = pronCodeDisplay(pronCode);
				if (dispName != boost::wstring_view())
					dispPronCode = dispName;

				txtStream << toQString(dispPronCode) << " ";
			}

			txtStream << "\n";
		}
		return true;
	}

	bool KaldiModelBuilder::writeSpkId2UttIdList(const std::map<boost::string_view, std::vector<boost::string_view>>& spkId2UttIdList,
		const boost::filesystem::path& filePath, ErrMsgList* errMsg) const
	{
		QFile file(toQStringBfs(filePath));
		if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
		{
			pushErrorMsg(errMsg, str(boost::format("Can't open file for writing (%1%)") % filePath.string()));
			return false;
		}

		QTextStream txtStream(&file);
		txtStream.setCodec("UTF-8");
		txtStream.setGenerateByteOrderMark(false);

		std::vector<boost::wstring_view> pronCodes;
		GrowOnlyPinArena<wchar_t> arena(1024);
		for (const auto& spkId2UttIdListPair : spkId2UttIdList)
		{
			const auto& spkId = spkId2UttIdListPair.first;
			txtStream << utf8ToQString(spkId) << " ";

			for (const auto& uttId : spkId2UttIdListPair.second)
			{
				txtStream << utf8ToQString(uttId) << " ";
			}

			txtStream << "\n";
		}
		return true;
	}
}
