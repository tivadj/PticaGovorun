#ifndef AUDIOMARKUPNAVIGATORDIALOG_H
#define AUDIOMARKUPNAVIGATORDIALOG_H

#include <QDialog>

namespace Ui {
class AudioMarkupNavigatorDialog;
}

class AudioMarkupNavigatorDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AudioMarkupNavigatorDialog(QWidget *parent = 0);
    ~AudioMarkupNavigatorDialog();
	int markerId() const;

protected:
	void showEvent(QShowEvent*) override;

private:
    Ui::AudioMarkupNavigatorDialog *ui;
};

#endif // AUDIOMARKUPNAVIGATORDIALOG_H
