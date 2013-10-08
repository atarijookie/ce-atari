#include <QDebug>

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "global.h"

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

void MainWindow::on_chbDrive01_stateChanged(int arg1)
{
    bool drive01 = ui->chbDrive01->isChecked();

    if(drive01) {
        qDebug() << "Drive ID: 1";
        core.setNextCmd(CMD_SET_DRIVE_ID_1);
    } else {
        qDebug() << "Drive ID: 0";
        core.setNextCmd(CMD_SET_DRIVE_ID_0);
    }
}

void MainWindow::on_chbWriteProtect_stateChanged(int arg1)
{
    bool wp = ui->chbWriteProtect->isChecked();

    if(wp) {
        qDebug() << "Drive is Read Only";
        core.setNextCmd(CMD_WRITE_PROTECT_ON);
    } else {
        qDebug() << "Drive is Read-Write";
        core.setNextCmd(CMD_WRITE_PROTECT_OFF);
    }
}

void MainWindow::on_chbDiskChg_stateChanged(int )
{
    bool dc = ui->chbDiskChg->isChecked();

    if(dc) {
        qDebug() << "Disk changed";
        core.setNextCmd(CMD_DISK_CHANGE_ON);
    } else {
        qDebug() << "Disk not changed";
        core.setNextCmd(CMD_DISK_CHANGE_OFF);
    }
}

void MainWindow::on_pushButton_clicked()
{
    core.sendHalfWord();
}
