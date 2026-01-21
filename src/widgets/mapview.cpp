#include "mapview.h"
#include <QApplication>
#include <QMenu>
#include <QMessageBox>
#include <QDebug>
#include <QScrollBar>

MapView::MapView(QWidget* parent)
    : QGraphicsView(parent)
    , m_scene(nullptr)
    , m_mapItem(nullptr)
    , m_isDragging(false)
    , m_lastDragPos(QPoint())
    , m_zoomLevel(1.0)
    , m_minZoom(0.1)
    , m_maxZoom(10.0)
    , m_addMarkerMode(false)
    , m_mapSize(800, 600)  // 默认地图尺寸
{
    // 创建并设置场景
    m_scene = new QGraphicsScene(this);
    m_scene->setSceneRect(0, 0, m_mapSize.x(), m_mapSize.y());
    setScene(m_scene);

    // 视图设置
    setRenderHint(QPainter::Antialiasing);       // 抗锯齿
    setRenderHint(QPainter::SmoothPixmapTransform); // 平滑像素变换
    setDragMode(QGraphicsView::NoDrag);          // 初始不拖拽（我们自己处理）
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);  // 缩放中心为鼠标位置
    setResizeAnchor(QGraphicsView::NoAnchor);

    // 创建默认地图占位符（灰色背景，带文字提示）
    QPixmap placeholderPixmap(static_cast<int>(m_mapSize.x()), static_cast<int>(m_mapSize.y()));
    placeholderPixmap.fill(QColor(220, 220, 220));

    // 在占位符上绘制提示文字
    QPainter painter(&placeholderPixmap);
    painter.setPen(QColor(150, 150, 150));
    QFont font = painter.font();
    font.setPixelSize(24);
    painter.setFont(font);
    painter.drawText(placeholderPixmap.rect(), Qt::AlignCenter, "地图图片未加载\n请在代码中设置地图");
    painter.end();

    // 添加地图图片到场景
    m_mapItem = m_scene->addPixmap(placeholderPixmap);
    m_mapItem->setZValue(-1);  // 设置为最底层
    m_mapItem->setPos(0, 0);
}

void MapView::setMapPixmap(const QPixmap& pixmap) {
    if (!m_mapItem) {
        return;
    }

    // 移除旧地图
    m_scene->removeItem(m_mapItem);
    delete m_mapItem;

    // 更新地图尺寸
    m_mapSize = QPointF(pixmap.width(), pixmap.height());
    m_scene->setSceneRect(0, 0, m_mapSize.x(), m_mapSize.y());

    // 添加新地图
    m_mapItem = m_scene->addPixmap(pixmap);
    m_mapItem->setZValue(-1);
    m_mapItem->setPos(0, 0);

    // 重新添加所有标记（因为它们可能在旧地图上）
    // 注意：这里简化处理，实际可能需要重新计算标记位置
}

void MapView::clearMarkers() {
    // 移除所有标记图形项
    for (QGraphicsEllipseItem* item : m_markerItems) {
        m_scene->removeItem(item);
        delete item;
    }
    m_markerItems.clear();
}

void MapView::addMarker(const Marker& marker) {
    // 将归一化坐标转换为像素坐标
    QPointF pixelPos = normalizedToPixel(marker.position());

    // 计算标记点的大小（根据缩放级别调整，保持视觉大小一致）
    const double baseRadius = 10.0;  // 基础半径
    double visualRadius = baseRadius / m_zoomLevel;

    // 创建标记图形项（圆形）
    QGraphicsEllipseItem* markerItem = m_scene->addEllipse(
        pixelPos.x() - visualRadius,
        pixelPos.y() - visualRadius,
        visualRadius * 2,
        visualRadius * 2,
        QPen(Qt::black, 1 / m_zoomLevel),  // 边框
        QBrush(marker.color())             // 填充颜色
    );

    // 存储标记ID（用于点击识别）
    markerItem->setData(0, marker.id());  // data(0) 存储 markerId
    markerItem->setData(1, marker.note()); // data(1) 存储备注信息
    markerItem->setData(2, marker.createTime().toString(Qt::ISODate)); // data(2) 存储创建时间

    // 设置可点击标志
    markerItem->setFlag(QGraphicsItem::ItemIsSelectable);

    // 添加到映射表
    m_markerItems[marker.id()] = markerItem;

    qDebug() << "Marker added:" << marker.id() << "at" << pixelPos;
}

void MapView::addMarkers(const QList<Marker>& markers) {
    for (const Marker& marker : markers) {
        addMarker(marker);
    }
}

void MapView::removeMarker(const QString& markerId) {
    if (!m_markerItems.contains(markerId)) {
        qWarning() << "Marker not found:" << markerId;
        return;
    }

    QGraphicsEllipseItem* item = m_markerItems.take(markerId);
    m_scene->removeItem(item);
    delete item;

    qDebug() << "Marker removed:" << markerId;
}

QPointF MapView::pixelToNormalized(const QPointF& pixelPos) const {
    return QPointF(
        pixelPos.x() / m_mapSize.x(),
        pixelPos.y() / m_mapSize.y()
    );
}

QPointF MapView::normalizedToPixel(const QPointF& normalizedPos) const {
    return QPointF(
        normalizedPos.x() * m_mapSize.x(),
        normalizedPos.y() * m_mapSize.y()
    );
}

void MapView::setAddMarkerMode(bool enabled) {
    m_addMarkerMode = enabled;

    // 更新鼠标光标
    if (enabled) {
        setCursor(Qt::CrossCursor);
    } else {
        setCursor(Qt::ArrowCursor);
    }
}

