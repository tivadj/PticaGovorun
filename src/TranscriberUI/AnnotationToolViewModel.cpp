#include "AnnotationToolViewModel.h"
#include <memory>
#include <QSettings>
#include <QDir>
#include <QFileInfo>
#include "PresentationHelpers.h"
#include "assertImpl.h"
#include "SpeechDataValidation.h"
#include "FileHelpers.h"

namespace PticaGovorun
{
	namespace
	{
		const char* RecentSpeechProjDir = "RecentSpeechProjDir"; // where xml annotation files reside
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

#ifdef PG_HAS_JULIUS
		juliusRecognizerProvider_ = std::make_shared<JuliusRecognizerProvider>();
#endif

		load();
	}

	void AnnotationToolViewModel::onClose()
	{
		saveStateSettings();
	}

	void AnnotationToolViewModel::load()
	{
		auto commandFilePath = AppHelpers::mapPathBfs("pgdata/commandWindow.txt");
		ErrMsgList errMsg;
		if (!readAllText(commandFilePath, commandList_, &errMsg))
		{
			nextNotification(QString("Can't read the content of command window. %1").arg(combineErrorMessages(errMsg)));
		}
		else
		{
			emit commandsListChanged();
		}

		//
		loadStateSettings();
	}

	void AnnotationToolViewModel::loadStateSettings()
	{
		curRecognizerName_ = "shrekky";

		QString speechProjDir = AppHelpers::configParamQString(RecentSpeechProjDir, QString(""));
		if (tryChangeSpeechProjDir(speechProjDir))
		{
			QString recentAnnotFileRelPath = AppHelpers::configParamQString(RecentAnnotFileRelPath, QString(""));
			if (!recentAnnotFileRelPath.isEmpty())
			{
				QString recentAnnotFilePath = QFileInfo(speechProjDir, recentAnnotFileRelPath).canonicalFilePath();
				fileWorkspaceViewModel_openAnnotFile(recentAnnotFilePath.toStdWString());
			}
		}
	}

