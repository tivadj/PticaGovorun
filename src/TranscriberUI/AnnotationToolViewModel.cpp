#include "AnnotationToolViewModel.h"
#include <memory>
#include <QSettings>
#include "PresentationHelpers.h"
#include "SpeechTranscriptionValidation.h"

namespace PticaGovorun
{
	namespace
	{
		const char* IniFileName = "TranscriberUI.ini"; // where to store settings
		const char* SpeechDataRoot = "SpeechDataRoot"; // where wav and annotation files reside
		const char* WavFilePath = "WavFilePath"; // last opened wav file path
	}

	AnnotationToolViewModel::AnnotationToolViewModel()
	{
	}


	AnnotationToolViewModel::~AnnotationToolViewModel()
	{
	}

	void AnnotationToolViewModel::init(std::shared_ptr<SharedServiceProvider> serviceProvider)
	{
		serviceProvider_ = serviceProvider;
		notificationService_ = serviceProvider->notificationService();

		fileWorkspaceViewModel_ = std::make_shared<FileWorkspaceViewModel>();
		QObject::connect(fileWorkspaceViewModel_.get(), SIGNAL(openAudioFile(const std::wstring&)), this, SLOT(fileWorkspaceViewModel_openAudioFile(const std::wstring&)));

		audioMarkupNavigator_ = std::make_shared<AudioMarkupNavigator>();
		phoneticDictModel_ = std::make_shared<PhoneticDictionaryViewModel>();

		juliusRecognizerProvider_ = std::make_shared<JuliusRecognizerProvider>();

		loadStateSettings();
	}

	void AnnotationToolViewModel::onClose()
	{
		saveStateSettings();
	}

	void AnnotationToolViewModel::loadStateSettings()
	{
		curRecognizerName_ = "shrekky";

		QSettings settings(IniFileName, QSettings::IniFormat); // reads from current directory
		
		QString audioDataDir = settings.value(SpeechDataRoot, QString("")).toString();
		fileWorkspaceViewModel_->setWorkingDirectory(audioDataDir.toStdWString());

		QString speechWavDirVar = settings.value(WavFilePath, QString("")).toString();
		if (!speechWavDirVar.isEmpty())
			fileWorkspaceViewModel_openAudioFile(speechWavDirVar.toStdWString());
	}

	void AnnotationToolViewModel::saveStateSettings()
	{
		QSettings settings(IniFileName, QSettings::IniFormat);
		
		QString lastAudioFilePath;
		auto transcrModel = activeTranscriptionModel();
		if (transcrModel != nullptr)
			lastAudioFilePath = transcrModel->audioFilePath();

		settings.setValue(WavFilePath, lastAudioFilePath);
		settings.sync();
	}

	void AnnotationToolViewModel::saveRequest()
	{
		for (auto _ : audioTranscriptionModels_)
			_->saveAudioMarkupToXml();
	}

	std::shared_ptr<SpeechTranscriptionViewModel> AnnotationToolViewModel::activeTranscriptionModel()
	{
		if (activeAudioTranscriptionModelInd_ == -1)
			return nullptr;
		return audioTranscriptionModels_[activeAudioTranscriptionModelInd_];
	}

	std::shared_ptr<SpeechTranscriptionViewModel> AnnotationToolViewModel::audioTranscriptionModelByFilePathAbs(const std::wstring& filePath)
	{
		auto it = std::find_if(audioTranscriptionModels_.begin(), audioTranscriptionModels_.end(), [&filePath](std::shared_ptr<SpeechTranscriptionViewModel> pTranscriptionModel)
		{
			auto p = pTranscriptionModel->audioFilePath().toStdWString();
			return p == filePath;
		});
		if (it == audioTranscriptionModels_.end())
			return nullptr;
		return *it;
	}

	std::shared_ptr<FileWorkspaceViewModel> AnnotationToolViewModel::fileWorkspaceModel()
	{
		return fileWorkspaceViewModel_;
	}

