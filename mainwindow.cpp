#include "mainwindow.h"
#include <QLabel>
#include <QTextEdit>
#include <QGroupBox>
#include <QDir>
#include <QDebug>
#include <QImageReader>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_mapView(nullptr)
    , m_timelineWidget(nullptr)
    , m_addMarkerButton(nullptr)
    , m_syncButton(nullptr)
    , m_markerManager(nullptr)
    , m_apiClient(nullptr)
{
    // 创建业务逻辑组件
    m_markerManager = new MarkerManager(this);
    m_apiClient = new ApiClient(this);

    // 初始化UI（必须在连接信号之前）
    setupUi();

    // 设置窗口属性
    setWindowTitle("NPU 虚拟校园地图 - 交互系统");
    resize(1200, 800);

    // 连接信号和槽（在 setupUi 之后，确保所有 UI 组件已创建）
    if (m_timelineWidget) {
        connect(m_timelineWidget, &TimelineWidget::indexChanged,
                this, &MainWindow::onTimelineIndexChanged);
        connect(m_timelineWidget, &TimelineWidget::restoreLatestRequested,
                this, &MainWindow::onRestoreLatestClicked);
    }

    connect(m_markerManager, &MarkerManager::markersChanged,
            this, &MainWindow::onMarkersChanged);
    connect(m_markerManager, &MarkerManager::currentSnapshotChanged,
            [this](int index, const MapSnapshot& snapshot) {
                if (m_timelineWidget) {
                    m_timelineWidget->setCurrentIndex(index);
                }
            });

    connect(m_apiClient, &ApiClient::snapshotsFetched,
            this, &MainWindow::onSnapshotsFetched);
    connect(m_apiClient, &ApiClient::errorOccurred,
            this, &MainWindow::onNetworkError);

    // 初始化时间轴（空状态）
    if (m_timelineWidget) {
        m_timelineWidget->setSnapshots(m_markerManager->snapshots());
    }

    qDebug() << "MainWindow initialized";
}

void MainWindow::setupUi() {
    // 创建中央部件
    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    // 主布局（横向）
    QHBoxLayout* mainLayout = new QHBoxLayout(centralWidget);

    // ========== 左侧控制面板 ==========
    QWidget* controlPanel = createControlPanel();
    controlPanel->setFixedWidth(250);
    mainLayout->addWidget(controlPanel);

    // ========== 中央地图显示区域 ==========
    m_mapView = new MapView(this);
    mainLayout->addWidget(m_mapView, 1);  // stretch=1 占据剩余空间

    // 增加 QImage 分配限制（默认 256MB，图片较大时需要）
    QImageReader::setAllocationLimit(512);  // 设置为 512MB

    // 加载地图图片（如果存在）
    QPixmap mapPixmap("map.jpg");
    qDebug() << "Loading map.jpg, isNull:" << mapPixmap.isNull()
             << "size:" << mapPixmap.size();
    if (!mapPixmap.isNull()) {
        m_mapView->setMapPixmap(mapPixmap);
        qDebug() << "Map loaded successfully";
    } else {
        qDebug() << "Failed to load map.jpg - file not found or invalid format";
        qDebug() << "Current working dir:" << QDir::currentPath();
    }

    // 连接地图视图信号
    connect(m_mapView, &MapView::addMarkerRequested,
            this, &MainWindow::onAddMarkerRequested);
    connect(m_mapView, &MapView::deleteMarkerRequested,
            this, &MainWindow::onDeleteMarkerRequested);

    // ========== 底部时间轴（停靠窗口） ==========
    QDockWidget* timelineDock = new QDockWidget("历史时间轴", this);
    timelineDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);
    timelineDock->setFeatures(QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetMovable);

    m_timelineWidget = new TimelineWidget(this);
    timelineDock->setWidget(m_timelineWidget);
    addDockWidget(Qt::BottomDockWidgetArea, timelineDock);
}

