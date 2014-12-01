#ifndef TRANSCRIBERVIEWMODEL_H
#define TRANSCRIBERVIEWMODEL_H
#include <vector>
#include <atomic>
#include <memory>
#include <QObject>
//#include "array_view.hpp"
#include <portaudio.h>
#include "XmlAudioMarkup.h"
#include "JuliusToolNativeWrapper.h"

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
	void currentFrameIndChanged(long oldValue);
public:
    TranscriberViewModel();

    void loadAudioFile();

	// Plays the current segment of audio.
	// restoreCurFrameInd=true, if current frame must be restored when playing finishes.
	void soundPlayerPlay(long startPlayingFrameInd, long finishPlayingFrameInd, bool restoreCurFrameInd);
	// outLeftMarkerInd (may be null): returns the index of current segment's left marker.
	std::tuple<long, long> getFrameRangeToPlay(long curFrameInd, SegmentStartFrameToPlayChoice startFrameChoice, int* outLeftMarkerInd = nullptr);

	void soundPlayerPlayCurrentSegment(SegmentStartFrameToPlayChoice startFrameChoice);
	void soundPlayerPause();
	void soundPlayerPlay();
	// Plays from the current frame index to the next frame marker or the end of audio, what will occur earlier.
	void soundPlayerTogglePlayPause();

	//

	void loadAudioMarkupFromXml();
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
	const std::vector<short>& audioSamples() const;

	const std::vector<PticaGovorun::TimePointMarker>& frameIndMarkers() const;

	void setLastMousePressPos(const QPointF& localPos);
	long currentFrameInd() const;
	void setCurrentFrameInd(long value);
	void recognizeCurrentSegment();
private:
	// returns the index in the closest to the left time point marker in markers collection
	// returns -1 if there is no markers to the left of 'frameInd' or audio samples were not loaded.
	int findLeftCloseMarkerInd(long frameInd);
private:
	std::vector<short> audioSamples_;
    QString audioFilePathAbs_;
	float docOffsetX_ = 0;
	
	// number of pixels to the left/right of samples graph
	// together with the samples graph is treated as the document
	float docPaddingPix_ = 100;
	long currentFrameInd_ = -1;
	std::vector<PticaGovorun::TimePointMarker> frameIndMarkers_;

	// Represents internal audio player's data.
	// The range (StartPlayingFrameInd;FinishPlayingFrameInd) of frames is played.
	struct SoundPlayerData
	{
		TranscriberViewModel* transcriberViewModel;
		long CurPlayingFrameInd;

#if PG_DEBUG
		// used to restore cursor to initial position
		long StartPlayingFrameInd; // is not changed when playing
#endif

		// used to determine when to stop playing
		long FinishPlayingFrameInd; // is not changed when playing

		// PticaGovorun::PGFrameIndNull if CurFrameInd should not be restored
		long RestoreCurrentFrameInd; // CurFrameInd to restore when playing finishes
		PaStream *stream;
		std::atomic<bool> allowPlaying;
	};
	SoundPlayerData soundPlayerData_;
	std::atomic<bool> isPlaying_;

	// recognition
	std::unique_ptr<PticaGovorun::JuliusToolWrapper> recognizer_;
	
public:
	float scale_;
};

#endif // TRANSCRIBERVIEWMODEL_H
