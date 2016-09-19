#include <functional>
#include "SpeechAnnotation.h"
#include <QDir>
#include <QDirIterator>
#include "CoreUtils.h"
#include "PhoneticService.h"
#include "assertImpl.h"

namespace PticaGovorun
{

	SpeechAnnotation::SpeechAnnotation()
	{
	}


	SpeechAnnotation::~SpeechAnnotation()
	{
	}

	std::wstring SpeechAnnotation::audioFileRelPath() const
	{
		return audioFileRelPath_;
	}

	void SpeechAnnotation::setAudioFileRelPath(std::wstring value)
	{
		audioFileRelPath_ = value;
	}

	size_t SpeechAnnotation::markersSize() const
	{
		return frameIndMarkers_.size();
	}

	const std::vector<TimePointMarker>& SpeechAnnotation::markers() const
	{
		return frameIndMarkers_;
	}

	TimePointMarker& SpeechAnnotation::marker(int markerInd)
	{
		return frameIndMarkers_[markerInd];
	}

	const TimePointMarker& SpeechAnnotation::marker(int markerInd) const
	{
		return frameIndMarkers_[markerInd];
	}

	int SpeechAnnotation::markerIndByMarkerId(int markerId)
	{
		for (size_t i = 0; i < frameIndMarkers_.size(); ++i)
		{
			const auto& marker = frameIndMarkers_[i];
			if (marker.Id == markerId)
				return i;
		}
		return -1;
	}
	
	TimePointMarker* SpeechAnnotation::markerById(int markerId, size_t* resultMarkerInd)
	{
		for (size_t i = 0; i < frameIndMarkers_.size(); ++i)
		{
			auto& marker = frameIndMarkers_[i];
			if (marker.Id == markerId)
			{
				if (resultMarkerInd != nullptr) 
					*resultMarkerInd = i;
				return &marker;
			}
		}
		return nullptr;
	}
	
	bool SpeechAnnotation::setMarkerFrameInd(int markerId, long frameInd)
	{
		size_t markerInd = (size_t)-1;
		auto pMarker = markerById(markerId, &markerInd);
		if (pMarker == nullptr || pMarker ->SampleInd == frameInd)
			return false;

		// keep markers collection ordered by FrameInd
		pMarker->SampleInd = frameInd;
		std::sort(std::begin(frameIndMarkers_), std::end(frameIndMarkers_), 
			[](const TimePointMarker& a, const TimePointMarker& b)
		{
			return a.SampleInd < b.SampleInd;
		});
		return true;
	}

	int SpeechAnnotation::getClosestMarkerInd(long frameInd, long* dist)
	{
		auto markerFrameIndSelector = [](const PticaGovorun::TimePointMarker& m) { return m.SampleInd; };
		int closestMarkerInd = PticaGovorun::getClosestMarkerInd(frameIndMarkers_, markerFrameIndSelector, frameInd, dist);

		return closestMarkerInd;
	}

	void SpeechAnnotation::addSpeaker(const std::wstring& speakerBriefId, const std::wstring& name)
	{
		SpeakerFantom speaker;
		speaker.BriefId = speakerBriefId;
		speaker.Name = name;
		speakers_.push_back(speaker);
	}

	const std::vector<SpeakerFantom>& SpeechAnnotation::speakers() const
	{
		return speakers_;
	}

	bool SpeechAnnotation::findSpeaker(const std::wstring& speakerBriefId, SpeakerFantom* speaker) const
	{
		for(const SpeakerFantom& s : speakers_)
		{
			if (s.BriefId == speakerBriefId)
			{
				if (speaker != nullptr)
					*speaker = s;
				return true;
			}
		}
		return false;
	}

	const std::wstring SpeechAnnotation::inferRecentSpeaker(int markerInd) const
	{
		if (markerInd < 0 || markerInd >= frameIndMarkers_.size())
			return L"";

		// find the speaker in previous segment
		std::wstring prevSpeakerBriefId;
		for (int i = markerInd; i >= 0; i--)
		{
			const PticaGovorun::TimePointMarker& prevMarker = frameIndMarkers_[i];
			if (!prevMarker.TranscripText.isEmpty() && !prevMarker.SpeakerBriefId.empty())
			{
				prevSpeakerBriefId = prevMarker.SpeakerBriefId;
				break;
			}
		}
		return prevSpeakerBriefId;
	}

