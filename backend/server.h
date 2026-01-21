#ifndef SERVER_H
#define SERVER_H

#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonObject>
#include <QJsonArray>
#include <QList>

#include "../src/data/marker.h"
#include "../src/data/mapsnapshot.h"

/**
 * @brief 简单的 HTTP 服务器
 *
 * 处理前端的所有 API 请求，支持文件持久化存储
 */
class HttpServer : public QObject {
    Q_OBJECT

public:
    explicit HttpServer(QObject* parent = nullptr);
    ~HttpServer() override = default;

    /**
     * @brief 启动服务器
     * @param port 监听端口
     * @return 成功返回 true
     */
    bool start(quint16 port = 8080);

    /**
     * @brief 停止服务器
     */
    void stop();

    /**
     * @brief 加载数据文件
     */
    void loadData();

    /**
     * @brief 保存数据到文件
     */
    void saveData();

signals:
    /**
     * @brief 请求处理完成信号
     */
    void requestProcessed();

private slots:
    /**
     * @brief 处理新连接
     */
    void onNewConnection();

    /**
     * @brief 处理请求数据
     */
    void onRequestReady();

private:
    /**
     * @brief 处理 HTTP 请求
     * @param method HTTP 方法 (GET/POST/DELETE)
     * @param path 请求路径
     * @param body 请求体
     * @param socket 客户端 socket
     */
    void handleRequest(const QString& method, const QString& path,
                      const QByteArray& body, QTcpSocket* socket);

    /**
     * @brief 发送 HTTP 响应
     * @param socket 客户端 socket
     * @param statusCode 状态码
     * @param data 响应数据
     */
    void sendResponse(QTcpSocket* socket, int statusCode,
                      const QByteArray& data = QByteArray());

    /**
     * @brief 发送 JSON 响应
     * @param socket 客户端 socket
     * @param statusCode 状态码
     * @param json JSON 对象
     */
    void sendJsonResponse(QTcpSocket* socket, int statusCode,
                          const QJsonObject& json);

    /**
     * @brief 发送 JSON 数组响应
     * @param socket 客户端 socket
     * @param statusCode 状态码
     * @param array JSON 数组
     */
    void sendJsonArrayResponse(QTcpSocket* socket, int statusCode,
                               const QJsonArray& array);

private:
    QTcpServer* m_tcpServer;
    QList<MapSnapshot> m_snapshots;  ///< 所有快照数据
    QString m_dataFile;               ///< 数据文件路径
};

#endif // SERVER_H
