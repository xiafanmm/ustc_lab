#include <QApplication>
#include "MainWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv); // Qt 程序必须有的对象

    MainWindow window;
    window.show(); // 显示窗口

    return app.exec(); // 进入事件循环
}