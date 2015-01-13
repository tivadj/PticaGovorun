#ifndef TRANSCRIBERVIEWMODEL_H
#define TRANSCRIBERVIEWMODEL_H
#include <vector>
#include <map>
#include <atomic>
#include <memory>
#include <hash_set>

#include "MLUtils.h"


#include <QObject>
#include <QPointF>
#include <portaudio.h>
#include "XmlAudioMarkup.h"
#include "JuliusToolNativeWrapper.h"
#include "SpeechProcessing.h"
#include "AudioMarkupNavigator.h"

// Specifies which frame to choose as a starting one when playing an audio segment between two markers.
enum class SegmentStartFrameToPlayChoice
{
	// Starts playing from the current cursor.
	CurrentCursor,

	// Starts playing from the left marker of the segment.
	SegmentBegin
};

// The document is a graph for samples plus padding to the left and right.
// The samples graph is painted with current 'scale'.
// A user can specify samples of interest using a cursor. The sample is specified with a sample index.
// The range of samples is specified with indices of two boundary samples.
class TranscriberViewModel : public QObject
{
    Q_OBJECT
public:
signals :
	// Occurs when audio samples where successfully loaded from a file.
	void audioSamplesLoaded();
    void nextNotification(const QString& message);
    void audioSamplesChanged();
	void docOffsetXChanged();
	void lastMouseDocPosXChanged(float mouseDocPosX);

	// Occurs when current frame cursor changes.
	void cursorChanged(std::pair<long,long> oldValue);

	// Occurs when current marker selection changed.
	void currentMarkerIndChanged();
public:
    TranscriberViewModel();

    void loadAudioFile();

	// Plays the current segment of audio.
	// restoreCurFrameInd=true, if current frame must be restored when playing finishes.
	// The audio samples to play must exist during the process of playing.
	void soundPlayerPlay(const short* audioSouce, long startPlayingFrameInd, long finishPlayingFrameInd, bool restoreCurFrameInd);
	// outLeftMarkerInd (may be null): returns the index of current segment's left marker.
	std::tuple<long, long> getFrameRangeToPlay(long curFrameInd, SegmentStartFrameToPlayChoice startFrameChoice, int* outLeftMarkerInd = nullptr);

	void soundPlayerPlayCurrentSegment(SegmentStartFrameToPlayChoice startFrameChoice);
	void soundPlayerPause();

	// Plays from current cursor to the end of the audio.
	void soundPlayerPlay();

	// Plays from the current frame index to the next frame marker or the end of audio, what will occur earlier.
	void soundPlayerTogglePlayPause();
	bool soundPlayerIsPlaying() const;
	QString audioMarkupFilePathAbs() const;
	//

	void loadAudioMarkupFromXml();
	void saveAudioMarkupToXml();

	QString audioFilePath() const;
    void setAudioFilePath(const QString& filePath);

	// Number of pixels per one sample in X direction. Used to construct a samples graph.
	float pixelsPerSample() const;

	float docPaddingX() const;

	// The document position (in pixels) which is shown in a viewer. Vary in [0; docWidthPix()] range.
	float docOffsetX() const;
    void setDocOffsetX(float docOffsetXPix);

	// Width of the document (in pixels).
    float docWidthPix() const;
	
	// Returns first visible sample which is at docOffsetX position.
	float docPosXToSampleInd(float docPosX) const;
	float sampleIndToDocPosX(long sampleInd) const;
	//const arv::array_view<short> audioSamples222() const
	//{
	//  return arv::array_view<short>(audioSamples_);
	//	return arv::array_view<short>(std::begin(audioSamples_), std::end(audioSamples_));
	//}

public: // current sample
	const std::vector<short>& audioSamples() const;

	void setLastMousePressPos(const QPointF& localPos, bool isShiftPressed);
	long currentSampleInd() const;
	std::pair<long, long> cursor() const;
	static std::pair<long, long> nullCursor() { return std::make_pair(PticaGovorun::NullSampleInd, PticaGovorun::NullSampleInd); }
private:
	void setCursorInternal(long curSampleInd, bool updateCurrentMarkerInd, bool updateViewportOffset);
	void setCursorInternal(std::pair<long, long> value, bool updateCurrentMarkerInd, bool updateViewportOffset);
private:
		
	// The range of samples to perform action on.
	// when user clicks on the screen the first value is updated
	// if user selects the region, the second value is updated
	// two values are in unordered state
	std::pair<long, long> cursor_ = nullCursor();

	// markers
private:
	// Helper structure to store marker and its index in original markers collection.
	struct MarkerRefToOrigin
	{
		const PticaGovorun::TimePointMarker& Marker;
		int MarkerInd; // marker index in original 'frameIndMarkers_' collection.
	};

	// Some routines require only subset of markers (only phone level or only word level).
	// This temporary collection may store subset of markers.
	std::vector<MarkerRefToOrigin> markersOfInterestCache_;

	std::vector<PticaGovorun::TimePointMarker> frameIndMarkers_; // stores markers of all level (word, phone)
	int currentMarkerInd_ = -1;
	std::hash_set<int> usedMarkerIds_; // stores ids of all markers; used to generate new free marker id

	// Level of detail for newly created markers.
	PticaGovorun::MarkerLevelOfDetail templateMarkerLevelOfDetail_ = PticaGovorun::MarkerLevelOfDetail::Word;
	QPointF draggingLastViewportPos_; // used to avoid frequent repaints when mouse pos doesn't change
	bool isDraggingMarker_ = false;
	int draggingMarkerInd_ = -1;
	float lastMousePressDocPosX_ = -1;

public:

