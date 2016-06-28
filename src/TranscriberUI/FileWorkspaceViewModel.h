#pragma once
#include <QDebug>
#include <QTreeWidget>
#include <QFileInfo>
#include "SpeechAnnotation.h"

namespace PticaGovorun
{
	// The element in the tree.
	struct FileWorkspaceItem
	{
		std::wstring FilePathAbs;
	};

	class FileWorkspaceViewModel : public QObject
	{
		Q_OBJECT
	public:
	signals :
		// Occurs when audio samples where successfully loaded from a file.
		void openAnnotFile(const std::wstring& annotFileAbsPath);

		// Occurs when path to speech annotation directory is changed.
		void annotDirChanged(const std::wstring& oldWorkingDir);

	public:
		FileWorkspaceViewModel();

		void setAnnotDir(const std::wstring& dir);
		std::wstring annotDir() const;

		void populateItems(QList<QTreeWidgetItem*>& items) const;

	private:
		void populateSubItemsWithoutItemItselfRec(const AnnotSpeechDirNode& annotStructure, QTreeWidgetItem* parent) const;
	private:
		std::wstring annotDir_;
	};

	
}