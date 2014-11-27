#ifndef TRANSCRIBERVIEWMODEL_H
#define TRANSCRIBERVIEWMODEL_H
#include <vector>
#include <QObject>
//#include "array_view.hpp"

// The document is a graph for samples plus padding to the left and right.
// The samples graph is painted with current 'scale'.
class TranscriberViewModel : public QObject
{
    Q_OBJECT
public:
signals:
    void nextNotification(const QString& message);
    void audioSamplesChanged();
	void docOffsetXChanged();
	void lastMouseDocPosXChanged(float mouseDocPosX);
public:
    TranscriberViewModel();

    void loadAudioFile();

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
	float firstVisibleSampleInd() const;
	float docPosXToSampleInd(float docPosX) const;
	float sampleIndToDocPosX(int sampleInd) const;
	//const arv::array_view<short> audioSamples222() const
	//{
	//  return arv::array_view<short>(audioSamples_);
	//	return arv::array_view<short>(std::begin(audioSamples_), std::end(audioSamples_));
	//}
	const std::vector<short>& audioSamples() const;

	void setLastMousePressPos(const QPointF& localPos);
private:
	std::vector<short> audioSamples_;
    QString audioFilePathAbs_;
	float docOffsetX_ = 0;
	
	// number of pixels to the left/right of samples graph
	// together with the samples graph is treated as the document
	float docPaddingPix_ = 100;
public:
	float scale_;
};

#endif // TRANSCRIBERVIEWMODEL_H
