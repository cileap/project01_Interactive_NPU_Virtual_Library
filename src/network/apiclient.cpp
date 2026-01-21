#include "apiclient.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>

ApiClient::ApiClient(QObject* parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_baseUrl("http://localhost:8888/api")  // 默认后端地址
{
    // 连接网络管理器的完成信号
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &ApiClient::onNetworkReply);
}

void ApiClient::setBaseUrl(const QString& baseUrl) {
    m_baseUrl = baseUrl;
}

void ApiClient::setUsername(const QString& username) {
    m_username = username;
}

void ApiClient::fetchSnapshots() {
    QNetworkRequest request(buildUrl("/map/snapshots"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // 添加用户身份头部（可选，用于权限验证）
    if (!m_username.isEmpty()) {
        request.setRawHeader("X-User", m_username.toUtf8());
    }

    m_networkManager->get(request);
    qDebug() << "Fetching snapshots from:" << request.url();
}

void ApiClient::addMarker(const Marker& marker) {
    QNetworkRequest request(buildUrl("/map/markers"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    if (!m_username.isEmpty()) {
        request.setRawHeader("X-User", m_username.toUtf8());
    }

    // 发送完整的标记数据（包含 id, x, y 字段，与后端 Marker::fromJson 格式匹配）
    QJsonObject json;
    json["id"] = marker.id();
    json["x"] = marker.position().x();
    json["y"] = marker.position().y();
    json["note"] = marker.note();
    json["color"] = marker.color().name();
    json["createTime"] = marker.createTime().toString(Qt::ISODate);
    json["createdBy"] = marker.createdBy();

    QJsonDocument doc(json);
    m_networkManager->post(request, doc.toJson());

    qDebug() << "Adding marker:" << marker.id() << "at" << marker.position();
}

void ApiClient::deleteMarker(const QString& markerId) {
    QString endpoint = QString("/map/markers/%1").arg(markerId);
    QNetworkRequest request(buildUrl(endpoint));

    if (!m_username.isEmpty()) {
        request.setRawHeader("X-User", m_username.toUtf8());
    }

    m_networkManager->deleteResource(request);
    qDebug() << "Deleting marker:" << markerId;
}

void ApiClient::uploadSnapshots(const QList<MapSnapshot>& snapshots) {
    QNetworkRequest request(buildUrl("/map/snapshots/batch"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    if (!m_username.isEmpty()) {
        request.setRawHeader("X-User", m_username.toUtf8());
    }

    // 构建JSON数组
    QJsonArray snapshotArray;
    for (const MapSnapshot& snapshot : snapshots) {
        snapshotArray.append(snapshot.toJson());
    }

    QJsonDocument doc(snapshotArray);
    m_networkManager->post(request, doc.toJson());

    qDebug() << "Uploading" << snapshots.size() << "snapshots";
}

void ApiClient::onNetworkReply(QNetworkReply* reply) {
    // 检查网络错误
    if (reply->error() != QNetworkReply::NoError) {
        QString errorMsg = QString("Network error: %1").arg(reply->errorString());
        qWarning() << errorMsg;
        emit errorOccurred(errorMsg);
        reply->deleteLater();
        return;
    }

    // 获取响应数据
    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);

    if (doc.isNull()) {
        QString errorMsg = "Invalid JSON response";
        qWarning() << errorMsg;
        emit errorOccurred(errorMsg);
        reply->deleteLater();
        return;
    }

    // 根据请求的URL判断响应类型并处理
    QString urlPath = reply->url().path();

    // 处理获取快照列表的响应
    if (urlPath.contains("/map/snapshots") && reply->operation() == QNetworkAccessManager::GetOperation) {
        QJsonArray snapshotArray = doc.array();
        QList<MapSnapshot> snapshots;
        for (const QJsonValue& value : snapshotArray) {
            snapshots.append(MapSnapshot::fromJson(value.toObject()));
        }
        qDebug() << "Fetched" << snapshots.size() << "snapshots";
        emit snapshotsFetched(snapshots);
    }

    // 处理添加标记的响应
    else if (urlPath.contains("/map/markers") && reply->operation() == QNetworkAccessManager::PostOperation) {
        QJsonObject json = doc.object();
        Marker marker = Marker::fromJson(json);
        qDebug() << "Marker added successfully:" << marker.id();
        emit markerAdded(marker);
    }

    // 处理删除标记的响应
    else if (urlPath.contains("/map/markers/") && reply->operation() == QNetworkAccessManager::DeleteOperation) {
        QString markerId = reply->url().fileName();  // 从URL提取markerId
        qDebug() << "Marker deleted successfully:" << markerId;
        emit markerDeleted(markerId);
    }

    reply->deleteLater();
}

QString ApiClient::buildUrl(const QString& endpoint) const {
    return m_baseUrl + endpoint;
}
