#pragma once
#include <boost/filesystem.hpp>
#include <gsl/span>
#include "SphinxModel.h"

namespace PticaGovorun
{
	class KaldiModelBuilder
	{
		struct AssignedPhaseAudioSegmentAndUttId
		{
			const AssignedPhaseAudioSegment* SegRef;
			std::string UttId;
		};

		boost::filesystem::path outDirPath_;
		std::function<auto (boost::wstring_view)->boost::wstring_view> pronCodeDisplayTrain_;
		std::function<auto (boost::wstring_view)->boost::wstring_view> pronCodeDisplayTest_;
	public:
		KaldiModelBuilder(const boost::filesystem::path& outDirPath,
			std::function<auto (boost::wstring_view)->boost::wstring_view> pronCodeDisplayTrain,
			std::function<auto (boost::wstring_view)->boost::wstring_view> pronCodeDisplayTest);
		bool generate(gsl::span<const AssignedPhaseAudioSegment> segsRefs, ErrMsgList* errMsg) const;


		/// Kaldi's wav.scp (uttId->wav file path), utt2spk, text (uttId->transcription) use utterance id as the first token in a line.
		/// UttId should start with speakerId (is it mandatory?) so when all utterances are sorted, they become ordered by speakerId.
		static void createUtteranceId(const AssignedPhaseAudioSegment& segRef, std::string* uttId);

		static void createSpkIdToUttIdListMap(gsl::span<const AssignedPhaseAudioSegmentAndUttId> segsRefs, ResourceUsagePhase targetPhase,
			std::map<boost::string_view, std::vector<boost::string_view>>* spkId2UttIdList);

		bool writeUttId2SpeakerId(gsl::span<const AssignedPhaseAudioSegmentAndUttId> segsRefs, ResourceUsagePhase targetPhase,
			const boost::filesystem::path& filePath,
			ErrMsgList* errMsg) const;

		bool writeUttId2WavPathAbs(gsl::span<const AssignedPhaseAudioSegmentAndUttId> segsRefs, ResourceUsagePhase targetPhase,
			const boost::filesystem::path& filePath,
			ErrMsgList* errMsg) const;

		bool writeUttId2Transcription(gsl::span<const AssignedPhaseAudioSegmentAndUttId> segsRefs, ResourceUsagePhase targetPhase,
			std::function<auto (boost::wstring_view)->boost::wstring_view> pronCodeDisplay,
			const boost::filesystem::path& filePath,
			ErrMsgList* errMsg) const;
		
		bool writeSpkId2UttIdList(const std::map<boost::string_view, std::vector<boost::string_view>>& spkId2UttIdList,
			const boost::filesystem::path& filePath, ErrMsgList* errMsg) const;
	};
}
