#pragma once
#include <string>
#include <vector>
#include <tuple>
#include <hash_set>
#include "SpeechProcessing.h"
#include <boost/utility/string_ref.hpp>

namespace PticaGovorun
{
	struct PG_EXPORTS SpeechAnnotationParameter
	{
		std::string Name;
		std::wstring Value;
	};

	// Represents a speaker which voice occur in current recording.
	struct PG_EXPORTS SpeakerFantom
	{
		std::wstring BriefId;
		std::wstring Name;
	};

	// Represents information associated with speech signal used for learning of speech recognizer.
	class PG_EXPORTS SpeechAnnotation
	{
		friend PG_EXPORTS std::tuple<bool, const char*> loadAudioMarkupFromXml(const std::wstring& audioFilePathAbs, SpeechAnnotation& speechAnnot);
		std::wstring audioFileRelPath_; // the relative path to corresponding audio file
	private:
		std::vector<SpeechAnnotationParameter> parameters_;
		std::vector<SpeakerFantom> speakers_;
		
		// Stores markers of all level (word, phone)
		// The markers are ordered by increased SampleInd.
		std::vector<TimePointMarker> frameIndMarkers_;
		std::hash_set<int> usedMarkerIds_; // stores ids of all markers; used to generate new free marker id
	public:
		SpeechAnnotation();
		~SpeechAnnotation();

		std::wstring audioFileRelPath() const;
		void setAudioFileRelPath(std::wstring value);

		void clear();

		// Returns true if marker was deleted.
		bool deleteMarker(int markerInd);

		int generateMarkerId();

		// Adds annotation marker. This routine generates Id and insert given marker so all markers are ordered by Marker.SampleInd.
		// return new marker index
		int insertMarker(const PticaGovorun::TimePointMarker& marker);

		// Assumes Marker.Id and Marker.SampleInd are correct and just appends given marker to the end of internal structures.
		// Used when loading from serialized formats.
		void attachMarker(const PticaGovorun::TimePointMarker& marker);

		// Ensures that all markers are sorted ascending by FrameInd.
		// rename ~withChecks
		void insertNewMarkerSafe(int markerInd, const PticaGovorun::TimePointMarker& marker);

		// Validate marker's speech language
		void validateMarkers(QStringList& checkMsgs) const;

		size_t markersSize() const;

		const std::vector<TimePointMarker>& markers() const;

		TimePointMarker& marker(int markerInd);
		const TimePointMarker& marker(int markerInd) const;
		int markerIndByMarkerId(int markerId);

		int getClosestMarkerInd(long frameInd, long* dist);

		void addSpeaker(const std::wstring& speakerBriefId, const std::wstring& name);
		const std::vector<SpeakerFantom>& speakers() const;

		bool findSpeaker(const std::wstring& speakerBriefId, SpeakerFantom* speaker = nullptr) const;
		
		// Finds the speaker who has spoken recently, starting from given marker and goes back.
		// Returns Speaker.BriefId or empty string if the last speaker was not found.
		const std::wstring inferRecentSpeaker(int markerInd) const;

		// Update marker's frameInd and ensures that markers collection is ordered by FrameInd.
		// Returns true if the marker was updated.
		bool setMarkerFrameInd(int markerId, long frameInd);
	private:
		// Search for marker by Id. Returns pointer to the marker and index of the marker in the 
		// markers collection.
		// Note: marker.SampleInd can't be changed.
		TimePointMarker* markerById(int markerId, size_t* resultMarkerInd);
	};

	// returns the closest segment which contains given frameInd. The segment is described by two indices in
	// the original collection of markers.
	// returns false if such segment can't be determined.
	// leftMarkerInd=-1 for the frames before the first marker, and rightMarkerInd=-1 for the frames after the last marker.
	// acceptOutOfRangeFrameInd = true to return the first or the last segment for negative frameInd or frameInd larger than max frameInd.
	template <typename Markers, typename FrameIndSelector>
	bool findSegmentMarkerInds(const Markers& markers, FrameIndSelector markerFrameIndSelector, long frameInd, bool acceptOutOfRangeFrameInd, int& leftMarkerInd, int& rightMarkerInd)
	{
		if (markers.empty())
			return false;

		int lastMarkerInd = markers.size() - 1;
		long lastFrameInd = markerFrameIndSelector(markers[lastMarkerInd]);
		if (!acceptOutOfRangeFrameInd && (frameInd < 0 || frameInd >= lastFrameInd)) // out of range of samples
			return false;

		// before the first marker?
		if (frameInd < markerFrameIndSelector(markers[0]))
		{
			leftMarkerInd = -1;
			rightMarkerInd = 0;
			return true;
		}

		// after the last marker?
		if (frameInd >= markerFrameIndSelector(markers[lastMarkerInd]))
		{
			leftMarkerInd = lastMarkerInd;
			rightMarkerInd = -1;
			return true;
		}

		auto hitMarkerIt = PticaGovorun::binarySearch(std::begin(markers), std::end(markers), frameInd, markerFrameIndSelector);
		int hitMarkerInd = std::distance(std::begin(markers), hitMarkerIt);

		leftMarkerInd = hitMarkerInd;
		rightMarkerInd = hitMarkerInd + 1;

		PG_Assert(leftMarkerInd >= 0 && "Marker index is out of range");
		PG_Assert(rightMarkerInd < markers.size() && "Marker index is out of range");
		return true;
	}

	// dist=number of frames between frameInd and the closest marker.
	template <typename Markers, typename FrameIndSelector>
	int getClosestMarkerInd(const Markers& markers, FrameIndSelector markerFrameIndSelector, long frameInd, long* dist);

	PG_EXPORTS void validateSpeechTranscription(boost::wstring_ref speechDataDir);

	struct PG_EXPORTS AnnotSpeechFileNode
	{
		QString FileNameNoExt;
		//QString AudioPath;
		QString SpeechAnnotationAbsPath;
	};

	struct PG_EXPORTS AnnotSpeechDirNode
	{
		QString Name; // name of the directory
		QString DirFullPath;
		std::vector<AnnotSpeechDirNode> SubDirs;
		std::vector<AnnotSpeechFileNode> AnnotFiles;
	};

	// Loads hierarchical information about speech annotation.
	PG_EXPORTS void populateAnnotationFileStructure(const QString& annotRootDir, AnnotSpeechDirNode& pseudoRoot);

	// Gets the list of wav files from given hierarchy.
	PG_EXPORTS void flat(const AnnotSpeechDirNode& node, std::vector<AnnotSpeechFileNode>& result);
}
