#include "FileWorkspaceViewModel.h"
#include <QDirIterator>
#include <QString>

namespace PticaGovorun
{
	FileWorkspaceViewModel::FileWorkspaceViewModel()
	{
	}

	void FileWorkspaceViewModel::setAnnotDir(const std::wstring& dir)
	{
		if (annotDir_ != dir)
		{
			auto oldDir = annotDir_;
			annotDir_ = dir;
			emit annotDirChanged(oldDir);
		}
	}

	std::wstring FileWorkspaceViewModel::annotDir() const
	{
		return annotDir_;
	}

	void FileWorkspaceViewModel::populateItems(QList<QTreeWidgetItem*>& items) const
	{
		if (annotDir_.empty())
			return;
		QString curDirQ = QString::fromStdWString(annotDir_);

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
			item->setData(0, Qt::UserRole, QVariant::fromValue(fileItem.SpeechAnnotationAbsPath));
			parent->addChild(item);
		}
	}
}