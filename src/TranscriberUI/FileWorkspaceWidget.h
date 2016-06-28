#ifndef FILEWORKSPACEWIDGET_H
#define FILEWORKSPACEWIDGET_H

#include <memory>
#include <QWidget>
#include "FileWorkspaceViewModel.h"

namespace Ui {
class FileWorkspaceWidget;
}

namespace PticaGovorun
{
	class FileWorkspaceWidget : public QWidget
	{
		Q_OBJECT
	private:
		std::shared_ptr<FileWorkspaceViewModel> model_;
	public:
		explicit FileWorkspaceWidget(QWidget *parent = 0);
		~FileWorkspaceWidget();

		void setFileWorkspaceViewModel(std::shared_ptr<FileWorkspaceViewModel> model);
	private slots:
		void treeWidgetFileItems_itemDoubleClicked(QTreeWidgetItem * item, int column);
		void fileWorkspaceModel_workingDirChanged(const std::wstring& oldWorkingDir);
	private:
		void updateUI();
	private:
		Ui::FileWorkspaceWidget *ui;
	};
}

#endif // FILEWORKSPACEWIDGET_H
