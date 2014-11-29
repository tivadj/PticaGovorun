#include "transcribermainwindow.h"
#include <QApplication>
#include <QDebug>
#include <portaudio.h>

int main(int argc, char *argv[])
{
	PaError err = Pa_Initialize();
	if (err != paNoError)
	{
		qDebug() << "Pa_Initialize error: " << Pa_GetErrorText(err);
		return err;
	}
	qDebug() << "Pa_Initialize: success";
	
	QApplication a(argc, argv);
    TranscriberMainWindow w;
    w.show();

	int appExecOp = a.exec();

	err = Pa_Terminate();
	if (err != paNoError)
	{
		qDebug() << "Pa_Terminate error: " << Pa_GetErrorText(err);
	}
	else
		qDebug() << "Pa_Terminate: success";

	return appExecOp;
}
