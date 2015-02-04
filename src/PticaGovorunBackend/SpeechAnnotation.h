#pragma once
#include <string>
#include <vector>
#include <tuple>
#include <hash_set>
#include "SpeechProcessing.h"

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
		std::vector<SpeechAnnotationParameter> parameters_;
		std::vector<SpeakerFantom> speakers_;
		std::vector<TimePointMarker> frameIndMarkers_; // stores markers of all level (word, phone)
		std::hash_set<int> usedMarkerIds_; // stores ids of all markers; used to generate new free marker id
	public:
		SpeechAnnotation();
		~SpeechAnnotation();

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
		void validateMarkers(std::stringstream& msgValidate) const;

		const std::vector<TimePointMarker>& markers() const;

		TimePointMarker& marker(int markerInd);
		int markerIndByMarkerId(int markerId);

		int getClosestMarkerInd(long frameInd, long* dist);

		void addSpeaker(const std::wstring& speakerBriefId, const std::wstring& name);
		const std::vector<SpeakerFantom>& speakers() const;
		
		// Finds the speaker who has spoken recently, starting from given marker and goes back.
		// Returns Speaker.BriefId or empty string if the last speaker was not found.
		const std::wstring inferRecentSpeaker(int markerInd) const;
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
}
