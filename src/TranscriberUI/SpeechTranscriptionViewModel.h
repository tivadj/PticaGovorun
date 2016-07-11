#ifndef TRANSCRIBERVIEWMODEL_H
#define TRANSCRIBERVIEWMODEL_H
#include <vector>
#include <map>
#include <atomic>
#include <memory>
#include <hash_set>

#include <QPoint>
#include <QSize>
#include <portaudio.h>

#include "XmlAudioMarkup.h"
#include "MLUtils.h"
#include "JuliusToolNativeWrapper.h"
#include "SpeechAnnotation.h"
#include "SpeechProcessing.h"
#include "AudioMarkupNavigator.h"
#include "PhoneticDictionaryViewModel.h"
#include "VisualNotificationService.h"
#include "JuliusRecognizerProvider.h"
#include "AppHelpers.h"

namespace PticaGovorun
{
	class SharedServiceProvider;

	// Specifies which frame to choose as a starting one when playing an audio segment between two markers.
enum class SegmentStartFrameToPlayChoice
{
	// Starts playing from the current cursor.
	CurrentCursor,

	// Starts playing from the left marker of the segment.
	SegmentBegin
};

// Visual representation of the segment of samples.
struct DiagramSegment
{
	long SampleIndBegin = -1;
	long SampleIndEnd = -1;

	PticaGovorun::PhoneAlignmentInfo TranscripTextPhones; // splits transcripted text into phones and align them onto audio

	// Text used for phone alignment on this segment
	QString TextToAlign;
	//PhoneAlignmentInfo TranscripTextPhones; // splits transcripted text into phones and align them onto audio

	// segmemt recognition result
	QString RecogSegmentText;
	QString RecogSegmentWords;
	std::vector<PticaGovorun::AlignedPhoneme> RecogAlignedPhonemeSeq;
	bool RecogAlignedPhonemeSeqPadded = true; // true, if alignment was on a padded (with zeros) segment

	std::vector<PticaGovorun::ClassifiedSpeechSegment> ClassifiedFrames;

	// Recognized words for the segment of speech.
	std::vector<PticaGovorun::AlignedWord> WordBoundaries;
};

// The position of a sample inside the viewport.
// The viewport is split into horizontal lanes.
struct ViewportHitInfo
{
	int LaneInd = -1;

	// The horizontal offset from lane start.
	float DocXInsideLane = -1;
};

// The vertical part of a Rect.
struct RectY
{
	float Top;
	float Height;
};

// Mutually exclusive set of cursor kinds.
enum class TranscriberCursorKind
{
	// Cursor doesn't point to any sample.
	Empty,

	// Cursor points to the single sample.
	Single,

	// Cursor points to a range of samples.
	Range
};

// The document is a graph for samples plus padding to the left and right.
// The samples graph is painted with current 'scale'.
// A user can specify samples of interest using a cursor. The sample is specified with a sample index.
// The range of samples is specified with indices of two boundary samples.
// When playing audio, the current playing sample is shown.
class SpeechTranscriptionViewModel : public QObject
{
    Q_OBJECT
public:
signals :
	// Occurs when audio samples where successfully loaded from a file.
	void audioSamplesLoaded();
    void audioSamplesChanged();
	void docOffsetXChanged();
	void lastMouseDocPosXChanged(float mouseDocPosX);

	// Occurs when current frame cursor changes.
	void cursorChanged(std::pair<long,long> oldValue);

	// Occurs when current marker selection changed.
	void currentMarkerIndChanged();

	// Occurs when the index of current playing sample changes.
	void playingSampleIndChanged(long oldPlayingSampleInd);
public:
	SpeechTranscriptionViewModel();
	void init(std::shared_ptr<SharedServiceProvider> serviceProvider);

    void loadAnnotAndAudioFileRequest();

	// Plays the current segment of audio.
	// restoreCurFrameInd=true, if current frame must be restored when playing finishes.
	// The audio samples to play must exist during the process of playing.
	void soundPlayerPlay(const short* audioSouce, long startPlayingFrameInd, long finishPlayingFrameInd, bool restoreCurFrameInd);
	
	// Returns the ordered (first <= second) range of samples to process.
	// outLeftMarkerInd (may be null): returns the index of current segment's left marker.
	std::tuple<long, long> getSampleRangeToPlay(long curSampleInd, SegmentStartFrameToPlayChoice startFrameChoice, int* outLeftMarkerInd = nullptr);

	void soundPlayerPlayCurrentSegment(SegmentStartFrameToPlayChoice startFrameChoice);
	void soundPlayerPause();

	// Plays from current cursor to the end of the audio.
	void soundPlayerPlay();

	// Plays from the current frame index to the next frame marker or the end of audio, what will occur earlier.
	void soundPlayerTogglePlayPause();
	bool soundPlayerIsPlaying() const;
	long playingSampleInd() const;

	// updateViewportOffset is true to keep viewport above the playing caret.
	void setPlayingSampleInd(long value, bool updateViewportOffset);

	//
	void saveAudioMarkupToXml();

