#include <sstream>
#include <array>
#include <memory>
#include "AnnotationToolWidget.h"
#include "ui_AnnotationToolWidget.h"
#include <QDebug>
#include <QMouseEvent>
#include <QSize>
#include <QFileDialog>
#include <QTextDocumentFragment>
#include "PhoneticDictionaryDialog.h"
#include "SpeechTranscriptionWidget.h"
#include "ComponentsInfrastructure.h"
#include "assertImpl.h"

namespace PticaGovorun
{
	AnnotationToolMainWindow::SharedServiceProviderImpl::SharedServiceProviderImpl(AnnotationToolMainWindow& outer) 
		: outer_(outer)
	{
	}

	std::shared_ptr<VisualNotificationService> AnnotationToolMainWindow::SharedServiceProviderImpl::notificationService()
	{
		auto p = dynamic_cast<VisualNotificationService*>(&outer_);
		return std::shared_ptr<VisualNotificationService>(p, NoDeleteFunctor<VisualNotificationService>());
	}

#ifdef PG_HAS_JULIUS
	std::shared_ptr<JuliusRecognizerProvider> AnnotationToolMainWindow::SharedServiceProviderImpl::juliusRecognizerProvider()
	{
		return outer_.annotationToolModel_->juliusRecognizerProvider();
	}

	std::shared_ptr<RecognizerNameHintProvider> AnnotationToolMainWindow::SharedServiceProviderImpl::recognizerNameHintProvider()
	{
		auto p = dynamic_cast<RecognizerNameHintProvider*>(outer_.annotationToolModel_.get());
		return std::shared_ptr<RecognizerNameHintProvider>(p, NoDeleteFunctor<RecognizerNameHintProvider>());
	}
#endif

	AnnotationToolMainWindow::AnnotationToolMainWindow(QWidget *parent) :
		QMainWindow(parent),
		ui(new Ui::AnnotationToolMainWindow)
	{
		ui->setupUi(this);

		ui->tabWidgetSpeechTranscriptionTabs->clear();

		QObject::connect(ui->actionSave, SIGNAL(triggered()), this, SLOT(saveAction_triggered()));
		QObject::connect(ui->tabWidgetSpeechTranscriptionTabs, SIGNAL(tabCloseRequested(int)), this, SLOT(tabWidgetSpeechTranscriptionTabs_tabCloseRequested(int)));
		QObject::connect(ui->actionOpenAnnotDir, SIGNAL(triggered()), this, SLOT(actionOpenAnnotDir_triggered()));
		QObject::connect(ui->actionCloseAnnotDir, SIGNAL(triggered()), this, SLOT(actionCloseAnnotDir_triggered()));

		//
		sharedServiceProviderImpl_ = std::make_shared<SharedServiceProviderImpl>(*this);
		annotationToolModel_ = std::make_shared<AnnotationToolViewModel>();
		QObject::connect(annotationToolModel_.get(), SIGNAL(addedAudioTranscription(const boost::filesystem::path&)), this,
			SLOT(audioTranscriptionToolModel_addedAudioTranscription(const boost::filesystem::path&)));
		QObject::connect(annotationToolModel_.get(), SIGNAL(activeAudioTranscriptionChanged(int)), this, 
			SLOT(audioTranscriptionToolModel_activeAudioTranscriptionChanged(int)));
		QObject::connect(annotationToolModel_.get(), SIGNAL(audioTranscriptionRemoved(int)), this, 
			SLOT(audioTranscriptionToolModel_audioTranscriptionRemoved(int)));
		QObject::connect(annotationToolModel_.get(), SIGNAL(audioTranscriptionListCleared()), this,
			SLOT(audioTranscriptionToolModel_audioTranscriptionListCleared()));
		QObject::connect(annotationToolModel_.get(), SIGNAL(newAnnotDirQuery()), this, SLOT(fileWorkspaceModel_newAnnotDirQuery()));
		QObject::connect(annotationToolModel_.get(), SIGNAL(commandsListChanged()), this, SLOT(annotationToolModel_commandsListChanged()));
		annotationToolModel_->init(sharedServiceProviderImpl_);

		std::shared_ptr<FileWorkspaceViewModel> fileWorkspaceModel = annotationToolModel_->fileWorkspaceModel();
		ui->widgetFileWorkspace->setFileWorkspaceViewModel(fileWorkspaceModel);

		updateUI();
	}

	AnnotationToolMainWindow::~AnnotationToolMainWindow()
	{
		delete ui;
	}

	void AnnotationToolMainWindow::updateUI()
	{
	}

	void AnnotationToolMainWindow::actionOpenAnnotDir_triggered()
	{
		annotationToolModel_->openAnnotDirRequest();
	}

	void AnnotationToolMainWindow::actionCloseAnnotDir_triggered()
	{
		annotationToolModel_->closeAnnotDirRequest();
	}

