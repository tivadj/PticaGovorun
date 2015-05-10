#pragma once
#include <vector>
#include <QObject>
#include "FileWorkspaceViewModel.h"
#include "SpeechTranscriptionViewModel.h"
#include "SharedServiceProvider.h"
#include "VisualNotificationService.h"
#include "JuliusRecognizerProvider.h"

namespace PticaGovorun
{
	class AnnotationToolViewModel : public QObject,
		public RecognizerNameHintProvider
	{
	private:
		Q_OBJECT
	public:
	signals:
		void addedAudioTranscription(const std::wstring& filePath);
		void activeAudioTranscriptionChanged(int ind);
		void audioTranscriptionRemoved(int tabIndex);

	public:
		AnnotationToolViewModel();
		~AnnotationToolViewModel();
		void init(std::shared_ptr<SharedServiceProvider> serviceProvider);
		void onClose();

		void closeAudioTranscriptionTab(int tabIndex);

		// Loads UI state.
		void loadStateSettings();

		// Saves UI state.
		void saveStateSettings();

		void saveRequest();

		// recognizer
		QString recognizerName() const;
		void setRecognizerName(const QString& filePath);
		std::string recognizerNameHint() override;
		std::shared_ptr<JuliusRecognizerProvider> juliusRecognizerProvider();

		std::shared_ptr<SpeechTranscriptionViewModel> activeTranscriptionModel();
		std::shared_ptr<SpeechTranscriptionViewModel> audioTranscriptionModelByFilePathAbs(const std::wstring& filePath);
		std::shared_ptr<FileWorkspaceViewModel> fileWorkspaceModel();

		void validateAllSpeechAnnotationRequest();

		std::shared_ptr<PhoneticDictionaryViewModel> phoneticDictModel();

		// play
		void playComposingRecipeRequest(QString recipe);
	private slots:
		// file workspace
		void fileWorkspaceViewModel_openAudioFile(const std::wstring& filePath);
	private:
		void nextNotification(const QString& message) const;

	private:
		std::shared_ptr<SharedServiceProvider> serviceProvider_;
		std::shared_ptr<VisualNotificationService> notificationService_;
		std::shared_ptr<FileWorkspaceViewModel> fileWorkspaceViewModel_;
		std::shared_ptr<AudioMarkupNavigator> audioMarkupNavigator_;
		std::shared_ptr<PhoneticDictionaryViewModel> phoneticDictModel_;

		std::vector<std::shared_ptr<SpeechTranscriptionViewModel>> audioTranscriptionModels_;
		int activeAudioTranscriptionModelInd_ = -1;

		// recognition
		QString curRecognizerName_;
		std::shared_ptr<JuliusRecognizerProvider> juliusRecognizerProvider_;
	};

}