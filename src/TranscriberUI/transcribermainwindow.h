#ifndef TRANSCRIBERMAINWINDOW_H
#define TRANSCRIBERMAINWINDOW_H

#include <memory>
#include <QMainWindow>
#include "TranscriberViewModel.h"

namespace Ui {
class TranscriberMainWindow;
}

class TranscriberMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit TranscriberMainWindow(QWidget *parent = 0);
    ~TranscriberMainWindow();

    void updateUI();
private:
	void updateSamplesSlider();

private slots:
	void pushButtonLoad_Clicked();
	void pushButtonSaveAudioAnnot_Clicked();
	void pushButtonPlay_Clicked();
	void pushButtonPause_Clicked();
	void radioButtonWordLevel_toggled(bool checked);
	void lineEditFileName_editingFinished();
	void lineEditRecognizerName_editingFinished();
	void horizontalScrollBarSamples_valueChanged(int value);
	void transcriberModel_audioSamplesLoaded();
	void transcriberModel_nextNotification(const QString& message);
	void transcriberModel_audioSamplesChanged();
	void transcriberModel_docOffsetXChanged();
	void transcriberModel_lastMouseDocPosXChanged(float mouseDocPosX);
	void UpdateDocPosXAndFrameInd(float mouseDocPosX, float sampleInd);
	void transcriberModel_currentFrameIndChanged(long oldValue);
	void transcriberModel_currentMarkerIndChanged();
	void lineEditMarkerText_editingFinished();
	void checkBoxCurMarkerStopOnPlayback_toggled(bool checked);
private:
    Ui::TranscriberMainWindow *ui;
    std::shared_ptr<TranscriberViewModel> transcriberModel_;
};

#endif // TRANSCRIBERMAINWINDOW_H
