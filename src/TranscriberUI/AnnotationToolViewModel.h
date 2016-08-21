#pragma once
#include <vector>
#include <QObject>
#include "FileWorkspaceViewModel.h"
#include "SpeechTranscriptionViewModel.h"
#include "SharedServiceProvider.h"
#include "VisualNotificationService.h"
#include "JuliusRecognizerProvider.h"
#include "SpeechDataValidation.h"

namespace PticaGovorun
{
	class AnnotationToolViewModel : public QObject
#ifdef PG_HAS_JULIUS
		, public RecognizerNameHintProvider // represents the Julius recognizer name to 
#endif
	{
	private:
		Q_OBJECT
	public:
	signals:
		void addedAudioTranscription(const std::wstring& filePath);
		void activeAudioTranscriptionChanged(int ind);
		void audioTranscriptionRemoved(int tabIndex);
		void audioTranscriptionListCleared();

		// Notifies view to ask a user to set the new speech annotation directory.
		QString newAnnotDirQuery();
	public:
		AnnotationToolViewModel();
		~AnnotationToolViewModel();
		void init(std::shared_ptr<SharedServiceProvider> serviceProvider);
		void onClose();

		void closeAudioTranscriptionTab(int tabIndex);

		bool tryChangeSpeechProjDir(QString speechProjDir);

		// Declares a user intent to open new speech annotation directory.
		void openAnnotDirRequest();

		// Declares a user intent to close new speech annotation directory.
		void closeAnnotDirRequest();

		// Loads UI state.
		void loadStateSettings();

		// Saves UI state.
		void saveStateSettings();

		void saveRequest();

		// recognizer
		QString recognizerName() const;
		void setRecognizerName(const QString& filePath);
#ifdef PG_HAS_JULIUS
		std::string recognizerNameHint() override;
		std::shared_ptr<JuliusRecognizerProvider> juliusRecognizerProvider();
#endif
		std::shared_ptr<SpeechTranscriptionViewModel> activeTranscriptionModel();
		std::shared_ptr<SpeechTranscriptionViewModel> audioTranscriptionModelByFilePathAbs(const std::wstring& filePath);
		std::shared_ptr<FileWorkspaceViewModel> fileWorkspaceModel();

		void validateAllSpeechAnnotationRequest();

		std::shared_ptr<PhoneticDictionaryViewModel> phoneticDictModel();

		// play
		void playComposingRecipeRequest(QString recipe);

		// navigate
		void navigateToMarkerRequest();

	public:
		// Occurs when phonetic dialog closes and phonetic expansion of current marker's text must be updated.
		void onPronIdPhoneticSpecChanged();

		/// Checks whether speech working dir is opened.
		bool isSpeechProjectOpened() const;

	private slots:
		// file workspace
		void fileWorkspaceViewModel_openAnnotFile(const std::wstring& annotFilePath);
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

		std::shared_ptr<SpeechData> speechData_;
		std::shared_ptr<PhoneRegistry> phoneReg_;

		// recognition
		QString curRecognizerName_;
#ifdef PG_HAS_JULIUS
		std::shared_ptr<JuliusRecognizerProvider> juliusRecognizerProvider_;
#endif
	};

}