#include "myhotkey.h"
#include <iostream>
#include <conio.h>
#include <stdio.h>
#include <QProcess>
#include <Windows.h>

#include "hotkeyhandler.h"
#include "zetjsoncpp.h"
using namespace zetjsoncpp;

#pragma execution_character_set("utf-8")


void handA(void*)
{
    printf("this is A\n");
}

void handZ(void*)
{
    printf("this is Z\n");
}

void hand1(void* s)
{
    //OutputDebugStringA((char*)s);
    WinExec((char*)s, SW_SHOW);
}

int findProcess(const QString& name)
{
    QProcess p;
    p.start("tasklist");
    p.waitForFinished();

    QString output = p.readAllStandardOutput();
    QStringList lines = output.split("\n");

    //OutputDebugStringA(output.toStdString().c_str());
    for (QString line : lines) {
        //OutputDebugStringA(line.toStdString().c_str());
        if (line.contains(name, Qt::CaseInsensitive)) {
            line.replace(QRegExp("[\\s]+"), " ");
            QStringList parts = line.split(" ");
            return parts[1].toInt();
        }
    }
    return 0;
}

typedef struct
{
    HWND hwndWindow; // ���ھ��
    DWORD dwProcessID; // ����ID
}EnumWindowsArg;
///< ö�ٴ��ڻص�����
BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
    EnumWindowsArg *pArg = (EnumWindowsArg *)lParam;
    DWORD dwProcessID = 0;
    // ͨ�����ھ��ȡ�ý���ID
    ::GetWindowThreadProcessId(hwnd, &dwProcessID);
    if (dwProcessID == pArg->dwProcessID) {
        pArg->hwndWindow = hwnd;
        // �ҵ��˷���FALSE
        return FALSE;
    }
    // û�ҵ��������ң�����TRUE
    return TRUE;
}

///< ͨ������ID��ȡ���ھ��
HWND GetWindowHwndByPID(DWORD dwProcessID)
{
    HWND hwndRet = NULL;
    EnumWindowsArg ewa;
    ewa.dwProcessID = dwProcessID;
    ewa.hwndWindow = NULL;
    EnumWindows(EnumWindowsProc, (LPARAM)&ewa);
    if (ewa.hwndWindow) {
        hwndRet = ewa.hwndWindow;
    }
    return hwndRet;
}