	void AnnotationToolViewModel::validateAllSpeechAnnotationRequest()
	{
		QStringList checkMsgs;

		// in-memory data validation
		auto m = activeTranscriptionModel();
		if (m != nullptr)
			validateSpeechAnnotation(m->speechAnnotation(), *phoneticDictModel_, checkMsgs);

		// validate on-disk data

		QSettings settings(IniFileName, QSettings::IniFormat); // reads from current directory

		QString audioDataDir = settings.value(SpeechDataRoot, QString("")).toString();

		std::vector<wchar_t> pathBuff;
		boost::wstring_ref audioDataPath = toWStringRef(audioDataDir, pathBuff);

		validateAllOnDiskSpeechAnnotations(audioDataPath, *phoneticDictModel_, checkMsgs);

		QString msg;
		if (checkMsgs.isEmpty())
			msg = "Validation succeeded";
		else
		{
			msg = QString("Validation failed\n");
			msg.append(checkMsgs.join('\n'));
		}
		nextNotification(formatLogLineWithTime(msg));
	}

	std::shared_ptr<PticaGovorun::PhoneticDictionaryViewModel> AnnotationToolViewModel::phoneticDictModel()
	{
		return phoneticDictModel_;
	}

	void AnnotationToolViewModel::playComposingRecipeRequest(QString recipe)
	{
		auto m = activeTranscriptionModel();
		if (m != nullptr)
			m->playComposingRecipeRequest(recipe);
	}

	void AnnotationToolViewModel::fileWorkspaceViewModel_openAudioFile(const std::wstring& filePath)
	{
		QString filePathQ = QString::fromStdWString(filePath);

		auto transcriberModel = std::make_shared<SpeechTranscriptionViewModel>();
		transcriberModel->setAudioMarkupNavigator(audioMarkupNavigator_);
		transcriberModel->setPhoneticDictViewModel(phoneticDictModel_);
		transcriberModel->init(serviceProvider_);
		transcriberModel->setAudioFilePath(filePathQ);
		transcriberModel->loadAudioFileRequest();
		audioTranscriptionModels_.push_back(transcriberModel);
		emit addedAudioTranscription(filePath);

		activeAudioTranscriptionModelInd_ = (int)audioTranscriptionModels_.size() - 1;
		emit activeAudioTranscriptionChanged(activeAudioTranscriptionModelInd_);
	}

	void AnnotationToolViewModel::nextNotification(const QString& message) const
	{
		if (notificationService_ != nullptr)
			notificationService_->nextNotification(message);
	}

	void AnnotationToolViewModel::onPronIdPhoneticSpecChanged()
	{
		for (auto& m : audioTranscriptionModels_)
			m->onPronIdPhoneticSpecChanged();
	}

	void AnnotationToolViewModel::closeAudioTranscriptionTab(int tabIndex)
	{
		bool okIndex = tabIndex >= 0 || tabIndex < audioTranscriptionModels_.size();
		PG_DbgAssert(okIndex);
		if (!okIndex)
			return;

		bool toInitActiveTab = activeAudioTranscriptionModelInd_ == tabIndex;

		audioTranscriptionModels_.erase(audioTranscriptionModels_.begin() + tabIndex);

		if (toInitActiveTab)
		{
			int newValue;
			if (audioTranscriptionModels_.empty())
				newValue = -1;
			else
			{
				newValue = tabIndex;
				if (newValue >= audioTranscriptionModels_.size())
					newValue = audioTranscriptionModels_.size() - 1;
			}
			activeAudioTranscriptionModelInd_ = newValue;
		}

		emit audioTranscriptionRemoved(tabIndex);
		emit activeAudioTranscriptionChanged(activeAudioTranscriptionModelInd_);
	}

	QString AnnotationToolViewModel::recognizerName() const
	{
		return curRecognizerName_;
	}

	void AnnotationToolViewModel::setRecognizerName(const QString& filePath)
	{
		curRecognizerName_ = filePath;
	}

	std::string AnnotationToolViewModel::recognizerNameHint()
	{
		return recognizerName().toStdString();
	}

	std::shared_ptr<JuliusRecognizerProvider> AnnotationToolViewModel::juliusRecognizerProvider()
	{
		return juliusRecognizerProvider_;
	}
}