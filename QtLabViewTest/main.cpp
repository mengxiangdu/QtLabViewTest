#include "QtLabViewTest.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QtLabViewTest w;
    w.show();
    return a.exec();
}
