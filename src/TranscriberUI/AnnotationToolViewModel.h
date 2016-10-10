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
		void addedAudioTranscription(const boost::filesystem::path& filePath);
		void activeAudioTranscriptionChanged(int ind);
		void audioTranscriptionRemoved(int tabIndex);
		void audioTranscriptionListCleared();

		// Notifies view to ask a user to set the new speech annotation directory.
		QString newAnnotDirQuery();

		// commands
		void commandsListChanged();
	public:
		AnnotationToolViewModel();
		~AnnotationToolViewModel();
		void init(std::shared_ptr<SharedServiceProvider> serviceProvider);
		void onClose();

		void closeAudioTranscriptionTab(int tabIndex);

		bool tryChangeSpeechProjDir(const boost::filesystem::path& speechProjDir);

		// Declares a user intent to open new speech annotation directory.
		void openAnnotDirRequest();

		// Declares a user intent to close new speech annotation directory.
		void closeAnnotDirRequest();

		void load();

		// UI state
		void loadStateSettings();
		void saveStateSettings();

		void saveRequest();

		// recognizer
		boost::string_view recognizerName() const;
		void setRecognizerName(boost::string_view recogName);
#ifdef PG_HAS_JULIUS
		boost::string_view recognizerNameHint() override;
		std::shared_ptr<JuliusRecognizerProvider> juliusRecognizerProvider();
#endif
		std::shared_ptr<SpeechTranscriptionViewModel> activeTranscriptionModel();
		std::shared_ptr<SpeechTranscriptionViewModel> audioTranscriptionModelByAnnotFilePathAbs(const boost::filesystem::path& filePath);
		std::shared_ptr<FileWorkspaceViewModel> fileWorkspaceModel();

		void validateAllSpeechAnnotationRequest();

		std::shared_ptr<PhoneticDictionaryViewModel> phoneticDictModel();

	public:
		// play
		void playComposingRecipeRequest(boost::string_view recipe);
		bool processCommandList(boost::string_view recipe);
		void setCommandList(boost::string_view commandsList, bool updateView = true);
		boost::string_view commandList() const;
	private:
		std::string commandList_;

	public:
		// navigate
		void navigateToMarkerRequest();

	public:
		// Occurs when phonetic dialog closes and phonetic expansion of current marker's text must be updated.
		void onPronIdPhoneticSpecChanged();

		/// Checks whether speech working dir is opened.
		bool isSpeechProjectOpened() const;

	private slots:
		// file workspace
		void fileWorkspaceViewModel_openAnnotFile(const boost::filesystem::path& annotFilePath);
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
		std::string curRecognizerName_;
#ifdef PG_HAS_JULIUS
		std::shared_ptr<JuliusRecognizerProvider> juliusRecognizerProvider_;
#endif
	};

}