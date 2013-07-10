#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    core.start();
}

MainWindow::~MainWindow()
{
    DWORD start = GetTickCount();
    while(GetTickCount() - start < 1000) {
        if(core.isRunning()) {
            core.stopRunning();
        }

        if(!core.isRunning()) {
            break;
        }
    }

    delete ui;
}
