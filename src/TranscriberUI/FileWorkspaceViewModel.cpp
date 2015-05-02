#include "FileWorkspaceViewModel.h"
#include <QDirIterator>

namespace PticaGovorun
{
	FileWorkspaceViewModel::FileWorkspaceViewModel()
	{
	}

	void FileWorkspaceViewModel::setWorkingDirectory(const std::wstring& dir)
	{
		curDir_ = dir;
	}

	void FileWorkspaceViewModel::populateItems(QList<QTreeWidgetItem*>& items) const
	{
		if (curDir_.empty())
			return;

		QString curDirQ = QString::fromStdWString(curDir_);
		QDir dir(curDirQ);
		if (!dir.cd("SpeechAudio"))
			return;

		QTreeWidgetItem itemsStore;
		QFileInfo dirInfo(dir.absolutePath());
		populateSubItemsWithoutItemItselfRec(dirInfo, &itemsStore);

		items = itemsStore.takeChildren();
	}

	void FileWorkspaceViewModel::populateItemsRec(const QFileInfo& fileInfo, QTreeWidgetItem* parent) const
	{
		QString fileName = fileInfo.fileName();
		if (fileInfo.isFile() && !fileName.endsWith(".wav", Qt::CaseInsensitive))
			return;

		QString filePath = fileInfo.absoluteFilePath();

		QTreeWidgetItem* item = new QTreeWidgetItem();
		item->setText(0, fileName);
		item->setData(0, Qt::UserRole, QVariant::fromValue(filePath));
		parent->addChild(item);

		if (fileInfo.isDir())
		{
			populateSubItemsWithoutItemItselfRec(fileInfo, item);
		}
	}

	void FileWorkspaceViewModel::populateSubItemsWithoutItemItselfRec(const QFileInfo& fileInfoExcl, QTreeWidgetItem* parent) const
	{
		QDirIterator it(fileInfoExcl.absoluteFilePath(),
			QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
		while (it.hasNext())
		{
			it.next();
			QFileInfo childFileInfo = it.fileInfo();
			populateItemsRec(childFileInfo, parent);
		}
	}
}