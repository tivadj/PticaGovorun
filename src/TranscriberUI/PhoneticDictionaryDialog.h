#pragma once
#include <memory>
#include <QListWidgetItem>
#include <QDialog>
#include "PhoneticDictionaryViewModel.h"

namespace Ui {
	class PhoneticDictDialog;
}

namespace PticaGovorun
{
	class PhoneticDictionaryDialog : public QDialog
	{
		Q_OBJECT

	public:
		explicit PhoneticDictionaryDialog(QWidget *parent = 0);
		~PhoneticDictionaryDialog();

	public:
		void setPhoneticViewModel(std::shared_ptr<PhoneticDictionaryViewModel> phoneticDictViewModel);
		void attachDetachPhoneticViewModel(bool attach);
	protected:
		void closeEvent(QCloseEvent*) override;
		void showEvent(QShowEvent*) override;
	private slots:
		void lineEditCurrentWord_textEdited(const QString& text);
		void phoneticDictViewModel_matchedWordsChanged();
		void listWidgetWords_itemDoubleClicked(QListWidgetItem* item);
		void phoneticDictViewModel_phoneticTrnascriptChanged();
		void groupBoxBrowseDict_toggled(bool checked);
		void listWidgetWords_itemSelectionChanged();
		void lineEditNewWord_textChanged(const QString & text);
		void pushButtonAddNewWord_clicked();

		void findMatchedWords();
		QString currentBrowsedDictId();
	private:
		Ui::PhoneticDictDialog *ui;
		std::shared_ptr<PhoneticDictionaryViewModel> phoneticDictViewModel_;
	};
}
