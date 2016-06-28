#include "FileWorkspaceWidget.h"
#include "ui_FileWorkspaceWidget.h"
#include <QDebug>
#include <QFileDialog>
namespace PticaGovorun
{
	FileWorkspaceWidget::FileWorkspaceWidget(QWidget *parent) :
		QWidget(parent),
		ui(new Ui::FileWorkspaceWidget)
	{
		ui->setupUi(this);

		QObject::connect(ui->treeWidgetFileItems, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), this, SLOT(treeWidgetFileItems_itemDoubleClicked(QTreeWidgetItem*, int)));
	}

	FileWorkspaceWidget::~FileWorkspaceWidget()
	{
		delete ui;
	}

	void FileWorkspaceWidget::setFileWorkspaceViewModel(std::shared_ptr<FileWorkspaceViewModel> model)
	{
		model_ = model;

		QObject::connect(model_.get(), SIGNAL(annotDirChanged(const std::wstring&)), this, SLOT(fileWorkspaceModel_annotDirChanged(const std::wstring&)));

		updateUI();
	}

	void FileWorkspaceWidget::treeWidgetFileItems_itemDoubleClicked(QTreeWidgetItem* item, int column)
	{
		QVariant strVar = item->data(0, Qt::UserRole);
		if (strVar.isNull())
			return;
		QString annotFilePath = strVar.toString();
		model_->openAnnotFile(annotFilePath.toStdWString());
	}

	void FileWorkspaceWidget::fileWorkspaceModel_annotDirChanged(const std::wstring& oldWorkingDir)
	{
		updateUI();
	}

	void FileWorkspaceWidget::updateUI()
	{
		QList<QTreeWidgetItem*> rootItems;
		model_->populateItems(rootItems);

		ui->treeWidgetFileItems->clear();
		ui->treeWidgetFileItems->addTopLevelItems(rootItems);
		ui->treeWidgetFileItems->expandAll();
	}
}