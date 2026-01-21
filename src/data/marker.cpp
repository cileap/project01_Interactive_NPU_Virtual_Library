#include "marker.h"
#include <QRandomGenerator>

Marker::Marker(const QPointF& position,
               const QString& note,
               const QColor& color,
               const QDateTime& createTime,
               const QString& createdBy)
    : m_id(generateId())
    , m_position(position)
    , m_note(note)
    , m_color(color)
    , m_createTime(createTime)
    , m_createdBy(createdBy)
{
}

QString Marker::generateId() {
    // 使用时间戳 + 随机数生成唯一ID
    return QString("marker-%1-%2")
        .arg(QDateTime::currentMSecsSinceEpoch())
        .arg(QRandomGenerator::global()->bounded(10000));
}

QJsonObject Marker::toJson() const {
    QJsonObject obj;
    obj["id"] = m_id;
    obj["x"] = m_position.x();
    obj["y"] = m_position.y();
    obj["note"] = m_note;
    obj["color"] = m_color.name();
    obj["createTime"] = m_createTime.toString(Qt::ISODate);
    obj["createdBy"] = m_createdBy;
    return obj;
}

Marker Marker::fromJson(const QJsonObject& json) {
    Marker marker;
    marker.m_id = json["id"].toString();
    marker.m_position = QPointF(json["x"].toDouble(), json["y"].toDouble());
    marker.m_note = json["note"].toString();
    marker.m_color = QColor(json["color"].toString());
    marker.m_createTime = QDateTime::fromString(json["createTime"].toString(), Qt::ISODate);
    marker.m_createdBy = json["createdBy"].toString();
    return marker;
}