	void SpeechAnnotation::validateMarkers(QStringList& checkMsgs) const
	{
		for (auto& marker : frameIndMarkers_)
		{
			if (marker.LevelOfDetail == PticaGovorun::MarkerLevelOfDetail::Phone)
			{
				if (marker.Language != PticaGovorun::SpeechLanguage::NotSet)
					checkMsgs.append(QString("Phone marker[id=%1] has non empty language").arg(marker.Id));
				if (!marker.SpeakerBriefId.empty())
					checkMsgs.append(QString("Phone marker[id=%1] has non empty speakerId").arg(marker.Id));
			}

			else if (marker.LevelOfDetail == PticaGovorun::MarkerLevelOfDetail::Word)
			{
				if (marker.TranscripText.isEmpty())
				{
					if (marker.Language != PticaGovorun::SpeechLanguage::NotSet)
						checkMsgs.append(QString("Word marker[id=%1] without text has non empty language").arg(marker.Id));
					if (!marker.SpeakerBriefId.empty())
						checkMsgs.append(QString("Word marker[id=%1] without text has non empty speakerBriefId").arg(marker.Id));
				}
				else if (marker.TranscripText == toQString(fillerSilence()))
				{
					// silence has no language, has no speaker
					if (marker.Language != PticaGovorun::SpeechLanguage::NotSet)
						checkMsgs.append(QString("Silence marker[id=%1] can't define a language").arg(marker.Id));
					if (!marker.SpeakerBriefId.empty())
						checkMsgs.append(QString("Silence marker[id=%1] can't define speakerBriefId").arg(marker.Id));
				}
				else if (marker.TranscripText == toQString(fillerInhale()))
				{
					// inhale has no language but has a speaker
					if (marker.Language != PticaGovorun::SpeechLanguage::NotSet)
						checkMsgs.append(QString("Inhale marker[id=%1] can't define a language").arg(marker.Id));
					if (marker.SpeakerBriefId.empty())
						checkMsgs.append(QString("Inhale marker[id=%1] must define speakerBriefId").arg(marker.Id));
				}
				else
				{
					if (marker.Language == PticaGovorun::SpeechLanguage::NotSet)
						checkMsgs.append(QString("Word marker[id=%1] with text has empty language").arg(marker.Id));
					
					if (marker.SpeakerBriefId.empty())
						checkMsgs.append(QString("Word marker[id=%1] with text has empty speakerBriefId").arg(marker.Id));
					else if (!findSpeaker(marker.SpeakerBriefId))
						checkMsgs.append(QString("Word marker[id=%1] with text has undefined speakerBriefId").arg(marker.Id));
				}
			}
		}
	}

	void SpeechAnnotation::clear()
	{
		frameIndMarkers_.clear();
		parameters_.clear();
		speakers_.clear();
		usedMarkerIds_.clear(); // clear cache of used marker ids
	}

	bool SpeechAnnotation::deleteMarker(int markerInd)
	{
		if (markerInd < 0 || markerInd >= frameIndMarkers_.size())
			return false;

		frameIndMarkers_.erase(frameIndMarkers_.cbegin() + markerInd);
		return true;
	}

	int SpeechAnnotation::generateMarkerId()
	{
		// generate random id
		int result;
		while (true)
		{
			size_t maxId = frameIndMarkers_.size() * 2;
			if (maxId == 0)
				maxId = 100;
			result = 1 + rand() % maxId; // +1 for id>0
			if (usedMarkerIds_.find(result) == std::end(usedMarkerIds_))
				break;
		}
		usedMarkerIds_.insert(result);

#if PG_DEBUG
		// ensure generated id doesn't collide with id of other markers
		for (size_t i = 0; i < frameIndMarkers_.size(); ++i)
		{
			const auto& marker = frameIndMarkers_[i];
			PG_Assert2(marker.Id != result, "Generated marker id which collides with id of another marker");
		}
#endif

		return result;
	}

	int SpeechAnnotation::insertMarker(const TimePointMarker& marker)
	{
		PG_DbgAssert2(marker.Id == -1, "MarkerId is generated by owning collection");

		long frameInd = marker.SampleInd;

		// find the insertion position in the markers collection

		int newMarkerInd = -1;
		int leftMarkerInd;
		int rightMarkerInd;
		auto markerFrameIndSelector = [](const PticaGovorun::TimePointMarker& m) { return m.SampleInd; };
		bool findSegOp = findSegmentMarkerInds(frameIndMarkers_, markerFrameIndSelector, frameInd, true, leftMarkerInd, rightMarkerInd);
		if (!findSegOp || leftMarkerInd == -1) // insert the first marker
			newMarkerInd = 0;
		else
			newMarkerInd = leftMarkerInd + 1; // next to the left marker

		//
		TimePointMarker newMarker = marker;
		newMarker.Id = generateMarkerId();

		insertNewMarkerSafe(newMarkerInd, newMarker);
		return newMarkerInd;
	}

	void SpeechAnnotation::attachMarker(const PticaGovorun::TimePointMarker& marker)
	{
		frameIndMarkers_.push_back(marker);
		usedMarkerIds_.insert(marker.Id);
	}

