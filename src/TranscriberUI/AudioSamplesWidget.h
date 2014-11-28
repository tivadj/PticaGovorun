#ifndef AUDIOSAMPLESWIDGET_H
#define AUDIOSAMPLESWIDGET_H

#include <memory>
#include <QWidget>
#include <QMouseEvent>
#include "TranscriberViewModel.h"

class AudioSamplesWidget : public QWidget
{
    Q_OBJECT
public:
    explicit AudioSamplesWidget(QWidget *parent = 0);

	void setModel(std::shared_ptr<TranscriberViewModel> transcriberModel);


public:
	signals:

public slots:

protected:
	void paintEvent(QPaintEvent*) override;
	void mousePressEvent(QMouseEvent*) override;
	void keyPressEvent(QKeyEvent*) override;
private:
	std::shared_ptr<TranscriberViewModel> transcriberModel_;
};

#endif // AUDIOSAMPLESWIDGET_H