QWidget* MainWindow::createControlPanel() {
    QWidget* panel = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(15);

    // ========== 标题 ==========
    QLabel* titleLabel = new QLabel("控制面板", panel);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSize(14);
    titleLabel->setFont(titleFont);
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel);

    // ========== 添加标记组 ==========
    QGroupBox* addMarkerGroup = new QGroupBox("标记操作", panel);
    QVBoxLayout* addMarkerLayout = new QVBoxLayout(addMarkerGroup);

    m_addMarkerButton = new QPushButton("添加标记", panel);
    m_addMarkerButton->setToolTip("点击后在地图上选择位置添加标记");
    connect(m_addMarkerButton, &QPushButton::clicked,
            this, &MainWindow::onAddMarkerButtonClicked);
    addMarkerLayout->addWidget(m_addMarkerButton);

    QLabel* hintLabel = new QLabel("提示: 也可直接点击地图添加标记", panel);
    hintLabel->setStyleSheet("color: gray; font-size: 9pt;");
    hintLabel->setWordWrap(true);
    addMarkerLayout->addWidget(hintLabel);

    layout->addWidget(addMarkerGroup);

    // ========== 数据同步组 ==========
    QGroupBox* syncGroup = new QGroupBox("数据同步", panel);
    QVBoxLayout* syncLayout = new QVBoxLayout(syncGroup);

    m_syncButton = new QPushButton("从服务器同步", panel);
    connect(m_syncButton, &QPushButton::clicked,
            this, &MainWindow::onSyncFromServer);
    syncLayout->addWidget(m_syncButton);

    QLabel* serverLabel = new QLabel("服务器: http://localhost:8080", panel);
    serverLabel->setStyleSheet("color: gray; font-size: 8pt;");
    serverLabel->setWordWrap(true);
    syncLayout->addWidget(serverLabel);

    layout->addWidget(syncGroup);

    // ========== 当前状态组 ==========
    QGroupBox* statusGroup = new QGroupBox("当前状态", panel);
    QVBoxLayout* statusLayout = new QVBoxLayout(statusGroup);

    QLabel* markerCountLabel = new QLabel("标记数: 0", panel);
    markerCountLabel->setObjectName("markerCountLabel");
    statusLayout->addWidget(markerCountLabel);

    QLabel* snapshotLabel = new QLabel("快照数: 0", panel);
    snapshotLabel->setObjectName("snapshotCountLabel");
    statusLayout->addWidget(snapshotLabel);

    layout->addWidget(statusGroup);

    // 连接标记管理器信号，更新状态显示
    connect(m_markerManager, &MarkerManager::markersChanged, this, [this, markerCountLabel, snapshotLabel]() {
        markerCountLabel->setText(QString("标记数: %1").arg(m_markerManager->currentMarkers().size()));
        snapshotLabel->setText(QString("快照数: %1").arg(m_markerManager->snapshotCount()));
    });

    // ========== 弹性空间 ==========
    layout->addStretch();

    // ========== 使用说明 ==========
    QGroupBox* helpGroup = new QGroupBox("使用说明", panel);
    QVBoxLayout* helpLayout = new QVBoxLayout(helpGroup);
    QTextEdit* helpText = new QTextEdit(panel);
    helpText->setReadOnly(true);
    helpText->setMaximumHeight(150);
    helpText->setHtml(
        "<ul>"
        "<li><b>添加标记:</b> 点击按钮后选择地图位置</li>"
        "<li><b>查看备注:</b> 右键点击标记查看</li>"
        "<li><b>删除标记:</b> 右键点击标记选择删除</li>"
        "<li><b>时间回溯:</b> 使用底部时间轴滑块</li>"
        "<li><b>缩放地图:</b> 鼠标滚轮</li>"
        "<li><b>拖拽地图:</b> 鼠标左键拖拽</li>"
        "</ul>"
    );
    helpText->setFrameStyle(QFrame::NoFrame);
    helpLayout->addWidget(helpText);
    layout->addWidget(helpGroup);

    return panel;
}