//���ؽ���
void hidehandler(void* process_name)
{
    OutputDebugStringA("hidehandler");
    OutputDebugStringA((char*)process_name);
    int pid = findProcess((char*)process_name);
    if (pid == 0) {
        OutputDebugStringA("cannot find the process");
        return;
    }

    HWND hTask = GetWindowHwndByPID(pid);

    //hTask = ::FindWindow((char*)process_name, NULL);
    // ����
    bool bVisible = (::GetWindowLong(hTask, GWL_STYLE) & WS_VISIBLE) != 0;
    if (bVisible) {
        OutputDebugStringA("hide the process");
        ::ShowWindow(hTask, SW_HIDE);
        //SetWindowPos(hTask, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    } else {
        ::ShowWindow(hTask, SW_SHOW);
        OutputDebugStringA("show the process");
        //SetWindowPos(hTask, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    }
}

void killhandler(void* process_name)
{
    //int pid = findProcess((char*)process_name);
    //if (pid == 0) {
    //    return;
    //}

    //HWND hTask = GetWindowHwndByPID(pid);

    //hTask = ::FindWindow((char*)process_name, NULL);
    // ����
    //bool bVisible = (::GetWindowLong(hTask, GWL_STYLE) & WS_VISIBLE) != 0;
    //if (bVisible) {
    //    ::ShowWindow(hTask, SW_HIDE);
    //}

    QProcess killprocess;
    //QString cmd = QString("TASKKILL /PID %1 /F").arg(pid);
    QString cmd = QString("cmd.exe /c TASKKILL.exe /F /IM %1").arg((char*)process_name);
    OutputDebugStringA(cmd.toStdString().c_str());
    killprocess.start(cmd.toStdString().c_str());
    killprocess.waitForFinished();
}


typedef struct
{
    JsonVarMapString<ZJ_CONST_CHAR("hotkeytasks")> tasks;
    JsonVarMapString<ZJ_CONST_CHAR("hotkeyhidetasks")> hidetasks;
    JsonVarMapString<ZJ_CONST_CHAR("hotkeykilltasks")> killtasks;
} HotKeyTask;


CHotkeyHandler hk;

myhotkey::myhotkey(QWidget *parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);

    //https://converticon.com/ ico tools
    this->setWindowIcon(QIcon(":/myhotkey/logo.png"));

    menu = new QMenu(this);
    QIcon icon(":/myhotkey/logo.png");
    SysIcon = new QSystemTrayIcon(this);
    SysIcon->setIcon(icon);
    SysIcon->setToolTip("�ȼ�");
    min = new QAction("������С��", this);
    connect(min, &QAction::triggered, this, &myhotkey::hide);
    //   connect(min,SIGNAL(trigger()),this,&MainWindow::hide);
    max = new QAction("�������", this);
    connect(max, &QAction::triggered, this, &myhotkey::showMaximized);
    restor = new QAction("�ָ�ԭ��������", this);
    connect(restor, &QAction::triggered, this, &myhotkey::showNormal);
    quit = new QAction("�˳�", this);
    //    connect(quit,&QAction::triggered,this,&MainWindow::close);
    connect(quit, &QAction::triggered, qApp, &QApplication::quit);
    connect(SysIcon, &QSystemTrayIcon::activated, this, &myhotkey::on_activatedSysTrayIcon);

    menu->addAction(min);
    menu->addAction(max);
    menu->addAction(restor);
    menu->addSeparator(); //�ָ�
    menu->addAction(quit);
    SysIcon->setContextMenu(menu);
    SysIcon->show();

    close();

    OutputDebugStringA("enter myhotkey contructor");
    //hotkey
    int err, id;
    try {

        hk.RemoveHandler(id = 0);

        //std::string filename = R"(C:\Users\shake\Desktop\S\tc\hotkeys.json)";
        std::string filename = "hotkeys.json";
        auto json_object = zetjsoncpp::deserialize_file<zetjsoncpp::JsonVarObject<HotKeyTask>>(filename);

        // iterate of all interpolations and replace its data values...
        auto &tasks = json_object->tasks;
        for (auto it_map = tasks.begin(); it_map != tasks.end(); it_map++) {
            if (it_map->first.size() > 0) {
                char k = it_map->first[0];
                hk.InsertHandler(MOD_CONTROL | MOD_ALT, k, hand1, it_map->second, id);
                OutputDebugStringA(it_map->second.c_str());
                //err = hk.Start((LPVOID)(it_map->second.c_str()));
                //if (err != CHotkeyHandler::hkheOk) {
                //    printf("Error %d on Start()\n", err);
                //}
            }
        }

        auto &hidetasks = json_object->hidetasks;
        for (auto it_map = hidetasks.begin(); it_map != hidetasks.end(); it_map++) {
            if (it_map->first.size() > 0) {
                char k = it_map->first[0];
                hk.InsertHandler(MOD_CONTROL | MOD_ALT, k, hidehandler, it_map->second, id);
                //MOD_WIN
            }
        }

        auto &killtasks = json_object->killtasks;
        for (auto it_map = killtasks.begin(); it_map != killtasks.end(); it_map++) {
            if (it_map->first.size() > 0) {
                char k = it_map->first[0];
                hk.InsertHandler(MOD_CONTROL | MOD_ALT, k, killhandler, it_map->second, id);
                OutputDebugStringA(it_map->second.c_str());
            }
        }


        err = hk.Start(nullptr);
        if (err != CHotkeyHandler::hkheOk) {
            printf("Error %d on Start()\n", err);
        }

        //hk.InsertHandler(MOD_CONTROL | MOD_ALT, 'Z', hand1, id);
        //hk.InsertHandler(MOD_CONTROL | MOD_ALT, 'A', handA, id);
        //hk.InsertHandler(MOD_CONTROL | MOD_ALT, '1', hand1, id);

        //err = hk.Start((LPVOID)R"(E:\programfiles\Totalcommander10.52\TOTALCMD64.EXE)");
        //if (err != CHotkeyHandler::hkheOk)
        //{
        //    printf("Error %d on Start()\n", err);
        //    return err;
        //}

    }
    catch (std::exception& ex) {
        fprintf(stderr, "%s\n", ex.what());
    }

}

myhotkey::~myhotkey()
{
    hk.Stop();
}


void myhotkey::closeEvent(QCloseEvent * event)
{
    if (SysIcon->isVisible()) {
        this->hide();
        SysIcon->showMessage("�ȼ�", "�ȼ�����������");
        event->ignore();
    }
    else {
        event->accept();
    }

}

void myhotkey::on_activatedSysTrayIcon(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason) {

    case QSystemTrayIcon::Trigger:
        break;
    case QSystemTrayIcon::DoubleClick:
        this->show();
        break;
    default:
        break;

    }
}
