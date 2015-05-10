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
		void openAudioFile(const std::wstring& filePath);
	public:
		FileWorkspaceViewModel();

		void setWorkingDirectory(const std::wstring& dir);

		void populateItems(QList<QTreeWidgetItem*>& items) const;

	private:
		void populateSubItemsWithoutItemItselfRec(const AnnotSpeechDirNode& annotStructure, QTreeWidgetItem* parent) const;
	private:
		std::wstring curDir_;
	};
}