void MainWindow::onAddMarkerButtonClicked() {
    // 切换地图视图到添加标记模式
    m_mapView->setAddMarkerMode(true);
    m_addMarkerButton->setText("取消添加");
    m_addMarkerButton->setStyleSheet("background-color: #ffcccc");

    // 临时改变按钮行为
    disconnect(m_addMarkerButton, nullptr, nullptr, nullptr);
    connect(m_addMarkerButton, &QPushButton::clicked, this, [this]() {
        m_mapView->setAddMarkerMode(false);
        m_addMarkerButton->setText("添加标记");
        m_addMarkerButton->setStyleSheet("");

        // 恢复原始连接
        disconnect(m_addMarkerButton, nullptr, nullptr, nullptr);
        connect(m_addMarkerButton, &QPushButton::clicked,
                this, &MainWindow::onAddMarkerButtonClicked);
    });
}

void MainWindow::onAddMarkerRequested(const QPointF& normalizedPos) {
    // 关闭添加标记模式
    m_mapView->setAddMarkerMode(false);
    m_addMarkerButton->setText("添加标记");
    m_addMarkerButton->setStyleSheet("");

    // 恢复原始连接
    disconnect(m_addMarkerButton, nullptr, nullptr, nullptr);
    connect(m_addMarkerButton, &QPushButton::clicked,
            this, &MainWindow::onAddMarkerButtonClicked);

    // 显示添加标记对话框
    Marker marker = showAddMarkerDialog(normalizedPos);
    if (!marker.id().isEmpty()) {
        // 添加标记到管理器
        m_markerManager->addMarker(marker, "当前用户");

        // 同时发送到服务器
        m_apiClient->addMarker(marker);

        // 在地图上显示
        m_mapView->addMarker(marker);

        QMessageBox::information(this, "成功", "标记已添加");
    }
}

void MainWindow::onDeleteMarkerRequested(const QString& markerId) {
    // 从管理器中删除
    if (m_markerManager->deleteMarker(markerId, "当前用户")) {
        // 从地图上移除
        m_mapView->removeMarker(markerId);

        // 发送到服务器
        m_apiClient->deleteMarker(markerId);

        QMessageBox::information(this, "成功", "标记已删除");
    }
}

Marker MainWindow::showAddMarkerDialog(const QPointF& normalizedPos) {
    // 输入备注信息
    bool ok;
    QString note = QInputDialog::getText(
        this,
        "添加标记",
        "请输入备注信息:",
        QLineEdit::Normal,
        "",
        &ok
    );

    if (!ok || note.isEmpty()) {
        return Marker();  // 返回无效标记
    }

    // 选择颜色
    QColor color = QColorDialog::getColor(Qt::red, this, "选择标记颜色");
    if (!color.isValid()) {
        return Marker();  // 用户取消
    }

    // 创建标记
    Marker marker(normalizedPos, note, color);

    return marker;
}

void MainWindow::onTimelineIndexChanged(int index) {
    // 切换到指定快照
    m_markerManager->restoreSnapshot(index);
}

void MainWindow::onRestoreLatestClicked() {
    // 返回最新快照
    m_markerManager->restoreLatestSnapshot();
}

void MainWindow::onMarkersChanged(const QList<Marker>& markers) {
    // 更新地图显示
    m_mapView->clearMarkers();
    m_mapView->addMarkers(markers);
}

void MainWindow::onSyncFromServer() {
    m_syncButton->setEnabled(false);
    m_syncButton->setText("同步中...");

    m_apiClient->fetchSnapshots();

    // 重新启用按钮（在网络响应处理中）
    connect(m_apiClient, &ApiClient::snapshotsFetched, this, [this]() {
        m_syncButton->setEnabled(true);
        m_syncButton->setText("从服务器同步");
    }, Qt::SingleShotConnection);
}

void MainWindow::onSnapshotsFetched(const QList<MapSnapshot>& snapshots) {
    // 加载快照到管理器
    m_markerManager->loadFromSnapshots(snapshots);

    // 更新时间轴
    m_timelineWidget->setSnapshots(snapshots);

    QMessageBox::information(this, "同步成功",
                             QString("已同步 %1 个快照").arg(snapshots.size()));
}

void MainWindow::onNetworkError(const QString& error) {
    QMessageBox::warning(this, "网络错误", error);

    // 恢复按钮状态
    m_syncButton->setEnabled(true);
    m_syncButton->setText("从服务器同步");
}
