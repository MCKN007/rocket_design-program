#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 设置应用程序信息
    QApplication::setApplicationName("火箭液体发动机设计程序");
    QApplication::setOrganizationName("航天设计院");
    QApplication::setApplicationVersion("1.0.0");

    // 设置样式
    QApplication::setStyle("Fusion");

    MainWindow w;
    w.show();

    return a.exec();
}
