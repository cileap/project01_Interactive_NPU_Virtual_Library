#include "server.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QCoreApplication>
#include <QUrlQuery>
#include <QRegularExpression>

HttpServer::HttpServer(QObject* parent)
    : QObject(parent)
    , m_tcpServer(new QTcpServer(this))
    , m_dataFile("map_data.json")
{
    // 加载持久化数据
    loadData();

    // 连接新连接信号
    connect(m_tcpServer, &QTcpServer::newConnection,
            this, &HttpServer::onNewConnection);
}

bool HttpServer::start(quint16 port) {
    if (!m_tcpServer->listen(QHostAddress::Any, port)) {
        qWarning() << "Server failed to start:" << m_tcpServer->errorString();
        return false;
    }

    qDebug() << "Server started on port" << port;
    qDebug() << "Data file:" << m_dataFile;
    return true;
}

void HttpServer::stop() {
    if (m_tcpServer->isListening()) {
        m_tcpServer->close();
        qDebug() << "Server stopped";
    }
}

void HttpServer::loadData() {
    QFile file(m_dataFile);
    if (!file.exists()) {
        qDebug() << "Data file not found, starting with empty data";
        return;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open data file for reading:" << file.errorString();
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray()) {
        qWarning() << "Invalid data format, expected array";
        return;
    }

    QJsonArray snapshotArray = doc.array();
    m_snapshots.clear();
    for (const QJsonValue& value : snapshotArray) {
        m_snapshots.append(MapSnapshot::fromJson(value.toObject()));
    }

    qDebug() << "Loaded" << m_snapshots.size() << "snapshots from file";
}

void HttpServer::saveData() {
    QFile file(m_dataFile);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to open data file for writing:" << file.errorString();
        return;
    }

    QJsonArray snapshotArray;
    for (const MapSnapshot& snapshot : m_snapshots) {
        snapshotArray.append(snapshot.toJson());
    }

    QJsonDocument doc(snapshotArray);
    file.write(doc.toJson());
    file.close();

    qDebug() << "Saved" << m_snapshots.size() << "snapshots to file";
}

void HttpServer::onNewConnection() {
    QTcpSocket* socket = m_tcpServer->nextPendingConnection();
    connect(socket, &QTcpSocket::readyRead, this, &HttpServer::onRequestReady);
    connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
}

void HttpServer::onRequestReady() {
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    QByteArray data = socket->readAll();
    QString request = QString::fromUtf8(data);

    // 解析 HTTP 请求
    QStringList lines = request.split("\r\n");
    if (lines.isEmpty()) return;

    // 请求行: GET /api/map/snapshots HTTP/1.1
    QStringList requestLine = lines[0].split(' ');
    if (requestLine.size() < 2) {
        sendResponse(socket, 400, "Bad Request");
        return;
    }

    QString method = requestLine[0];
    QString path = requestLine[1];
    QString body;

    // 提取请求体
    int emptyLinePos = request.indexOf("\r\n\r\n");
    if (emptyLinePos >= 0) {
        body = request.mid(emptyLinePos + 4);
    }

    qDebug() << "Request:" << method << path;

    handleRequest(method, path, body.toUtf8(), socket);
}

