#include "AnnotationToolViewModel.h"
#include <memory>
#include <QSettings>
#include <QDir>
#include <QFileInfo>
#include "PresentationHelpers.h"
#include "SpeechTranscriptionValidation.h"

namespace PticaGovorun
{
	namespace
	{
		const char* IniFileName = "TranscriberUI.ini"; // where to store settings
		const char* RecentAnnotDir = "RecentAnnotDir"; // where xml annotation files reside
		const char* RecentAnnotFileRelPath = "RecentAnnotFileRelPath"; // relative path to last opened xml annot file
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
		QObject::connect(fileWorkspaceViewModel_.get(), SIGNAL(openAnnotFile(const std::wstring&)), this, SLOT(fileWorkspaceViewModel_openAnnotFile(const std::wstring&)));

		audioMarkupNavigator_ = std::make_shared<AudioMarkupNavigator>();
		phoneticDictModel_ = std::make_shared<PhoneticDictionaryViewModel>();

#ifdef PG_HAS_JULIUS
		juliusRecognizerProvider_ = std::make_shared<JuliusRecognizerProvider>();
#endif

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
		
		QString recentAnnotDir = settings.value(RecentAnnotDir, QString("")).toString();
		fileWorkspaceViewModel_->setAnnotDir(recentAnnotDir.toStdWString());

		QString recentAnnotFileRelPath = settings.value(RecentAnnotFileRelPath, QString("")).toString();
		if (!recentAnnotFileRelPath.isEmpty())
		{
			QString recentAnnotFilePath = QFileInfo(recentAnnotDir, recentAnnotFileRelPath).canonicalFilePath();
			fileWorkspaceViewModel_openAnnotFile(recentAnnotFilePath.toStdWString());
		}
	}

	void AnnotationToolViewModel::saveStateSettings()
	{
		QSettings settings(IniFileName, QSettings::IniFormat);

		auto annotDirQ = QString::fromStdWString(fileWorkspaceViewModel_->annotDir());
		if (!annotDirQ.isEmpty())
		{
			settings.setValue(RecentAnnotDir, annotDirQ);

			// save relative path to the last opened audio file
			auto transcrModel = activeTranscriptionModel();
			QString audioFileRelPath;
			if (transcrModel != nullptr)
			{
				QString recentAudioFilePath = transcrModel->annotFilePath();

				audioFileRelPath = QDir(annotDirQ).relativeFilePath(recentAudioFilePath);
			}

			settings.setValue(RecentAnnotFileRelPath, audioFileRelPath);
		}
		settings.sync();
	}

	void AnnotationToolViewModel::saveRequest()
	{
		for (auto _ : audioTranscriptionModels_)
			_->saveAudioMarkupToXml();
		if (phoneticDictModel_ != nullptr)
			phoneticDictModel_->saveDict();
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
			auto p = pTranscriptionModel->annotFilePath().toStdWString();
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

		QString recentAnnotDir = settings.value(RecentAnnotDir, QString("")).toString();

		std::vector<wchar_t> pathBuff;
		boost::wstring_ref recentAnnotDirRef = toWStringRef(recentAnnotDir, pathBuff);

		validateAllOnDiskSpeechAnnotations(recentAnnotDirRef, *phoneticDictModel_, checkMsgs);

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

	void AnnotationToolViewModel::navigateToMarkerRequest()
	{
		auto m = activeTranscriptionModel();
		if (m != nullptr)
			m->navigateToMarkerRequest();
	}

	void AnnotationToolViewModel::fileWorkspaceViewModel_openAnnotFile(const std::wstring& annotFilePath)
	{
		if (annotFilePath.empty())
			return;
		QString annotFilePathQ = QString::fromStdWString(annotFilePath);
		if (!QFileInfo(annotFilePathQ).exists())
		{
			nextNotification(QString("Can't find annotation file '%1'").arg(annotFilePathQ));
			return;
		}

		auto transcriberModel = std::make_shared<SpeechTranscriptionViewModel>();
		transcriberModel->setAnnotFilePath(annotFilePathQ);
		transcriberModel->setAudioMarkupNavigator(audioMarkupNavigator_);
		transcriberModel->setPhoneticDictViewModel(phoneticDictModel_);
		transcriberModel->init(serviceProvider_);
		transcriberModel->loadAnnotAndAudioFileRequest();
		audioTranscriptionModels_.push_back(transcriberModel);
		emit addedAudioTranscription(annotFilePath);

		activeAudioTranscriptionModelInd_ = (int)audioTranscriptionModels_.size() - 1;
		emit activeAudioTranscriptionChanged(activeAudioTranscriptionModelInd_);
	}

	void AnnotationToolViewModel::openAnnotDirRequest()
	{
		QString dirQ = newAnnotDirQuery();
		if (dirQ.isEmpty())
			return;
		fileWorkspaceViewModel_->setAnnotDir(dirQ.toStdWString());

		//
		activeAudioTranscriptionModelInd_ = -1;
		audioTranscriptionModels_.clear();
		emit activeAudioTranscriptionChanged(activeAudioTranscriptionModelInd_);
	}

	void AnnotationToolViewModel::closeAnnotDirRequest()
	{
		fileWorkspaceViewModel_->setAnnotDir(L"");

		//
		activeAudioTranscriptionModelInd_ = -1;
		audioTranscriptionModels_.clear();
		emit audioTranscriptionListCleared();
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

#ifdef PG_HAS_JULIUS
	std::string AnnotationToolViewModel::recognizerNameHint()
	{
		return recognizerName().toStdString();
	}

	std::shared_ptr<JuliusRecognizerProvider> AnnotationToolViewModel::juliusRecognizerProvider()
	{
		return juliusRecognizerProvider_;
	}
#endif
}