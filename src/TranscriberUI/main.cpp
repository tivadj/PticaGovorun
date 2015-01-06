#include <QApplication>
#include <QDebug>
#include <QDir>
#include <portaudio.h>
#include "transcribermainwindow.h"

#if HAS_MATLAB
#include "PticaGovorunInteropMatlab.h"
#endif

int main(int argc, char *argv[])
{
	auto cd = QDir::currentPath();
	qDebug() << "CD=" << cd;

	//
	qDebug() << "Initialize PortAudio runtime";
	PaError err = Pa_Initialize();
	if (err != paNoError)
	{
		qDebug() << "Pa_Initialize error: " << Pa_GetErrorText(err);
		return err;
	}
	qDebug() << "Pa_Initialize: success";

#if HAS_MATLAB
	qDebug() << "Init Matlab runtime";

	auto errHandler = [](const char* s) { qDebug() << s; return 1; };
	auto printHandler = [](const char* s) { qDebug() << s; return 1; };
	bool initMatlab = PticaGovorunInteropMatlabInitializeWithHandlers(errHandler, printHandler);
	if (!initMatlab)
	{
		qDebug() << "PWMatlabProxy initialization failed";
		return -1;
	}
#endif
	
	QApplication a(argc, argv);
    TranscriberMainWindow w;
    w.show();

	int appExecOp = a.exec();

	//
#if HAS_MATLAB
	PticaGovorunInteropMatlabTerminate();
#endif

	//
	err = Pa_Terminate();
	if (err != paNoError)
	{
		qDebug() << "Pa_Terminate error: " << Pa_GetErrorText(err);
	}
	else
		qDebug() << "Pa_Terminate: success";

	return appExecOp;
}
