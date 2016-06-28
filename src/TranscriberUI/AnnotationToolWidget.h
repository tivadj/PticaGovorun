#ifndef TRANSCRIBERMAINWINDOW_H
#define TRANSCRIBERMAINWINDOW_H

#include <memory>
#include <QMainWindow>
#include "SpeechTranscriptionViewModel.h"
#include "PhoneticDictionaryViewModel.h"
#include "FileWorkspaceViewModel.h"
#include "AnnotationToolViewModel.h"
#include "VisualNotificationService.h"
#include "SharedServiceProvider.h"

namespace Ui {
class AnnotationToolMainWindow;
}

namespace PticaGovorun
{
	class AnnotationToolMainWindow : public QMainWindow,
		public VisualNotificationService
	{
		Q_OBJECT
	public:
		explicit AnnotationToolMainWindow(QWidget *parent = 0);
		~AnnotationToolMainWindow();

		void updateUI();

		void nextNotification(const QString&) override;

	protected:
		void keyPressEvent(QKeyEvent*) override;
		void closeEvent(QCloseEvent*) override;

	private slots:
		void actionOpenAnnotDir_triggered();
		void actionCloseAnnotDir_triggered();
		void tabWidgetSpeechTranscriptionTabs_tabCloseRequested(int index);
	
		// REMOVE
		void pushButtonSaveAudioAnnot_Clicked();

		void lineEditRecognizerName_editingFinished();

		// segment composer
		void pushButtonSegmentComposerPlay_Clicked();

		// file workspace
		void audioTranscriptionToolModel_addedAudioTranscription(const std::wstring& filePath);
		void audioTranscriptionToolModel_activeAudioTranscriptionChanged(int ind);
		void audioTranscriptionToolModel_audioTranscriptionRemoved(int ind);
		void audioTranscriptionToolModel_audioTranscriptionListCleared();
		QString fileWorkspaceModel_newAnnotDirQuery();

	private:
		class SharedServiceProviderImpl : public SharedServiceProvider
		{
		public:
			explicit SharedServiceProviderImpl(AnnotationToolMainWindow& outer);
			std::shared_ptr<VisualNotificationService> notificationService() override;
			std::shared_ptr<JuliusRecognizerProvider> juliusRecognizerProvider() override;
			std::shared_ptr<RecognizerNameHintProvider> recognizerNameHintProvider() override;
		private:
			//std::reference_wrapper<TranscriberMainWindow> outer_; // TODO: type_traits(584): error C2139: 'PticaGovorun::TranscriberMainWindow' : an undefined class is not allowed as an argument to compiler intrinsic type trait '__is_abstract'
			AnnotationToolMainWindow& outer_;
		};
	private:
		Ui::AnnotationToolMainWindow *ui;
		std::shared_ptr<SharedServiceProviderImpl> sharedServiceProviderImpl_;
		std::shared_ptr<AnnotationToolViewModel> annotationToolModel_;
		std::shared_ptr<FileWorkspaceViewModel> fileWorkspaceModel_;
	};
}
#endif // TRANSCRIBERMAINWINDOW_H
