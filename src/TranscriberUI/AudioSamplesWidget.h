#ifndef AUDIOSAMPLESWIDGET_H
#define AUDIOSAMPLESWIDGET_H

#include <memory>
#include <QWidget>
#include "TranscriberViewModel.h"

class AudioSamplesWidget : public QWidget
{
    Q_OBJECT
public:
    explicit AudioSamplesWidget(QWidget *parent = 0);

	void setModel(std::shared_ptr<TranscriberViewModel> transcriberModel);

signals:

public slots:

protected:

	void paintEvent(QPaintEvent*) override;
private:
	std::shared_ptr<TranscriberViewModel> transcriberModel_;
};

#endif // AUDIOSAMPLESWIDGET_H
