#include "markermanager.h"

MarkerManager::MarkerManager(QObject* parent)
    : QObject(parent)
    , m_currentSnapshotIndex(-1)  // -1 表示没有快照
{
}

bool MarkerManager::addMarker(const Marker& marker, const QString& createdBy) {
    // 检查是否处于历史视图模式
    if (m_currentSnapshotIndex >= 0 && m_currentSnapshotIndex < m_snapshots.size() - 1) {
        qWarning() << "Cannot add marker: currently viewing historical snapshot."
                   << "Please restore to latest first.";
        return false;
    }

    // 如果有历史快照，确保我们在最新的快照基础上操作
    if (!m_snapshots.isEmpty()) {
        // 确保在最新快照上操作
        restoreLatestSnapshot();
    }

    // 添加标记到当前标记列表
    m_currentMarkers[marker.id()] = marker;

    // 创建新快照记录此次添加
    QString description = QString("添加标记: %1").arg(marker.note().left(20));
    if (!createdBy.isEmpty()) {
        description += QString(" (操作者: %1)").arg(createdBy);
    }
    createSnapshot(description);

    qDebug() << "Marker added:" << marker.id() << "Total markers:" << m_currentMarkers.size();

    return true;
}

bool MarkerManager::deleteMarker(const QString& markerId, const QString& deletedBy) {
    // 检查标记是否存在
    if (!m_currentMarkers.contains(markerId)) {
        qWarning() << "Cannot delete marker: not found" << markerId;
        return false;
    }

    // 检查是否处于历史视图模式
    if (m_currentSnapshotIndex >= 0 && m_currentSnapshotIndex < m_snapshots.size() - 1) {
        qWarning() << "Cannot delete marker: currently viewing historical snapshot."
                   << "Please restore to latest first.";
        return false;
    }

    // 确保在最新快照上操作
    if (!m_snapshots.isEmpty()) {
        restoreLatestSnapshot();
    }

    // 获取标记信息用于日志
    Marker marker = m_currentMarkers[markerId];

    // 从当前标记列表中移除
    m_currentMarkers.remove(markerId);

    // 创建新快照记录此次删除
    QString description = QString("删除标记: %1").arg(marker.note().left(20));
    if (!deletedBy.isEmpty()) {
        description += QString(" (操作者: %1)").arg(deletedBy);
    }
    createSnapshot(description);

    qDebug() << "Marker deleted:" << markerId << "Total markers:" << m_currentMarkers.size();

    return true;
}

QList<Marker> MarkerManager::currentMarkers() const {
    // 如果有快照且当前在查看某个快照，返回快照中的标记
    if (m_currentSnapshotIndex >= 0 && m_currentSnapshotIndex < m_snapshots.size()) {
        return m_snapshots[m_currentSnapshotIndex].markers();
    }

    // 否则返回当前内存中的标记
    return m_currentMarkers.values();
}

Marker MarkerManager::findMarker(const QString& markerId) const {
    // 先在当前标记中查找
    if (m_currentMarkers.contains(markerId)) {
        return m_currentMarkers[markerId];
    }

    // 如果没找到，在当前快照的标记中查找
    if (m_currentSnapshotIndex >= 0 && m_currentSnapshotIndex < m_snapshots.size()) {
        for (const Marker& marker : m_snapshots[m_currentSnapshotIndex].markers()) {
            if (marker.id() == markerId) {
                return marker;
            }
        }
    }

    // 没找到，返回无效标记
    return Marker();
}

bool MarkerManager::restoreSnapshot(int index) {
    if (index < 0 || index >= m_snapshots.size()) {
        qWarning() << "Invalid snapshot index:" << index;
        return false;
    }

    m_currentSnapshotIndex = index;
    const MapSnapshot& snapshot = m_snapshots[index];

    // 更新当前标记列表
    m_currentMarkers.clear();
    for (const Marker& marker : snapshot.markers()) {
        m_currentMarkers[marker.id()] = marker;
    }

    qDebug() << "Restored snapshot:" << snapshot.snapshotId()
             << "at" << snapshot.timestamp();

    emit currentSnapshotChanged(index, snapshot);
    emit markersChanged(snapshot.markers());

    return true;
}

void MarkerManager::restoreLatestSnapshot() {
    if (m_snapshots.isEmpty()) {
        return;
    }

    restoreSnapshot(m_snapshots.size() - 1);
}

void MarkerManager::createSnapshot(const QString& description) {
    MapSnapshot snapshot = createSnapshotInternal(description);
    m_snapshots.append(snapshot);
    m_currentSnapshotIndex = m_snapshots.size() - 1;

    // 更新当前标记列表到最新状态
    m_currentMarkers.clear();
    for (const Marker& marker : snapshot.markers()) {
        m_currentMarkers[marker.id()] = marker;
    }

    emit snapshotCreated(snapshot);
    emit currentSnapshotChanged(m_currentSnapshotIndex, snapshot);
    emit markersChanged(snapshot.markers());
}

const MapSnapshot& MarkerManager::snapshotAt(int index) const {
    if (index < 0 || index >= m_snapshots.size()) {
        static MapSnapshot invalidSnapshot;
        return invalidSnapshot;
    }
    return m_snapshots[index];
}

void MarkerManager::loadFromSnapshots(const QList<MapSnapshot>& snapshots) {
    m_snapshots = snapshots;
    if (!m_snapshots.isEmpty()) {
        // 默认加载到最新快照
        restoreLatestSnapshot();
    }
}

MapSnapshot MarkerManager::createSnapshotInternal(const QString& description) {
    QDateTime now = QDateTime::currentDateTime();
    QList<Marker> markers = m_currentMarkers.values();

    MapSnapshot snapshot(now, markers, description);
    return snapshot;
}
