#include <QApplication>
#include <QDebug>
#include "mainwindow.h"

/**
 * @brief 程序入口点
 *
 * 初始化Qt应用并显示主窗口。
 */
int main(int argc, char *argv[]) {
    qDebug() << "1: Creating QApplication";
    QApplication app(argc, argv);

    qDebug() << "2: Setting application info";
    // 设置应用信息
    app.setApplicationName("NPU Virtual Campus Map");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("NPU");

    qDebug() << "3: Creating MainWindow";
    // 创建并显示主窗口
    MainWindow mainWindow;

    qDebug() << "4: Showing MainWindow";
    mainWindow.show();

    qDebug() << "5: Starting event loop";
    return app.exec();
}