void HttpServer::handleRequest(const QString& method, const QString& path,
                                const QByteArray& body, QTcpSocket* socket) {
    // GET /api/map/snapshots - 获取所有快照
    if (method == "GET" && path == "/api/map/snapshots") {
        QJsonArray snapshotArray;
        for (const MapSnapshot& snapshot : m_snapshots) {
            snapshotArray.append(snapshot.toJson());
        }
        sendJsonArrayResponse(socket, 200, snapshotArray);
        return;
    }

    // POST /api/map/markers - 添加标记
    if (method == "POST" && path == "/api/map/markers") {
        QJsonDocument doc = QJsonDocument::fromJson(body);
        if (!doc.isObject()) {
            sendResponse(socket, 400, "Invalid JSON");
            return;
        }

        Marker marker = Marker::fromJson(doc.object());

        // 创建新快照
        QList<Marker> currentMarkers;
        if (!m_snapshots.isEmpty()) {
            currentMarkers = m_snapshots.last().markers();
        }
        currentMarkers.append(marker);

        QString description = QString("添加标记: %1").arg(marker.note().left(20));
        MapSnapshot newSnapshot(QDateTime::currentDateTime(), currentMarkers, description);
        m_snapshots.append(newSnapshot);

        // 持久化
        saveData();

        sendJsonResponse(socket, 201, marker.toJson());
        qDebug() << "Marker added:" << marker.id();
        return;
    }

    // DELETE /api/map/markers/{id} - 删除标记
    if (method == "DELETE" && path.startsWith("/api/map/markers/")) {
        QString markerId = path.mid(QString("/api/map/markers/").length());

        // 从最新快照中删除标记
        if (m_snapshots.isEmpty()) {
            sendResponse(socket, 404, "No snapshots found");
            return;
        }

        QList<Marker> currentMarkers = m_snapshots.last().markers();
        bool found = false;
        Marker deletedMarker;

        for (int i = 0; i < currentMarkers.size(); ++i) {
            if (currentMarkers[i].id() == markerId) {
                deletedMarker = currentMarkers[i];
                currentMarkers.removeAt(i);
                found = true;
                break;
            }
        }

        if (!found) {
            sendResponse(socket, 404, "Marker not found");
            return;
        }

        // 创建新快照
        QString description = QString("删除标记: %1").arg(deletedMarker.note().left(20));
        MapSnapshot newSnapshot(QDateTime::currentDateTime(), currentMarkers, description);
        m_snapshots.append(newSnapshot);

        // 持久化
        saveData();

        // 返回被删除的标记ID
        QJsonObject response;
        response["markerId"] = markerId;
        sendJsonResponse(socket, 200, response);
        qDebug() << "Marker deleted:" << markerId;
        return;
    }

    // POST /api/map/snapshots/batch - 批量上传快照
    if (method == "POST" && path == "/api/map/snapshots/batch") {
        QJsonDocument doc = QJsonDocument::fromJson(body);
        if (!doc.isArray()) {
            sendResponse(socket, 400, "Invalid JSON array");
            return;
        }

        QJsonArray snapshotArray = doc.array();
        for (const QJsonValue& value : snapshotArray) {
            m_snapshots.append(MapSnapshot::fromJson(value.toObject()));
        }

        // 持久化
        saveData();

        QJsonObject response;
        response["message"] = QString("Uploaded %1 snapshots").arg(snapshotArray.size());
        sendJsonResponse(socket, 201, response);
        qDebug() << "Uploaded" << snapshotArray.size() << "snapshots";
        return;
    }

    // 404 Not Found
    sendResponse(socket, 404, "Not Found");
}

void HttpServer::sendResponse(QTcpSocket* socket, int statusCode,
                              const QByteArray& data) {
    QString statusText;
    switch (statusCode) {
        case 200: statusText = "OK"; break;
        case 201: statusText = "Created"; break;
        case 400: statusText = "Bad Request"; break;
        case 404: statusText = "Not Found"; break;
        case 500: statusText = "Internal Server Error"; break;
        default: statusText = "Unknown"; break;
    }

    QString response = QString("HTTP/1.1 %1 %2\r\n"
                               "Content-Type: text/plain\r\n"
                               "Content-Length: %3\r\n"
                               "Connection: close\r\n"
                               "\r\n%4")
                          .arg(statusCode)
                          .arg(statusText)
                          .arg(data.size())
                          .arg(QString::fromUtf8(data));

    socket->write(response.toUtf8());
    socket->flush();
    socket->disconnectFromHost();
}

void HttpServer::sendJsonResponse(QTcpSocket* socket, int statusCode,
                                  const QJsonObject& json) {
    QJsonDocument doc(json);
    QByteArray data = doc.toJson(QJsonDocument::Compact);

    QString statusText;
    switch (statusCode) {
        case 200: statusText = "OK"; break;
        case 201: statusText = "Created"; break;
        case 400: statusText = "Bad Request"; break;
        case 404: statusText = "Not Found"; break;
        default: statusText = "Unknown"; break;
    }

    QString response = QString("HTTP/1.1 %1 %2\r\n"
                               "Content-Type: application/json\r\n"
                               "Content-Length: %3\r\n"
                               "Access-Control-Allow-Origin: *\r\n"
                               "Connection: close\r\n"
                               "\r\n%4")
                          .arg(statusCode)
                          .arg(statusText)
                          .arg(data.size())
                          .arg(QString::fromUtf8(data));

    socket->write(response.toUtf8());
    socket->flush();
    socket->disconnectFromHost();
}

void HttpServer::sendJsonArrayResponse(QTcpSocket* socket, int statusCode,
                                       const QJsonArray& array) {
    QJsonDocument doc(array);
    QByteArray data = doc.toJson(QJsonDocument::Compact);

    QString statusText;
    switch (statusCode) {
        case 200: statusText = "OK"; break;
        default: statusText = "Unknown"; break;
    }

    QString response = QString("HTTP/1.1 %1 %2\r\n"
                               "Content-Type: application/json\r\n"
                               "Content-Length: %3\r\n"
                               "Access-Control-Allow-Origin: *\r\n"
                               "Connection: close\r\n"
                               "\r\n%4")
                          .arg(statusCode)
                          .arg(statusText)
                          .arg(data.size())
                          .arg(QString::fromUtf8(data));

    socket->write(response.toUtf8());
    socket->flush();
    socket->disconnectFromHost();
}
