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

		QObject::connect(model_.get(), SIGNAL(workingDirChanged(const std::wstring&)), this, SLOT(fileWorkspaceModel_workingDirChanged(const std::wstring&)));

		updateUI();
	}

	void FileWorkspaceWidget::treeWidgetFileItems_itemDoubleClicked(QTreeWidgetItem* item, int column)
	{
		QVariant wavPathVar = item->data(0, Qt::UserRole);
		if (wavPathVar.isNull())
			return;
		QString wavFilePath = wavPathVar.toString();
		model_->openAudioFile(wavFilePath.toStdWString());
	}

	void FileWorkspaceWidget::fileWorkspaceModel_workingDirChanged(const std::wstring& oldWorkingDir)
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