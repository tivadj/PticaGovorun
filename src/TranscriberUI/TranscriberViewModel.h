#ifndef TRANSCRIBERVIEWMODEL_H
#define TRANSCRIBERVIEWMODEL_H
#include <vector>
#include <QObject>
//#include "array_view.hpp"

class TranscriberViewModel : public QObject
{
    Q_OBJECT
public:
    TranscriberViewModel();

    void loadAudioFile();

    QString audioFilePath() const;
    void setAudioFilePath(const QString& filePath);

	//const arv::array_view<short> audioSamples() const
	//{
	//	return arv::array_view<short>(audioSamples_);
	//}
	const std::vector<short>& audioSamples() const;

signals:
	void nextNotification(const QString& message);
	void audioSamplesChanged();
private:
	std::vector<short> audioSamples_;
    QString audioFilePathAbs_;
public:
	float scale_;
};

#endif // TRANSCRIBERVIEWMODEL_H