	void AnnotationToolMainWindow::tabWidgetSpeechTranscriptionTabs_tabCloseRequested(int index)
	{
		annotationToolModel_->closeAudioTranscriptionTab(index);
	}

	void AnnotationToolMainWindow::saveAction_triggered()
	{
		annotationToolModel_->saveRequest();
	}

	void AnnotationToolMainWindow::pushButtonSegmentComposerPlay_Clicked()
	{
		QString entireRecipe = ui->plainTextEditAudioSegmentsComposer->toPlainText();

		// update model's text from UI
		annotationToolModel_->setCommandList(toUtf8StdString(entireRecipe), false);

		QString composingRecipe = entireRecipe;
		if (ui->plainTextEditAudioSegmentsComposer->textCursor().hasSelection())
			composingRecipe = ui->plainTextEditAudioSegmentsComposer->textCursor().selection().toPlainText();
		
		annotationToolModel_->playComposingRecipeRequest(toUtf8StdString(composingRecipe));
	}

	void AnnotationToolMainWindow::audioTranscriptionToolModel_addedAudioTranscription(const boost::filesystem::path& annotFilePath)
	{
		std::shared_ptr<SpeechTranscriptionViewModel> transcriberModel = annotationToolModel_->audioTranscriptionModelByAnnotFilePathAbs(annotFilePath);
		PG_DbgAssert2(transcriberModel != nullptr, "Can't find created model");

		auto wdg = std::make_unique<SpeechTranscriptionWidget>();
		wdg->setTranscriberViewModel(transcriberModel);

		ui->tabWidgetSpeechTranscriptionTabs->addTab(wdg.release(), transcriberModel->modelShortName());
	}

	void AnnotationToolMainWindow::audioTranscriptionToolModel_activeAudioTranscriptionChanged(int ind)
	{
		ui->tabWidgetSpeechTranscriptionTabs->setCurrentIndex(ind);
	}

	void AnnotationToolMainWindow::audioTranscriptionToolModel_audioTranscriptionRemoved(int ind)
	{
		ui->tabWidgetSpeechTranscriptionTabs->removeTab(ind);
	}

	void AnnotationToolMainWindow::audioTranscriptionToolModel_audioTranscriptionListCleared()
	{
		ui->tabWidgetSpeechTranscriptionTabs->clear();
	}

	QString AnnotationToolMainWindow::fileWorkspaceModel_newAnnotDirQuery()
	{
		QString dirQ = QFileDialog::getExistingDirectory(this, "Select speech annotation directory", QString(), QFileDialog::ShowDirsOnly);
		return dirQ;
	}

	void AnnotationToolMainWindow::annotationToolModel_commandsListChanged()
	{
		auto commandsQ = utf8ToQString(annotationToolModel_->commandList());
		ui->plainTextEditAudioSegmentsComposer->setPlainText(commandsQ);
	}

	void AnnotationToolMainWindow::nextNotification(const QString& message)
	{
		ui->plainTextEditLogger->moveCursor(QTextCursor::End);
		ui->plainTextEditLogger->insertPlainText(message);
		ui->plainTextEditLogger->insertPlainText("\n");
	}

	void AnnotationToolMainWindow::keyPressEvent(QKeyEvent* ke)
	{
		if (ke->key() == Qt::Key_F3)
			annotationToolModel_->openAnnotDirRequest();

		else if (ke->key() == Qt::Key_P && ke->modifiers().testFlag(Qt::ControlModifier))
		{
			if (annotationToolModel_->isSpeechProjectOpened())
			{
				PhoneticDictionaryDialog phoneticDictDlg;
				phoneticDictDlg.setPhoneticViewModel(annotationToolModel_->phoneticDictModel());
				if (phoneticDictDlg.exec() == QDialog::Accepted) {}

				// update text line of each transcription view
				annotationToolModel_->onPronIdPhoneticSpecChanged();
			}
		}
		else if (ke->key() == Qt::Key_B && ke->modifiers().testFlag(Qt::ControlModifier))
		{
			annotationToolModel_->validateAllSpeechAnnotationRequest();
		}
		else if (ke->key() == Qt::Key_G && ke->modifiers().testFlag(Qt::ControlModifier))
			annotationToolModel_->navigateToMarkerRequest();
		else if (ke->key() == Qt::Key_F11)
			pushButtonSegmentComposerPlay_Clicked();

		else if (ke->key() == Qt::Key_S && ke->modifiers().testFlag(Qt::ControlModifier))
			annotationToolModel_->saveRequest();
		else
			QWidget::keyPressEvent(ke);
	}

	void AnnotationToolMainWindow::closeEvent(QCloseEvent*)
	{
		annotationToolModel_->onClose();
	}
}