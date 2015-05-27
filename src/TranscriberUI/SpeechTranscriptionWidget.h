#ifndef AUDIOTRANSCRIPTIONWIDGET_H
#define AUDIOTRANSCRIPTIONWIDGET_H

#include <QWidget>
#include "SpeechTranscriptionViewModel.h"

namespace Ui {
class SpeechTranscriptionWidget;
}

namespace PticaGovorun
{
	class SpeechTranscriptionWidget : public QWidget
	{
		Q_OBJECT

	public:
		explicit SpeechTranscriptionWidget(QWidget *parent = 0);
		~SpeechTranscriptionWidget();

		void setTranscriberViewModel(std::shared_ptr<SpeechTranscriptionViewModel> value);
	protected:
		void resizeEvent(QResizeEvent*) override;
		void keyPressEvent(QKeyEvent*) override;

	private slots:
		void horizontalScrollBarSamples_valueChanged(int value);
		void transcriberModel_audioSamplesLoaded();
		void transcriberModel_audioSamplesChanged();
		void transcriberModel_docOffsetXChanged();
		void transcriberModel_cursorChanged(std::pair<long, long> oldCursor);
		void transcriberModel_currentMarkerIndChanged();
		void transcriberModel_playingSampleIndChanged(long oldPlayingSampleInd);
		void transcriberModel_lastMouseDocPosXChanged(float mouseDocPosX);

		void comboBoxSpeakerId_currentIndexChanged(int index);
		void radioButtonWordLevel_toggled(bool checked);
		void groupBoxLang_toggled(bool checked);
		void lineEditMarkerText_editingFinished();
		void checkBoxCurMarkerStopOnPlayback_toggled(bool checked);
		void checkBoxCurMarkerUseInTrain_toggled(bool checked);
	private:
		void updateSamplesSlider();
		void UpdateCursorUI();
		void UpdateSpeakerListUI();
	private:
		Ui::SpeechTranscriptionWidget *ui;
		std::shared_ptr<SpeechTranscriptionViewModel> transcriberModel_;
	};
}

#endif // AUDIOTRANSCRIPTIONWIDGET_H
