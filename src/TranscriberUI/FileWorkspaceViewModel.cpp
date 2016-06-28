#include "FileWorkspaceViewModel.h"
#include <QDirIterator>
#include <QString>

namespace PticaGovorun
{
	FileWorkspaceViewModel::FileWorkspaceViewModel()
	{
	}

	void FileWorkspaceViewModel::setWorkingDirectory(const std::wstring& dir)
	{
		if (curDir_ != dir)
		{
			auto oldDir = curDir_;
			curDir_ = dir;
			emit workingDirChanged(oldDir);
		}
	}

	void FileWorkspaceViewModel::populateItems(QList<QTreeWidgetItem*>& items) const
	{
		QString curDirQ = QString::fromStdWString(curDir_);

		AnnotSpeechDirNode annotStructure;
		populateAnnotationFileStructure(curDirQ, annotStructure);

		QTreeWidgetItem itemsStore;
		populateSubItemsWithoutItemItselfRec(annotStructure, &itemsStore);

		items = itemsStore.takeChildren();
	}

	void FileWorkspaceViewModel::populateSubItemsWithoutItemItselfRec(const AnnotSpeechDirNode& annotStructure, QTreeWidgetItem* parent) const
	{
		for (const AnnotSpeechDirNode& subdir : annotStructure.SubDirs)
		{
			QTreeWidgetItem* item = new QTreeWidgetItem();
			item->setText(0, subdir.Name);
			parent->addChild(item);
			populateSubItemsWithoutItemItselfRec(subdir, item);
		}
		for (const AnnotSpeechFileNode& fileItem : annotStructure.AnnotFiles)
		{
			QTreeWidgetItem* item = new QTreeWidgetItem();
			item->setText(0, fileItem.FileNameNoExt);
			item->setData(0, Qt::UserRole, QVariant::fromValue(fileItem.AudioPath));
			parent->addChild(item);
		}
	}
}