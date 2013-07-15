#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "ccorethread.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT
    
public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
    
private slots:
    void on_chbDiskChg_stateChanged(int );
    void on_chbDrive01_stateChanged(int arg1);

    void on_chbWriteProtect_stateChanged(int arg1);

private:
    Ui::MainWindow *ui;

    CCoreThread core;
};

#endif // MAINWINDOW_H
