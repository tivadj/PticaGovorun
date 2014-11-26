#ifndef TRANSCRIBERMAINWINDOW_H
#define TRANSCRIBERMAINWINDOW_H

#include <memory>
#include <QMainWindow>
#include "TranscriberViewModel.h"

namespace Ui {
class TranscriberMainWindow;
}

class TranscriberMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit TranscriberMainWindow(QWidget *parent = 0);
    ~TranscriberMainWindow();

    void updateUI();

private slots:
	void pushButtonLoad_Clicked();
	void transcriberModel_nextNotification(const QString& message);
	void transcriberModel_audioSamplesChanged();
	void lineEditFileName_editingFinished();

private:
    Ui::TranscriberMainWindow *ui;
    std::shared_ptr<TranscriberViewModel> transcriberModel_;

};

#endif // TRANSCRIBERMAINWINDOW_H
