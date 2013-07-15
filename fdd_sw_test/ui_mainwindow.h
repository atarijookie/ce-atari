/********************************************************************************
** Form generated from reading UI file 'mainwindow.ui'
**
** Created: Mon 15. Jul 05:45:06 2013
**      by: Qt User Interface Compiler version 4.7.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtGui/QAction>
#include <QtGui/QApplication>
#include <QtGui/QButtonGroup>
#include <QtGui/QCheckBox>
#include <QtGui/QHeaderView>
#include <QtGui/QLabel>
#include <QtGui/QMainWindow>
#include <QtGui/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QWidget *centralWidget;
    QLabel *label;
    QCheckBox *chbWriteProtect;
    QCheckBox *chbDrive01;
    QCheckBox *chbDiskChg;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName(QString::fromUtf8("MainWindow"));
        MainWindow->resize(400, 300);
        centralWidget = new QWidget(MainWindow);
        centralWidget->setObjectName(QString::fromUtf8("centralWidget"));
        label = new QLabel(centralWidget);
        label->setObjectName(QString::fromUtf8("label"));
        label->setGeometry(QRect(20, 20, 151, 16));
        chbWriteProtect = new QCheckBox(centralWidget);
        chbWriteProtect->setObjectName(QString::fromUtf8("chbWriteProtect"));
        chbWriteProtect->setGeometry(QRect(20, 90, 171, 18));
        chbDrive01 = new QCheckBox(centralWidget);
        chbDrive01->setObjectName(QString::fromUtf8("chbDrive01"));
        chbDrive01->setGeometry(QRect(20, 110, 161, 18));
        chbDiskChg = new QCheckBox(centralWidget);
        chbDiskChg->setObjectName(QString::fromUtf8("chbDiskChg"));
        chbDiskChg->setGeometry(QRect(20, 130, 171, 18));
        MainWindow->setCentralWidget(centralWidget);

        retranslateUi(MainWindow);

        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QApplication::translate("MainWindow", "MainWindow", 0, QApplication::UnicodeUTF8));
        label->setText(QApplication::translate("MainWindow", "Floppy test app", 0, QApplication::UnicodeUTF8));
        chbWriteProtect->setText(QApplication::translate("MainWindow", "write protection", 0, QApplication::UnicodeUTF8));
        chbDrive01->setText(QApplication::translate("MainWindow", "drive 0 / 1 (1 when checked)", 0, QApplication::UnicodeUTF8));
        chbDiskChg->setText(QApplication::translate("MainWindow", "disk change (checked means ON)", 0, QApplication::UnicodeUTF8));
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