void MapView::wheelEvent(QWheelEvent* event) {
    // 计算缩放因子
    const double zoomFactor = 1.15;
    double newZoom = m_zoomLevel;

    if (event->angleDelta().y() > 0) {
        // 向上滚动，放大
        newZoom *= zoomFactor;
    } else {
        // 向下滚动，缩小
        newZoom /= zoomFactor;
    }

    // 限制缩放范围
    newZoom = qBound(m_minZoom, newZoom, m_maxZoom);

    // 应用缩放
    double scaleChange = newZoom / m_zoomLevel;
    scale(scaleChange, scaleChange);
    m_zoomLevel = newZoom;

    // 更新标记外观
    updateMarkerAppearance();

    // 发射信号
    emit zoomChanged(m_zoomLevel);

    qDebug() << "Zoom level:" << m_zoomLevel;
}

void MapView::mousePressEvent(QMouseEvent* event) {
    if (!scene()) {
        QGraphicsView::mousePressEvent(event);
        return;
    }

    // 检查是否点击了标记
    QGraphicsItem* item = scene()->itemAt(mapToScene(event->pos()), transform());
    QGraphicsEllipseItem* markerItem = dynamic_cast<QGraphicsEllipseItem*>(item);

    if (markerItem) {
        // 点击了标记，获取标记信息
        QString markerId = markerItem->data(0).toString();
        QString note = markerItem->data(1).toString();

        qDebug() << "Marker clicked:" << markerId << "Note:" << note;

        if (event->button() == Qt::RightButton) {
            // 右键点击，显示上下文菜单
            showMarkerContextMenu(event->pos(), markerId, note);
        } else {
            // 左键点击，发射信号
            emit markerClicked(markerId);
        }
        return;
    }

    if (event->button() == Qt::LeftButton) {
        if (m_addMarkerMode) {
            // 添加标记模式：获取点击位置并发射信号
            QPointF scenePos = mapToScene(event->pos());
            QPointF normalizedPos = pixelToNormalized(scenePos);
            qDebug() << "Add marker requested at:" << normalizedPos;
            emit addMarkerRequested(normalizedPos);
            return;
        }

        // 普通模式：开始拖拽
        m_isDragging = true;
        m_lastDragPos = event->pos();
        setCursor(Qt::ClosedHandCursor);
    }

    QGraphicsView::mousePressEvent(event);
}

void MapView::mouseMoveEvent(QMouseEvent* event) {
    if (m_isDragging && (event->buttons() & Qt::LeftButton)) {
        // 计算拖拽偏移
        QPoint delta = event->pos() - m_lastDragPos;
        m_lastDragPos = event->pos();

        // 平移视图
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
    }

    QGraphicsView::mouseMoveEvent(event);
}

void MapView::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && m_isDragging) {
        m_isDragging = false;
        setCursor(m_addMarkerMode ? Qt::CrossCursor : Qt::ArrowCursor);
    }

    QGraphicsView::mouseReleaseEvent(event);
}

void MapView::contextMenuEvent(QContextMenuEvent* event) {
    if (!scene()) {
        QGraphicsView::contextMenuEvent(event);
        return;
    }

    // 检查是否右键点击了标记
    QGraphicsItem* item = scene()->itemAt(mapToScene(event->pos()), transform());
    QGraphicsEllipseItem* markerItem = dynamic_cast<QGraphicsEllipseItem*>(item);

    if (markerItem) {
        // 右键点击了标记，显示上下文菜单
        QString markerId = markerItem->data(0).toString();
        QString note = markerItem->data(1).toString();
        showMarkerContextMenu(event->pos(), markerId, note);
    }

    QGraphicsView::contextMenuEvent(event);
}

void MapView::showMarkerContextMenu(const QPoint& pos, const QString& markerId, const QString& note) {
    QMenu menu(this);

    // 查看备注
    QAction* viewAction = menu.addAction("查看备注");
    connect(viewAction, &QAction::triggered, this, [this, note]() {
        QMessageBox::information(this, "标记备注", note.isEmpty() ? "无备注" : note);
    });

    menu.addSeparator();

    // 删除标记（管理员权限功能）
    QAction* deleteAction = menu.addAction("删除标记");
    deleteAction->setIcon(QIcon::fromTheme("edit-delete"));
    connect(deleteAction, &QAction::triggered, this, [this, markerId]() {
        // 确认删除
        auto reply = QMessageBox::question(
            this,
            "确认删除",
            "确定要删除这个标记吗？",
            QMessageBox::Yes | QMessageBox::No
        );

        if (reply == QMessageBox::Yes) {
            emit deleteMarkerRequested(markerId);
        }
    });

    // 在鼠标位置显示菜单
    menu.exec(mapToGlobal(pos));
}

void MapView::updateMarkerAppearance() {
    // 更新所有标记的大小，使其在视觉上保持一致
    const double baseRadius = 10.0;
    const double basePenWidth = 1.0;

    for (QGraphicsEllipseItem* item : m_markerItems) {
        double visualRadius = baseRadius / m_zoomLevel;
        double visualPenWidth = basePenWidth / m_zoomLevel;

        // 更新椭圆大小
        QRectF rect = item->rect();
        QPointF center = rect.center();
        item->setRect(
            center.x() - visualRadius,
            center.y() - visualRadius,
            visualRadius * 2,
            visualRadius * 2
        );

        // 更新边框宽度
        item->setPen(QPen(Qt::black, visualPenWidth));
    }
}
