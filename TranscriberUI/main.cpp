#include "transcribermainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    TranscriberMainWindow w;
    w.show();

    return a.exec();
}
