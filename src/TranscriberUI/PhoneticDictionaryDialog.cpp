#include <QString>
#include "PhoneticDictionaryDialog.h"
#include "ui_PhoneticDictionaryDialog.h"

namespace PticaGovorun
{
	PhoneticDictionaryDialog::PhoneticDictionaryDialog(QWidget* parent)
		: QDialog(parent),
		ui(new Ui::PhoneticDictDialog)
	{
		ui->setupUi(this);

		//
		QObject::connect(ui->lineEditCurrentWord, SIGNAL(textEdited(const QString&)), this, SLOT(lineEditCurrentWord_textEdited(const QString&)));
		QObject::connect(ui->listWidgetWords, SIGNAL(itemSelectionChanged()), this, SLOT(listWidgetWords_itemSelectionChanged()));
		QObject::connect(ui->listWidgetWords, SIGNAL(itemDoubleClicked(QListWidgetItem *)), this, SLOT(listWidgetWords_itemDoubleClicked(QListWidgetItem*)));
		QObject::connect(ui->radioButtonBrowsePersian, SIGNAL(toggled(bool)), this, SLOT(groupBoxBrowseDict_toggled(bool)));
		QObject::connect(ui->radioButtonBrowseBroken, SIGNAL(toggled(bool)), this, SLOT(groupBoxBrowseDict_toggled(bool)));
		QObject::connect(ui->radioButtonBrowseShrekky, SIGNAL(toggled(bool)), this, SLOT(groupBoxBrowseDict_toggled(bool)));
		QObject::connect(ui->lineEditNewWord, SIGNAL(textChanged(const QString&)), this, SLOT(lineEditNewWord_textChanged(const QString&)));
		QObject::connect(ui->pushButtonAddNewWord, SIGNAL(clicked()), this, SLOT(pushButtonAddNewWord_clicked()));
	}

	PhoneticDictionaryDialog::~PhoneticDictionaryDialog()
	{
	}

	void PhoneticDictionaryDialog::setPhoneticViewModel(std::shared_ptr<PhoneticDictionaryViewModel> phoneticDictViewModel)
	{
		phoneticDictViewModel_ = phoneticDictViewModel;
		phoneticDictViewModel_->ensureDictionaryLoaded();
		attachDetachPhoneticViewModel(true);
	}

	void PhoneticDictionaryDialog::attachDetachPhoneticViewModel(bool attach)
	{
		if (attach)
		{
			   QObject::connect(phoneticDictViewModel_.get(), SIGNAL(matchedWordsChanged()), this, SLOT(phoneticDictViewModel_matchedWordsChanged()));
			   QObject::connect(phoneticDictViewModel_.get(), SIGNAL(phoneticTrnascriptChanged()), this, SLOT(phoneticDictViewModel_phoneticTrnascriptChanged()));
		}
		else
		{
			QObject::disconnect(phoneticDictViewModel_.get(), SIGNAL(matchedWordsChanged()), this, SLOT(phoneticDictViewModel_matchedWordsChanged()));
			QObject::connect(phoneticDictViewModel_.get(), SIGNAL(phoneticTrnascriptChanged()), this, SLOT(phoneticDictViewModel_phoneticTrnascriptChanged()));
		}
	}

	void PhoneticDictionaryDialog::closeEvent(QCloseEvent*)
	{
		attachDetachPhoneticViewModel(false);
	}

	void PhoneticDictionaryDialog::showEvent(QShowEvent*)
	{
		ui->lineEditCurrentWord->setFocus();
	}

	void PhoneticDictionaryDialog::lineEditCurrentWord_textEdited(const QString& text)
	{
		findMatchedWords();
	}
	void PhoneticDictionaryDialog::groupBoxBrowseDict_toggled(bool checked)
	{
		findMatchedWords();
	}

	QString PhoneticDictionaryDialog::currentBrowsedDictId()
	{
		QString browseDictStr;
		if (ui->radioButtonBrowsePersian->isChecked())
			browseDictStr = "persian";
		else if (ui->radioButtonBrowseBroken->isChecked())
			browseDictStr = "broken";
		else if (ui->radioButtonBrowseShrekky->isChecked())
			browseDictStr = "shrekky";
		return browseDictStr;
	}

	void PhoneticDictionaryDialog::findMatchedWords()
	{
		QString browseDictStr = currentBrowsedDictId();
		QString text = ui->lineEditCurrentWord->text();

		phoneticDictViewModel_->updateSuggesedWordsList(browseDictStr, text);
	}

	void PhoneticDictionaryDialog::phoneticDictViewModel_matchedWordsChanged()
	{
		ui->listWidgetWords->clear();
		
		const QStringList& words = phoneticDictViewModel_->matchedWords();
		ui->listWidgetWords->addItems(words);
	}

	void PhoneticDictionaryDialog::listWidgetWords_itemDoubleClicked(QListWidgetItem* item)
	{
		QListWidgetItem* currentItem = ui->listWidgetWords->currentItem();
		QString wordQ = currentItem->text();

		QString browseDictStr = currentBrowsedDictId();
		phoneticDictViewModel_->setEditedWordSourceDictionary(browseDictStr);
		
			// set word skeleton for edit
		ui->lineEditNewWord->setText(wordQ);
		
		QString wordProns = ui->plainTextEditPronunciations->toPlainText();
		ui->plainTextNewWordPronunciations->setPlainText(wordProns);
	}

	void PhoneticDictionaryDialog::listWidgetWords_itemSelectionChanged()
	{
		QString dictId = currentBrowsedDictId();
		QListWidgetItem* currentItem = ui->listWidgetWords->currentItem();
		std::wstring word = currentItem->text().toStdWString();

		phoneticDictViewModel_->showWordPhoneticTranscription(dictId, word);
	}

	void PhoneticDictionaryDialog::lineEditNewWord_textChanged(const QString & text)
	{
		QString curWord = text;
		QString pronStr = phoneticDictViewModel_->getWordAutoTranscription(curWord);
		ui->lineEditWordTranscriptionAuto->setText(pronStr);
	}

	void PhoneticDictionaryDialog::phoneticDictViewModel_phoneticTrnascriptChanged()
	{
		ui->plainTextEditPronunciations->setPlainText(phoneticDictViewModel_->wordPhoneticTranscript());
	}

	void PhoneticDictionaryDialog::pushButtonAddNewWord_clicked()
	{
		QString word = ui->lineEditNewWord->text();
		QString pronLinesAsStr = ui->plainTextNewWordPronunciations->toPlainText();

		QString dictId = ui->radioButtonNewWordPersian->isChecked() ? ui->radioButtonNewWordPersian->text() : ui->radioButtonNewWordBroken->text();
		phoneticDictViewModel_->updateWordPronunciation(dictId, word, pronLinesAsStr);
	}
}