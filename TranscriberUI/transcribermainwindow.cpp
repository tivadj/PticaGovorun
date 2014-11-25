#include "transcribermainwindow.h"
#include "ui_transcribermainwindow.h"

TranscriberMainWindow::TranscriberMainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::TranscriberMainWindow)
{
    ui->setupUi(this);
}

TranscriberMainWindow::~TranscriberMainWindow()
{
    delete ui;
}
