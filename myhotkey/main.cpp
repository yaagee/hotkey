#include "myhotkey.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    myhotkey w;
    w.hide();
    return a.exec();
}