	void SpeechAnnotation::insertNewMarkerSafe(int newMarkerInd, const PticaGovorun::TimePointMarker& newMarker)
	{
		const auto insPosIt = frameIndMarkers_.cbegin() + newMarkerInd;

#if PG_DEBUG
		// ensure all markers are sorted by FrameInd
		// compare to the right marker
		if (insPosIt != std::cend(frameIndMarkers_))
		{
			int rightFrameInd = insPosIt->SampleInd;
			PG_Assert2(newMarker.SampleInd <= rightFrameInd, "New marker is out of FrameInd order");
		}
		// compare to the left marker
		if (insPosIt != std::cbegin(frameIndMarkers_))
		{
			auto leftMarkerIt = insPosIt - 1;
			int leftFrameInd = leftMarkerIt->SampleInd;
			PG_Assert2(leftFrameInd <= newMarker.SampleInd, "New marker is out of FrameInd order");
		}
#endif

		frameIndMarkers_.insert(insPosIt, newMarker);
	}

	template <typename Markers, typename FrameIndSelector>
	int getClosestMarkerInd(const Markers& markers, FrameIndSelector markerFrameIndSelector, long frameInd, long* distFrames)
	{
		*distFrames = -1;

		int leftMarkerInd = -1;
		int rightMarkerInd = -1;
		if (!findSegmentMarkerInds(markers, markerFrameIndSelector, frameInd, true, leftMarkerInd, rightMarkerInd))
		{
			// there is no closest marker
			return -1;
		}

		int closestMarkerInd = -1;

		if (leftMarkerInd == -1)
		{
			// current cursor is before the first marker
			closestMarkerInd = rightMarkerInd;
			*distFrames = markers[rightMarkerInd].SampleInd - frameInd;
		}
		if (rightMarkerInd == -1)
		{
			// current cursor is to the right of the last marker
			closestMarkerInd = leftMarkerInd;
			*distFrames = frameInd - markers[leftMarkerInd].SampleInd;
		}

		if (closestMarkerInd == -1)
		{
			// there are two candidate (left and right) markers to select, select closest

			long distLeft = frameInd - markers[leftMarkerInd].SampleInd;
			PG_Assert2(distLeft >= 0, "Error calculating the distance from the current cursor to the left marker");

			long distRight = markers[rightMarkerInd].SampleInd - frameInd;
			PG_Assert2(distRight >= 0, "Error calculating the distance from the current cursor to the right marker");

			if (distLeft < distRight)
			{
				closestMarkerInd = leftMarkerInd;
				*distFrames = distLeft;
			}
			else
			{
				closestMarkerInd = rightMarkerInd;
				*distFrames = distRight;
			}
		}

		PG_Assert2(closestMarkerInd != -1, "Error calculating the closest marker");
		PG_Assert2(*distFrames >= 0, "Error calculating the distance to the closest marker");

		return closestMarkerInd;
	}

	// Recursively constructs file structure of speech annotation files.
	struct SpeechAnnotationWorkspaceBuilder
	{
		void populateItemsRec(const QFileInfo& fileInfo, AnnotSpeechDirNode& parent)
		{
			QString fileName = fileInfo.fileName();
			if (fileInfo.isFile() && fileName.endsWith(".xml", Qt::CaseInsensitive))
			{
				AnnotSpeechFileNode fileRecord;
				fileRecord.FileNameNoExt = fileInfo.completeBaseName();
				//fileRecord.AudioPath = ""; // audio path will be populated when xml file is opened
				fileRecord.SpeechAnnotationAbsPath = fileInfo.absoluteFilePath();
				parent.AnnotFiles.push_back(fileRecord);
			}
			else if (fileInfo.isDir())
			{
				AnnotSpeechDirNode annotDir;
				annotDir.Name = fileInfo.fileName();
				annotDir.DirFullPath = fileInfo.absoluteFilePath();

				populateSubItemsWithoutItemItselfRec(fileInfo, annotDir);
				
				parent.SubDirs.push_back(annotDir);
			}
		}

		void populateSubItemsWithoutItemItselfRec(const QFileInfo& fileInfoExcl, AnnotSpeechDirNode& parent)
		{
			QDirIterator it(fileInfoExcl.absoluteFilePath(),
				QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
			while (it.hasNext())
			{
				it.next();
				QFileInfo childFileInfo = it.fileInfo();
				populateItemsRec(childFileInfo, parent);
			}
		}
	};

	void populateAnnotationFileStructure(const QString& annotRootDir, AnnotSpeechDirNode& pseudoRoot)
	{
		SpeechAnnotationWorkspaceBuilder annotStructure;
		annotStructure.populateSubItemsWithoutItemItselfRec(QFileInfo(annotRootDir), pseudoRoot);
	}

	void flat(const AnnotSpeechDirNode& node, std::vector<AnnotSpeechFileNode>& result)
	{
		std::function<auto (const AnnotSpeechDirNode&) -> void> flatRec;
		flatRec = [&result, &flatRec](const AnnotSpeechDirNode& cur) -> void
		{
			std::copy(cur.AnnotFiles.begin(), cur.AnnotFiles.end(), std::back_inserter(result));

			for (const AnnotSpeechDirNode& subdir : cur.SubDirs)
				flatRec(subdir);
		};
		flatRec(node);
	}
}