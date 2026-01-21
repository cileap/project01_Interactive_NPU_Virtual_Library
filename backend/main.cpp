#include <QGuiApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QDebug>
#include "server.h"

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);

    // 设置应用信息
    app.setApplicationName("NPU Map Backend Server");
    app.setApplicationVersion("1.0.0");

    // 命令行参数解析
    QCommandLineParser parser;
    parser.setApplicationDescription("NPU 虚拟校园地图后端服务器");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption portOption(QStringList() << "p" << "port",
                                  "服务器监听端口", "port", "8888");
    parser.addOption(portOption);

    parser.process(app);

    quint16 port = parser.value(portOption).toUShort();

    // 创建并启动服务器
    HttpServer server;
    if (!server.start(port)) {
        qCritical() << "Failed to start server";
        return 1;
    }

    qDebug() << "Server is running. Press Ctrl+C to stop.";

    return app.exec();
}
