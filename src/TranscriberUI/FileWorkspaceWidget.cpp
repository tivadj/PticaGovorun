#include "FileWorkspaceWidget.h"
#include "ui_FileWorkspaceWidget.h"
#include <QDebug>
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

		QList<QTreeWidgetItem*> rootItems;
		model->populateItems(rootItems);

		ui->treeWidgetFileItems->clear();
		ui->treeWidgetFileItems->addTopLevelItems(rootItems);
		ui->treeWidgetFileItems->expandAll();
	}

	void FileWorkspaceWidget::treeWidgetFileItems_itemDoubleClicked(QTreeWidgetItem* item, int column)
	{
		QVariant wavPathVar = item->data(0, Qt::UserRole);
		if (wavPathVar.isNull())
			return;
		QString wavFilePath = wavPathVar.toString();
		model_->openAudioFile(wavFilePath.toStdWString());
	}
}