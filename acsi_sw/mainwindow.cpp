#include <QDebug>

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "global.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    core = new CCoreThread();
    core->start();
}

MainWindow::~MainWindow()
{
    DWORD start = GetTickCount();
    while(GetTickCount() - start < 1000) {
        if(core->isRunning()) {
            core->stopRunning();
        }

        if(!core->isRunning()) {
            break;
        }
    }

    delete ui;
}

void MainWindow::on_pushButton_clicked()
{
    core->sendHalfWord();
}
