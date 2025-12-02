/*
 * Description: 客户端程序入口
 * Author: 夏凡
 * Create: 2025-12-02
 */

#include <QApplication>
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    MainWindow window;
    window.show();
    return app.exec();
}