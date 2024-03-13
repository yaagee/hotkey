#pragma once

#include <QSystemTrayIcon>
#include <QtWidgets/QMainWindow>
#include <QCloseEvent>

#include "ui_myhotkey.h"

class myhotkey : public QMainWindow
{
    Q_OBJECT

public:
    myhotkey(QWidget *parent = nullptr);
    ~myhotkey();

private:
    void closeEvent(QCloseEvent * event);

private slots:
    void on_activatedSysTrayIcon(QSystemTrayIcon::ActivationReason reason);

private:
    Ui::myhotkeyClass ui;

private:
    QSystemTrayIcon *SysIcon;
    QAction *min; //最小化
    QAction *max; //最大化
    QAction *restor; //恢复
    QAction *quit; //退出
    QMenu *menu;


};
