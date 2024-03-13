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
    QAction *min; //��С��
    QAction *max; //���
    QAction *restor; //�ָ�
    QAction *quit; //�˳�
    QMenu *menu;


};