	void AnnotationToolViewModel::saveStateSettings()
	{
		QString iniAbsPath = AppHelpers::appIniFilePathAbs();
		QSettings settings(iniAbsPath, QSettings::IniFormat);

		auto annotDirQ = QString::fromStdWString(speechData_->speechProjDir().wstring());
		if (!annotDirQ.isEmpty())
		{
			settings.setValue(RecentSpeechProjDir, annotDirQ);

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
		if (speechData_ != nullptr)
			speechData_->saveDict();
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
		if (!isSpeechProjectOpened())
			return;

		QStringList checkMsgs;

		// in-memory data validation
		auto m = activeTranscriptionModel();
		if (m != nullptr)
			speechData_->validateOneSpeechAnnot(m->speechAnnotation(), &checkMsgs);

		// validate on-disk data
		bool valid = speechData_->validate(&checkMsgs);

		QString msg;
		if (valid)
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
	
	bool AnnotationToolViewModel::processCommandList(boost::string_view recipe)
	{
		static const int TimePrec = 4;
		static const boost::string_view annotTotalDurationH = "annot.TotalDurationH";
		static const boost::string_view setCurRecogStr = "setCurRecog";
		
		if (recipe.find(annotTotalDurationH.data(), 0, annotTotalDurationH.size()) != std::string::npos)
		{
			if (speechData_ == nullptr)
				return false;

			auto projDir = speechData_->speechProjDir();
			auto annotDir = speechData_->speechAnnotDirPath();
			auto audioDir = speechData_->speechProjDir() / "SpeechAudio";

			//
			static const boost::string_view current = "current";
			auto folderOrAudioFilePath = audioDir;
			if (recipe.find(current.data(), 0, current.size()) != std::string::npos)
			{
				auto activeModel = activeTranscriptionModel();
				if (activeModel != nullptr)
				{
					folderOrAudioFilePath = activeModel->audioFilePathAbs();
				}
			}

			auto segAccept = [](const AnnotatedSpeechSegment& seg)->bool
			{
				if (seg.TranscriptText.find(L"#") != std::wstring::npos) // segments to ignore
					return false;
				return true;
			};

			std::vector<AnnotatedSpeechSegment> segments;
			ErrMsgList errMsg;
			bool removeSilenceAnnot = true;
			if (!loadSpeechAndAnnotation(toQString(folderOrAudioFilePath.wstring()), audioDir.wstring(), annotDir.wstring(), MarkerLevelOfDetail::Word, false, removeSilenceAnnot, segAccept, segments, &errMsg))
			{
				nextNotification(combineErrorMessages(errMsg));
				return false;
			}

			double totalDurH = 0;
			for (const auto& seg : segments)
			{
				ptrdiff_t numSamples = seg.EndMarker.SampleInd - seg.StartMarker.SampleInd;
				PG_DbgAssert(seg.SampleRate != -1);
				double dur = numSamples / static_cast<double>(seg.SampleRate); // seconds
				double durH = dur / 3600; // hours
				totalDurH += durH;
			}

			auto msg = QString("%1=%2").arg(utf8ToQString(annotTotalDurationH)).arg(totalDurH, 0, 'f', TimePrec);
			nextNotification(msg);
			return true;
		}
		else if (recipe.find(setCurRecogStr.data(), 0, setCurRecogStr.size()) != std::string::npos)
		{
			auto spacePos = recipe.find_first_of(' ');
			if (spacePos != std::string::npos)
			{
				auto recogName = recipe.substr(spacePos + 1);
				setRecognizerName(recogName);
			}
			return true;
		}
		return false;
	}

	void AnnotationToolViewModel::setCommandList(boost::string_view commandsList, bool updateView)
	{
		if (commandList_ != commandsList)
		{
			commandList_.assign(commandsList.data(), commandsList.size());
			if (updateView)
				emit commandsListChanged();
		}
	}

	boost::string_view AnnotationToolViewModel::commandList() const
	{
		return commandList_;
	}

	void AnnotationToolViewModel::playComposingRecipeRequest(boost::string_view recipe)
	{
		// reuse audio composer for command processing
		if (processCommandList(recipe))
			return;

		auto m = activeTranscriptionModel();
		if (m != nullptr)
			m->playComposingRecipeRequest(utf8ToQString(commandList_));
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
		transcriberModel->setSpeechData(speechData_);
		transcriberModel->init(serviceProvider_);
		transcriberModel->loadAnnotAndAudioFileRequest();
		audioTranscriptionModels_.push_back(transcriberModel);
		emit addedAudioTranscription(annotFilePath);

		activeAudioTranscriptionModelInd_ = (int)audioTranscriptionModels_.size() - 1;
		emit activeAudioTranscriptionChanged(activeAudioTranscriptionModelInd_);
	}

	bool AnnotationToolViewModel::tryChangeSpeechProjDir(QString speechProjDir)
	{
		if (speechProjDir.isEmpty())
			return false;

		//
		static const size_t WordsCount = 200000;
		auto stringArena = std::make_shared<GrowOnlyPinArena<wchar_t>>(WordsCount * 6); // W*C, W words, C chars per word

		//
		phoneReg_ = std::make_shared<PhoneRegistry>();
		bool allowSoftHardConsonant = true;
		bool allowVowelStress = true;
		phoneReg_->setPalatalSupport(PalatalSupport::AsPalatal);
		initPhoneRegistryUk(*phoneReg_, allowSoftHardConsonant, allowVowelStress);

		//
		speechData_ = std::make_shared<SpeechData>(speechProjDir.toStdWString());
		speechData_->setStringArena(stringArena);
		speechData_->setPhoneReg(phoneReg_);


		phoneticDictModel_ = std::make_shared<PhoneticDictionaryViewModel>(speechData_, phoneReg_);

		auto annotDir = speechData_->speechAnnotDirPath();
		fileWorkspaceViewModel_->setAnnotDir(annotDir.wstring());

		//
		activeAudioTranscriptionModelInd_ = -1;
		audioTranscriptionModels_.clear();
		emit activeAudioTranscriptionChanged(activeAudioTranscriptionModelInd_);
		return true;
	}

	void AnnotationToolViewModel::openAnnotDirRequest()
	{
		QString dirQ = newAnnotDirQuery();
		tryChangeSpeechProjDir(dirQ);
	}

	void AnnotationToolViewModel::closeAnnotDirRequest()
	{
		fileWorkspaceViewModel_->setAnnotDir(L"");
		speechData_.reset();

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

	bool AnnotationToolViewModel::isSpeechProjectOpened() const
	{
		return speechData_ != nullptr;
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

	boost::string_view AnnotationToolViewModel::recognizerName() const
	{
		return curRecognizerName_;
	}

	void AnnotationToolViewModel::setRecognizerName(boost::string_view recogName)
	{
		curRecognizerName_.assign(recogName.data(), recogName.size());
	}

#ifdef PG_HAS_JULIUS
	boost::string_view AnnotationToolViewModel::recognizerNameHint()
	{
		return recognizerName();
	}

	std::shared_ptr<JuliusRecognizerProvider> AnnotationToolViewModel::juliusRecognizerProvider()
	{
		return juliusRecognizerProvider_;
	}
#endif
}