	const std::vector<PticaGovorun::TimePointMarker>& frameIndMarkers() const;
	void collectMarkersOfInterest(std::vector<MarkerRefToOrigin>& markersOfInterest, bool processPhoneMarkers);
	
	template <typename MarkerPred>
	void transformMarkersIf(std::vector<MarkerRefToOrigin>& markersOfInterest, MarkerPred canSelectMarker);
	
	void insertNewMarkerAtCursorRequest();
	void deleteCurrentMarkerRequest();
	// dist=number of frames between frameInd and the closest marker.
	template <typename Markers, typename FrameIndSelector>
	int getClosestMarkerInd(const Markers& markers, FrameIndSelector markerFrameIndSelector, long frameInd, long* dist) const;
	void selectMarkerClosestToCurrentCursorRequest();
	int currentMarkerInd() const;
	void setCurrentMarkerTranscriptText(const QString& text);
	void setCurrentMarkerLevelOfDetail(PticaGovorun::MarkerLevelOfDetail levelOfDetail);
	void setCurrentMarkerStopOnPlayback(bool stopsPlayback);

	void setTemplateMarkerLevelOfDetail(PticaGovorun::MarkerLevelOfDetail levelOfDetail);
	PticaGovorun::MarkerLevelOfDetail templateMarkerLevelOfDetail() const;

	void dragMarkerStart(const QPointF& localPos, int markerInd);
	void dragMarkerStop();
	void dragMarkerContinue(const QPointF& localPos);
private:
	void setCurrentMarkerIndInternal(int markerInd, bool updateCurrentSampleInd, bool updateViewportOffset);

	// returns the closest segment which contains given frameInd. The segment is described by two indices in
	// the original collection of markers.
	// returns false if such segment can't be determined.
	// leftMarkerInd=-1 for the frames before the first marker, and rightMarkerInd=-1 for the frames after the last marker.
	// acceptOutOfRangeFrameInd = true to return the first or the last segment for negative frameInd or frameInd larger than max frameInd.
	template <typename Markers, typename FrameIndSelector>
	bool findSegmentMarkerInds(const Markers& markers, FrameIndSelector markerFrameIndSelector, long frameInd, bool acceptOutOfRangeFrameInd, int& leftMarkerInd, int& rightMarkerInd) const;

	int generateMarkerId();

	// Ensures that all markers are sorted ascending by FrameInd.
	void insertNewMarkerSafe(int markerInd, const PticaGovorun::TimePointMarker& marker);
public:	// recongizer

	void ensureRecognizerIsCreated();
	void recognizeCurrentSegmentRequest();
	void ensureWordToPhoneListVocabularyLoaded();
	void alignPhonesForCurrentSegmentRequest();
	size_t silencePadAudioFramesCount() const;
	QString recognizerName() const;
	void setRecognizerName(const QString& filePath);
	
	void computeMfccRequest();
	void testMfccRequest();
	void classifyMfccIntoPhones();
private:
	std::map<std::string, std::unique_ptr<PticaGovorun::EMQuick>> phoneNameToEMObj_;
	std::map<std::string, std::vector<float>> phoneNameToFeaturesVector_;

public: // segment composer
	void playComposingRecipeRequest(QString recipe);
	int markerIndByMarkerId(int markerId);
private:
	std::vector<short> composedAudio_;

public:
	// navigation
	void setAudioMarkupNavigator(std::shared_ptr<AudioMarkupNavigator> audioMarkupNavigator);
	void navigateToMarkerRequest();
private:
	std::shared_ptr<AudioMarkupNavigator> audioMarkupNavigator_;

private:
	std::vector<short> audioSamples_;
    QString audioFilePathAbs_;
	float docOffsetX_ = 0;
	
	// number of pixels to the left/right of samples graph
	// together with the samples graph is treated as the document
	float docPaddingPix_ = 100;

	// Represents internal audio player's data.
	// The range (StartPlayingFrameInd;FinishPlayingFrameInd) of frames is played.
	struct SoundPlayerData
	{
		TranscriberViewModel* transcriberViewModel;
		const short* AudioSouce;
		long CurPlayingFrameInd;

#if PG_DEBUG
		// used to restore cursor to initial position
		long StartPlayingFrameInd; // is not changed when playing
#endif

		// used to determine when to stop playing
		long FinishPlayingFrameInd; // is not changed when playing

		// PticaGovorun::NullSampleInd if CurFrameInd should not be restored
		long RestoreCurrentFrameInd; // CurFrameInd to restore when playing finishes
		PaStream *stream;
		std::atomic<bool> allowPlaying;
	};
	SoundPlayerData soundPlayerData_;
	std::atomic<bool> isPlaying_;

	// recognition
	QString curRecognizerName_;
	std::unique_ptr<PticaGovorun::JuliusToolWrapper> recognizer_;
	std::map<std::wstring, std::vector<std::string>> wordToPhoneListDict_;
	std::vector<short> audioSegmentBuffer_; // the buffer to keep the padded audio segments
	
	// number of padding silence samples to the left and right of audio segment
	// The silence prefix/suffix for audio is equal across all recognizers.
	size_t silencePadAudioFramesCount_ = 1500; // pad with couple of windows of FrameSize

public:
	float scale_;
};

#endif // TRANSCRIBERVIEWMODEL_H