	void saveCurrentRangeAsWavRequest();

	QString modelShortName() const;
	QString annotFilePath() const;
	void setAnnotFilePath(const QString& filePath);

	// Number of pixels per one sample in X direction. Used to construct a samples graph.
	float pixelsPerSample() const;

	float docPaddingX() const;

	// The document position (in pixels) which is shown in a viewer. Vary in [0; docWidthPix()] range.
	float docOffsetX() const;
    void setDocOffsetX(float docOffsetXPix);

	// Width of the document (in pixels).
    float docWidthPix() const;

	bool phoneRulerVisible() const;

	void scrollPageForwardRequest();
	void scrollPageBackwardRequest();
	void scrollDocumentStartRequest();
	void scrollDocumentEndRequest();
private:
	// forward=true, false=backward
	void scrollPageWithLimits(float newDocOffsetX);
public:
	// Number of horizontal strips which split viewport area.
	// Used to show more waveform data on a screen.
	int lanesCount() const;
	void setLanesCount(int lanesCount);
	void increaseLanesCountRequest();
	void decreaseLanesCountRequest();

	QSizeF viewportSize() const;
	void setViewportSize(float viewportWidth, float viewportHeight);
private:
	float docOffsetX_ = 0;
	int lanesCount_ = 3; // number of horizontal lines to split viewport into

	// number of pixels to the left/right of samples graph
	// together with the samples graph is treated as the document
	float docPaddingPix_ = 100;
	bool phoneRulerVisible_ = false;
	QSizeF viewportSize_;
	
public:
	// Returns first visible sample which is at docOffsetX position.
	float docPosXToSampleInd(float docPosX) const;
	float sampleIndToDocPosX(long sampleInd) const;

	// Converts local (x;y) position into document position, counting from viewport origin.
	// The document offset of the viewport is not counted.
	// Lanes are handled.
	// Returns true if position is inside the viewport.
	bool viewportPosToLocalDocPosX(const QPointF& pos, float& docPosX) const;

	// Returns true, if doc position hit the viewport
	bool docPosToViewport(float docX, ViewportHitInfo& hitInfo) const;

	// Returns the vertical coordinates of a lane.
	RectY laneYBounds(int laneInd) const;
public:
	// generic requests
	void deleteRequest(bool isControl);

	// Refresh any data from cache.
	void refreshRequest();

public: // current sample
	const std::vector<short>& audioSamples() const;

	void setLastMousePressPos(const QPointF& localPos, bool isShiftPressed);
	long currentSampleInd() const;
	std::pair<long, long> cursor() const;
	
	// Returns a pair of indices specifying current cursor, so that start <= end.
	std::pair<long, long> cursorOrdered() const;
	TranscriberCursorKind cursorKind() const;

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

	PticaGovorun::SpeechAnnotation speechAnnot_;
	int currentMarkerInd_ = -1;

	// Level of detail for newly created markers.
	PticaGovorun::MarkerLevelOfDetail templateMarkerLevelOfDetail_ = PticaGovorun::MarkerLevelOfDetail::Word;
	PticaGovorun::SpeechLanguage templateMarkerSpeechLanguage_ = PticaGovorun::SpeechLanguage::Ukrainian;
	QPointF draggingLastViewportPos_; // used to avoid frequent repaints when mouse pos doesn't change
	bool isDraggingMarker_ = false;
	int draggingMarkerId_ = -1;
	float lastMousePressDocPosX_ = -1;

public:
	// Diagrams on sound wave.

	const wv::slice<const DiagramSegment> diagramSegments() const;
	void collectDiagramSegmentIndsOverlappingRange(std::pair<long, long> samplesRange, std::vector<int>& diagElemInds);
	
	// returns true if something was deleted
	bool deleteDiagramSegmentsAtCursor(std::pair<long, long> samplesRange);
private:
	std::vector<DiagramSegment> diagramSegments_;
public:

	const PticaGovorun::SpeechAnnotation& speechAnnotation() const;
	const std::vector<PticaGovorun::TimePointMarker>& frameIndMarkers() const;
	void setCurrentMarkerSpeaker(const std::wstring& speakerBriefId);
	
	template <typename MarkerPred>
	void transformMarkersIf(const std::vector<PticaGovorun::TimePointMarker>& markers, std::vector<MarkerRefToOrigin>& markersOfInterest, MarkerPred canSelectMarker);
	
	void insertNewMarkerAtCursorRequest();

	void insertNewMarker(const PticaGovorun::TimePointMarker& marker, bool updateCursor, bool updateViewportOffset);

	// true if marker was deleted
	bool deleteMarker(int markerInd);

	void selectMarkerClosestToCurrentCursorRequest();
	void selectMarkerForward();
	void selectMarkerBackward();
	void selectMarkerInternal(bool moveForward);
	int currentMarkerInd() const;
	void setCurrentMarkerTranscriptText(const QString& text);
	void setCurrentMarkerLevelOfDetail(PticaGovorun::MarkerLevelOfDetail levelOfDetail);
	void setCurrentMarkerStopOnPlayback(bool stopsPlayback);
	void setCurrentMarkerExcludePhase(boost::optional<ResourceUsagePhase> excludePhase);
	void setCurrentMarkerLang(PticaGovorun::SpeechLanguage lang);
	QString currentMarkerPhoneListString() const;
	void onPronIdPhoneticSpecChanged();

	void setTemplateMarkerLevelOfDetail(PticaGovorun::MarkerLevelOfDetail levelOfDetail);
	PticaGovorun::MarkerLevelOfDetail templateMarkerLevelOfDetail() const;
	PticaGovorun::SpeechLanguage templateMarkerSpeechLanguage() const;

	void dragMarkerStart(const QPointF& localPos, int markerId);
	void dragMarkerStop();
	void dragMarkerContinue(const QPointF& localPos);
private:
	void setCurrentMarkerIndInternal(int markerInd, bool updateCursor, bool updateViewportOffset);

public:	// recongizer
#ifdef PG_HAS_JULIUS
	void ensureRecognizerIsCreated();
	void recognizeCurrentSegmentJuliusRequest();
	void alignPhonesForCurrentSegmentJuliusRequest();
#endif
#if PG_HAS_SPHINX
	void recognizeCurrentSegmentSphinxRequest();
#endif
	void dumpSilence();
	void dumpSilence(long i1, long i2);
	void analyzeUnlabeledSpeech();
	void ensureWordToPhoneListVocabularyLoaded();
	void loadAuxiliaryPhoneticDictionaryRequest();

	size_t silencePadAudioFramesCount() const;
private:
	std::map<std::string, std::vector<float>> phoneNameToFeaturesVector_;

public: // segment composer
	void playComposingRecipeRequest(QString recipe);
private:
	std::vector<short> composedAudio_;

	// string assiciated with the range of samples (cursor)
	QString cursorTextToAlign_;

public:
	// navigation
	void setAudioMarkupNavigator(std::shared_ptr<AudioMarkupNavigator> audioMarkupNavigator);
	void navigateToMarkerRequest();
private:
	std::shared_ptr<AudioMarkupNavigator> audioMarkupNavigator_;

public:
	// phonetic dictionary
	void setPhoneticDictViewModel(std::shared_ptr<PticaGovorun::PhoneticDictionaryViewModel> phoneticDictViewModel);
	std::shared_ptr<PticaGovorun::PhoneticDictionaryViewModel> phoneticDictViewModel();
private:
	std::shared_ptr<PticaGovorun::PhoneticDictionaryViewModel> phoneticDictViewModel_;
	
public:
	void setNotificationService(std::shared_ptr<VisualNotificationService>);
	void nextNotification(const QString& message) const;
private:
	std::shared_ptr<VisualNotificationService> notificationService_;

private:
	std::vector<short> audioSamples_;
	float audioFrameRate_; // frame (sample) rate of current audio
    QString annotFilePathAbs_; // abs path to speech annotation (xml) file

	// Represents internal audio player's data.
	// The range (StartPlayingFrameInd;FinishPlayingFrameInd) of frames is played.
	struct SoundPlayerData
	{
		SpeechTranscriptionViewModel* transcriberViewModel;
		const short* AudioSouce;
		long CurPlayingFrameInd;

#if PG_DEBUG
		// used to restore cursor to initial position
		long StartPlayingFrameInd; // is not changed when playing
#endif

		// used to determine when to stop playing
		long FinishPlayingFrameInd; // is not changed when playing

		// True to move the cursor to the last playing sample index when playing stops.
		bool MoveCursorToLastPlayingPosition = false;

		PaStream *stream;
		std::atomic<bool> allowPlaying;
	};
	SoundPlayerData soundPlayerData_;
	std::atomic<bool> isPlaying_;
	long playingSampleInd_ = PticaGovorun::NullSampleInd; // the plyaing sample or -1 if audio is not playing

	// recognition
#ifdef PG_HAS_JULIUS
	std::shared_ptr<RecognizerNameHintProvider> recognizerNameHintProvider_;
	std::shared_ptr<JuliusRecognizerProvider> juliusRecognizerProvider_;
	JuliusToolWrapper* recognizer_ = nullptr;
	std::map<std::wstring, std::vector<std::string>> wordToPhoneListDict_;
	std::map<std::wstring, std::vector<std::string>> wordToPhoneListAuxiliaryDict_;
#endif
	std::vector<short> audioSegmentBuffer_; // the buffer to keep the padded audio segments
	
	// number of padding silence samples to the left and right of audio segment
	// The silence prefix/suffix for audio is equal across all recognizers.
	size_t silencePadAudioSamplesCount_ = 0; // pad with couple of windows of FrameSize
	float silenceMagnitudeThresh_ = -1;
	float silenceSmallWndMagnitudeThresh_ = -1;
	float silenceSlidingWindowDur_ = -1; // ms, the interval to check for silence
	float silenceSlidingWindowShift_ = -1;

public:
	float scale_;
};

}
#endif // TRANSCRIBERVIEWMODEL_H
