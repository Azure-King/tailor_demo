#include <QApplication>
#include <QMainWindow>
#include <QSplitter>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTextEdit>
#include <QVBoxLayout>

#include <QAction>
#include <QToolBar>
#include <QMenu>
#include <algorithm>
#include <QFileDialog>
#include <QDebug>
#include <QKeySequence>
#include <QBrush>
#include <QColorDialog>
#include <QTimer>
#include <QMessageBox>
#include <QScrollBar>

#include "Sketch2DView.h"
#include "FourViewContainer.h"
#include "PolygonIO.h"
#include "BooleanOperations.h"

// 更新布尔运算结果模型树的辅助函数
void updateBooleanResultsTree(Sketch2DView* view, QTreeWidgetItem* booleanResultsItem) {
    // 清空现有子节点
    while (booleanResultsItem->childCount() > 0) {
        delete booleanResultsItem->child(0);
    }

    // 获取布尔运算结果
    const auto& booleanResults = view->booleanResults();

    // 添加结果多边形到模型树
    for (int i = 0; i < booleanResults.size(); ++i) {
        const auto& resultPoly = booleanResults[i];
        QString name = QString("结果%1").arg(i + 1);
        if (resultPoly.isHole) {
            name += " (内环)";
        }

        auto* item = new QTreeWidgetItem(booleanResultsItem);
        item->setText(0, name);
        item->setData(0, Qt::UserRole, 100 + i); // 100+ 表示布尔运算结果
        item->setData(0, Qt::UserRole + 1, resultPoly.isHole); // 存储是否为内环

        // 根据是否为内环设置颜色
        if (resultPoly.isHole) {
            item->setForeground(0, QBrush(QColor(128, 128, 128))); // 灰色表示内环
        }
    }
}

// 更新 Clip Pattern 结果模型树的辅助函数
void updateClipPatternResultsTree(Sketch2DView* view, QTreeWidgetItem* clipPatternResultsItem) {
    // 清空现有子节点
    while (clipPatternResultsItem->childCount() > 0) {
        delete clipPatternResultsItem->child(0);
    }

    // 获取 Clip Pattern 结果
    const auto& clipPatternResults = view->clipPatternResults();

    // 添加结果多边形到模型树
    for (int i = 0; i < clipPatternResults.size(); ++i) {
        const auto& resultPoly = clipPatternResults[i];
        QString name = QString("Clip结果%1").arg(i + 1);
        if (resultPoly.isHole) {
            name += " (内环)";
        }

        auto* item = new QTreeWidgetItem(clipPatternResultsItem);
        item->setText(0, name);
        item->setData(0, Qt::UserRole, 200 + i); // 200+ 表示 Clip Pattern 结果
        item->setData(0, Qt::UserRole + 1, resultPoly.isHole); // 存储是否为内环

        // 根据是否为内环设置颜色
        if (resultPoly.isHole) {
            item->setForeground(0, QBrush(QColor(128, 128, 128))); // 灰色表示内环
        }
    }
}

// 更新 Subject Pattern 结果模型树的辅助函数
void updateSubjectPatternResultsTree(Sketch2DView* view, QTreeWidgetItem* subjectPatternResultsItem) {
    // 清空现有子节点
    while (subjectPatternResultsItem->childCount() > 0) {
        delete subjectPatternResultsItem->child(0);
    }

    // 获取 Subject Pattern 结果
    const auto& subjectPatternResults = view->subjectPatternResults();

    // 添加结果多边形到模型树
    for (int i = 0; i < subjectPatternResults.size(); ++i) {
        const auto& resultPoly = subjectPatternResults[i];
        QString name = QString("Subject结果%1").arg(i + 1);
        if (resultPoly.isHole) {
            name += " (内环)";
        }

        auto* item = new QTreeWidgetItem(subjectPatternResultsItem);
        item->setText(0, name);
        item->setData(0, Qt::UserRole, 300 + i); // 300+ 表示 Subject Pattern 结果
        item->setData(0, Qt::UserRole + 1, resultPoly.isHole); // 存储是否为内环

        // 根据是否为内环设置颜色
        if (resultPoly.isHole) {
            item->setForeground(0, QBrush(QColor(128, 128, 128))); // 灰色表示内环
        }
    }
}

// 更新 Drafting 线段模型树的辅助函数
void updateDraftingSegmentsTree(FourViewContainer* viewContainer, QTreeWidgetItem* draftingSegmentsItem) {
    // 保存当前选中的 drafting 线段索引
    QSet<int> selectedSegIndices;
    auto* tree = draftingSegmentsItem->treeWidget();
    if (tree) {
        for (auto* item : tree->selectedItems()) {
            if (item->parent() == draftingSegmentsItem) {
                int type = item->data(0, Qt::UserRole).toInt();
                if (type >= 400) {
                    selectedSegIndices.insert(type - 400);
                }
            }
        }
    }

    // 清空现有子节点（阻塞信号避免信息面板闪动）
    if (tree) tree->blockSignals(true);
    while (draftingSegmentsItem->childCount() > 0) {
        delete draftingSegmentsItem->child(0);
    }

    const auto& segments = viewContainer->draftingSegments();

    for (int i = 0; i < (int)segments.size(); ++i) {
        const auto& seg = segments[i];

        QString flags;
        if (seg.reversed) flags += "反";
        else flags += "正";
        if (seg.isArc()) flags += ",弧";
        if (seg.end) flags += ",end";
        if (seg.discarded) flags += ",无效";  // discarded 与 end 含义相近但表示相反

        QString name = QString("线段#%1: %2 (%3)→(%4) [%5]")
            .arg(seg.index)
            .arg(seg.setName())
            .arg(QString("(%1,%2)").arg(seg.startX, 0, 'f', 1).arg(seg.startY, 0, 'f', 1))
            .arg(QString("(%1,%2)").arg(seg.endX, 0, 'f', 1).arg(seg.endY, 0, 'f', 1))
            .arg(flags);

        auto* item = new QTreeWidgetItem(draftingSegmentsItem);
        item->setText(0, name);
        item->setData(0, Qt::UserRole, 400 + seg.index); // 400+ 表示 Drafting 线段

        // 着色规则：discarded边(被废弃的中间边)使用灰色，正常end边按集合着色
        if (seg.discarded) {
            item->setForeground(0, QBrush(QColor(128, 128, 128))); // 灰色 = 废弃中间边
        } else if (seg.isPolygonSetB) {
            item->setForeground(0, QBrush(QColor(0, 120, 255))); // 蓝色 = Clip
        } else {
            item->setForeground(0, QBrush(QColor(255, 80, 80))); // 红色 = Subject
        }

        // 恢复之前选中的线段
        if (selectedSegIndices.contains(seg.index)) {
            item->setSelected(true);
        }
    }

    // 恢复信号，手动触发一次 selectionChanged 以更新信息面板
    if (tree) {
        tree->blockSignals(false);
        if (!selectedSegIndices.isEmpty()) {
            emit tree->itemSelectionChanged();
        }
    }
}

// 更新所有结果模型树的辅助函数
void updateAllResultsTrees(FourViewContainer* viewContainer,
    QTreeWidgetItem* booleanResultsItem,
    QTreeWidgetItem* clipPatternResultsItem,
    QTreeWidgetItem* subjectPatternResultsItem,
    QTreeWidgetItem* draftingSegmentsItem) {
    // 更新布尔运算结果
    updateBooleanResultsTree(viewContainer->bottomRightView(), booleanResultsItem);

    // 更新 Clip Pattern 结果
    updateClipPatternResultsTree(viewContainer->bottomLeftView(), clipPatternResultsItem);

    // 更新 Subject Pattern 结果
    updateSubjectPatternResultsTree(viewContainer->topRightView(), subjectPatternResultsItem);

    // 更新 Drafting 线段（仅在调试模式下才刷新，实时更新太慢会拖慢四视图）
    if (viewContainer->isDebugMode()) {
        updateDraftingSegmentsTree(viewContainer, draftingSegmentsItem);
    }
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    QMainWindow window;
    window.resize(1200, 700);

    // 创建主水平分割器
    auto* mainSplitter = new QSplitter(Qt::Horizontal, &window);

    // 创建左侧垂直分割器，用于分隔输入和输出模型树
    auto* leftSplitter = new QSplitter(Qt::Vertical, mainSplitter);
    leftSplitter->setMinimumWidth(150);
    leftSplitter->setMaximumWidth(300);

    // ==================== 输入模型树（上面部分） ====================
    auto* inputModelTree = new QTreeWidget(leftSplitter);
    inputModelTree->setHeaderLabel("输入模型");
    inputModelTree->setSelectionMode(QAbstractItemView::ExtendedSelection);

    // 输入多段线节点
    auto* polylinesItem = new QTreeWidgetItem(inputModelTree);
    polylinesItem->setText(0, "多段线");
    polylinesItem->setExpanded(true);

    // 输入多边形节点
    auto* polygonsItem = new QTreeWidgetItem(inputModelTree);
    polygonsItem->setText(0, "多边形");
    polygonsItem->setExpanded(true);

    // ==================== 输出模型树（下面部分） ====================
    auto* outputModelTree = new QTreeWidget(leftSplitter);
    outputModelTree->setHeaderLabel("输出结果");
    outputModelTree->setSelectionMode(QAbstractItemView::ExtendedSelection);

    // 布尔运算结果节点
    auto* booleanResultsItem = new QTreeWidgetItem(outputModelTree);
    booleanResultsItem->setText(0, "布尔运算结果");
    booleanResultsItem->setExpanded(true);

    // Clip Pattern 结果节点
    auto* clipPatternResultsItem = new QTreeWidgetItem(outputModelTree);
    clipPatternResultsItem->setText(0, "Clip Pattern");
    clipPatternResultsItem->setExpanded(true);

    // Subject Pattern 结果节点
    auto* subjectPatternResultsItem = new QTreeWidgetItem(outputModelTree);
    subjectPatternResultsItem->setText(0, "Subject Pattern");
    subjectPatternResultsItem->setExpanded(true);

    // Drafting 线段节点
    auto* draftingSegmentsItem = new QTreeWidgetItem(outputModelTree);
    draftingSegmentsItem->setText(0, "Drafting 线段");
    draftingSegmentsItem->setExpanded(true);

    // Drafting 线段信息面板（嵌入在左侧，非弹窗）
    auto* draftingInfoPanel = new QTextEdit(leftSplitter);
    draftingInfoPanel->setReadOnly(true);
    draftingInfoPanel->setPlaceholderText("选中 Drafting 线段查看详细信息");
    draftingInfoPanel->setMinimumHeight(80);

    // 设置左侧分割器的比例（输入在上，输出在中，信息面板在下）
    leftSplitter->setStretchFactor(0, 1);  // 输入模型树
    leftSplitter->setStretchFactor(1, 2);  // 输出模型树
    leftSplitter->setStretchFactor(2, 1);  // 信息面板（可自由拉长）
    leftSplitter->setSizes({ 200, 300, 150 });

    // Use FourViewContainer instead of single Sketch2DView
    auto* viewContainer = new FourViewContainer(&window);
    viewContainer->setMinimumSize(800, 300);

    auto* view = viewContainer->mainView(); // Main view for tool selection

    mainSplitter->addWidget(leftSplitter);
    mainSplitter->addWidget(viewContainer);
    mainSplitter->setStretchFactor(0, 0);
    mainSplitter->setStretchFactor(1, 1);
    mainSplitter->setSizes({ 300, 1000 });

    window.setCentralWidget(mainSplitter);

    auto* toolbar = window.addToolBar("Tools");
    toolbar->setMovable(false);

    QAction* polygonAction = toolbar->addAction("Polygon");
    polygonAction->setCheckable(true);

    QAction* polylineAction = toolbar->addAction("Polyline");
    polylineAction->setCheckable(true);

    // 暂时注释掉 Clear 按钮
    //QAction* clearAction = toolbar->addAction("Clear");

    QAction* importAction = toolbar->addAction("Import");
    importAction->setShortcut(QKeySequence("Ctrl+I"));

    // Drafting debug toggle button
    QAction* debugDraftingAction = toolbar->addAction("调试Drafting");
    debugDraftingAction->setCheckable(true);
    debugDraftingAction->setToolTip("切换Drafting数据结构调试视图\n直接可视化end曲线段，不调用Pattern算法");

    // 设置多边形为默认模式
    polygonAction->setChecked(true);
    view->setTool(Sketch2DView::Tool::Polygon);

    QObject::connect(polylineAction, &QAction::triggered, [&]() {
        polylineAction->setChecked(true);
        polygonAction->setChecked(false);
        view->setTool(Sketch2DView::Tool::Polyline);
        });

    QObject::connect(debugDraftingAction, &QAction::triggered, [debugDraftingAction, viewContainer]() {
        bool checked = debugDraftingAction->isChecked();
        viewContainer->setDebugMode(checked);
        if (checked) {
            debugDraftingAction->setText("退出调试");
        } else {
            debugDraftingAction->setText("调试Drafting");
        }
        });

    QObject::connect(polygonAction, &QAction::triggered, [&]() {
        polygonAction->setChecked(true);
        polylineAction->setChecked(false);
        view->setTool(Sketch2DView::Tool::Polygon);
        });

    // 暂时注释掉 Clear 按钮连接
    //QObject::connect(clearAction, &QAction::triggered, [&]() {
    //    view->clear();
    //
    //    while (polylinesItem->childCount() > 0) {
    //        delete polylinesItem->child(0);
    //    }
    //    while (polygonsItem->childCount() > 0) {
    //        delete polygonsItem->child(0);
    //    }
    //    });

    QObject::connect(importAction, &QAction::triggered, [&]() {
        QString filePath = QFileDialog::getOpenFileName(nullptr, "导入多边形", "", "Polygon Files (*.txt);;All Files (*)");
        if (filePath.isEmpty()) {
            return;
        }

        std::vector<tailor_visualization::Polygon> importedPolygons;
        bool success = tailor_visualization::PolygonIO::ImportFromFile(filePath, importedPolygons);

        if (!success || importedPolygons.empty()) {
            qDebug() << "导入失败或文件为空!";
            return;
        }

        // 获取当前的多边形，添加新的
        QVector<Sketch2DView::Polygon> currentPolygons = view->polygons();

        for (size_t i = 0; i < importedPolygons.size(); ++i) {
            const auto& polygon = importedPolygons[i];
            Sketch2DView::Polygon newPolygon;
            for (const auto& edge : polygon.edges) {
                Sketch2DView::PolygonVertex vertex;
                vertex.point = QPointF(edge.startPoint.x, edge.startPoint.y);
                vertex.bulge = edge.bulge;
                newPolygon.vertices.append(vertex);
            }

            // 添加到视图
            currentPolygons.append(newPolygon);
        }

        // 更新视图
        view->setPolygons(currentPolygons);

        // 同步更新其他三个视图
        viewContainer->synchronizeViews();

        // 更新模型树
        int startIndex = polygonsItem->childCount();
        for (size_t i = 0; i < importedPolygons.size(); ++i) {
            QString name = QString("多边形%1").arg(startIndex + (int)i + 1);
            auto* item = new QTreeWidgetItem(polygonsItem);
            item->setText(0, name);
            item->setData(0, Qt::UserRole, 1); // 1 = polygon
        }

        qDebug() << "成功导入" << importedPolygons.size() << "个多边形";
        });

    // 连接右下角布尔运算下拉框的信号
    QObject::connect(viewContainer, &FourViewContainer::booleanOperationTriggered, [viewContainer, booleanResultsItem](int operationIndex) {
        // 获取右下角视图（布尔运算结果视图）
        auto* bottomRightView = viewContainer->bottomRightView();

        tailor_visualization::BooleanOperation operation;
        switch (operationIndex) {
        case 0: // Union
            operation = tailor_visualization::BooleanOperation::Union;
            qDebug() << "执行并集运算";
            break;
        case 1: // Intersection
            operation = tailor_visualization::BooleanOperation::Intersection;
            qDebug() << "执行交集运算";
            break;
        case 2: // Difference
            operation = tailor_visualization::BooleanOperation::Difference;
            qDebug() << "执行差集运算";
            break;
        case 3: // XOR
            operation = tailor_visualization::BooleanOperation::XOR;
            qDebug() << "执行异或运算";
            break;
        default:
            return;
        }
        bottomRightView->executeBooleanOperation(operation);
        viewContainer->synchronizeViews();
        // 模型树更新由 dataChanged 信号驱动
        });

    QObject::connect(view, &Sketch2DView::polylineAdded, [polylinesItem](int index, const QString& name) {
        auto* item = new QTreeWidgetItem(polylinesItem);
        item->setText(0, name);
        item->setData(0, Qt::UserRole, 0); // 0 = polyline
        // 不再存储索引，使用在父节点中的位置
        // 模型树更新由 dataChanged 信号驱动
        });

    QObject::connect(view, &Sketch2DView::polygonAdded, [polygonsItem](int index, const QString& name) {
        auto* item = new QTreeWidgetItem(polygonsItem);
        item->setText(0, name);
        item->setData(0, Qt::UserRole, 1); // 1 = polygon
        // 不再存储索引，使用在父节点中的位置
        // 模型树更新由 dataChanged 信号驱动
        });

    // 处理 polygonRoleChanged 信号，更新模型树颜色
    QObject::connect(view, &Sketch2DView::polygonRoleChanged, [polygonsItem, polylinesItem, view](int index, bool isPolygon) {
        QTreeWidgetItem* parentItem = isPolygon ? polygonsItem : polylinesItem;
        if (index >= 0 && index < parentItem->childCount()) {
            QTreeWidgetItem* item = parentItem->child(index);
            // 获取当前角色状态
            Sketch2DView::BooleanRole role = view->polygonRole(index, isPolygon);
            // 根据角色设置颜色
            if (role == Sketch2DView::BooleanRole::Clip) {
                item->setForeground(0, QBrush(QColor(0, 0, 255))); // 蓝色
            } else if (role == Sketch2DView::BooleanRole::Subject) {
                item->setForeground(0, QBrush(QColor(255, 0, 0))); // 红色
            } else {
                item->setForeground(0, QBrush()); // 恢复默认颜色
            }
        }
        // 模型树更新由 dataChanged 信号驱动
        });

    // ==================== 输入模型树选择变化 ====================
    // ==================== 输入模型树选择变化 ====================
    QObject::connect(inputModelTree, &QTreeWidget::itemSelectionChanged, [inputModelTree, view]() {
        auto selectedItems = inputModelTree->selectedItems();

        // 清除主视图的选择
        view->clearSelection();

        if (selectedItems.isEmpty()) {
            return;
        }

        // 收集需要选中的类型和索引
        int selectedPolylineIndex = -1;
        int selectedPolygonIndex = -1;
        QSet<int> polylineIndices;
        QSet<int> polygonIndices;

        for (QTreeWidgetItem* item : selectedItems) {
            if (!item->parent()) {
                continue;
            }

            int type = item->data(0, Qt::UserRole).toInt();
            int index = item->parent()->indexOfChild(item);

            if (type == 0) { // polyline
                polylineIndices.insert(index);
                selectedPolylineIndex = index;
            } else if (type == 1) { // polygon
                polygonIndices.insert(index);
                selectedPolygonIndex = index;
            }
            // 注意：输入模型树不应该包含结果多边形（类型>=100）
        }

        // 处理主视图中的多边形/多段线选择
        for (int index : polylineIndices) {
            view->addSelectedPolyline(index);
        }
        for (int index : polygonIndices) {
            view->addSelectedPolygon(index);
        }

        // 如果只有一个选中项，设置为主选择
        if (polylineIndices.size() + polygonIndices.size() == 1) {
            if (selectedPolylineIndex >= 0) {
                view->setSelectedPolygon(selectedPolylineIndex, false);
            } else if (selectedPolygonIndex >= 0) {
                view->setSelectedPolygon(selectedPolygonIndex, true);
            }
        }
        });

    // ==================== 调试视图点击 drafting 线段 → 同步模型树选中 ====================
    QObject::connect(viewContainer->mainView(), &Sketch2DView::draftingSegmentSelectedInView,
        [viewContainer, draftingSegmentsItem, outputModelTree](int index) {
            // 在模型树中找到对应的 drafting 线段项并选中
            for (int i = 0; i < draftingSegmentsItem->childCount(); ++i) {
                auto* child = draftingSegmentsItem->child(i);
                int type = child->data(0, Qt::UserRole).toInt();
                if (type - 400 == index) {
                    outputModelTree->clearSelection();
                    child->setSelected(true);
                    outputModelTree->scrollToItem(child);
                    break;
                }
            }
        });

    // ==================== 输出模型树选择变化 ====================
    QObject::connect(outputModelTree, &QTreeWidget::itemSelectionChanged, [outputModelTree, viewContainer, draftingInfoPanel]() {
        auto selectedItems = outputModelTree->selectedItems();

        // 获取所有结果视图
        auto* bottomRightView = viewContainer->bottomRightView(); // 布尔运算结果视图
        auto* bottomLeftView = viewContainer->bottomLeftView();   // Clip Pattern 结果视图
        auto* topRightView = viewContainer->topRightView();       // Subject Pattern 结果视图

        // 清除所有结果视图的高亮
        bottomRightView->clearHighlightedResult();
        bottomLeftView->clearHighlightedResult();
        topRightView->clearHighlightedResult();

        if (selectedItems.isEmpty()) {
            draftingInfoPanel->clear();
            viewContainer->mainView()->clearHighlightedDraftingSegment();
            return;
        }

        // 收集结果多边形的选择
        QSet<int> booleanResultIndices;
        QSet<int> clipPatternResultIndices;
        QSet<int> subjectPatternResultIndices;

        for (QTreeWidgetItem* item : selectedItems) {
            if (!item->parent()) {
                continue;
            }

            int type = item->data(0, Qt::UserRole).toInt();
            int index = item->parent()->indexOfChild(item);

            if (type >= 100 && type < 200) { // 布尔运算结果 (100-199)
                int resultIndex = type - 100; // 转换为实际索引
                booleanResultIndices.insert(resultIndex);
            } else if (type >= 200 && type < 300) { // Clip Pattern 结果 (200-299)
                int resultIndex = type - 200; // 转换为实际索引
                clipPatternResultIndices.insert(resultIndex);
            } else if (type >= 400) { // Drafting 线段 (400+)
                // Drafting segment selected - will show detail info below
            }
            // 注意：输出模型树不应该包含普通多边形/多段线（类型0-1）
        }

        // 检查是否选中了 Drafting 线段，如果是则在信息面板中显示详细信息
        bool hasDraftingSelection = false;
        for (QTreeWidgetItem* item : selectedItems) {
            if (!item->parent()) continue;
            int type = item->data(0, Qt::UserRole).toInt();
            if (type >= 400) {
                int segIndex = type - 400;
                const auto& segments = viewContainer->draftingSegments();
                const DraftingSegmentInfo* seg = nullptr;
                for (const auto& s : segments) {
                    if (s.index == segIndex) {
                        seg = &s;
                        break;
                    }
                }
                if (seg) {
                    QString info;
                    info += QString("<h3>线段 #%1</h3>").arg(seg->index);
                    info += "<table>";
                    info += QString("<tr><td><b>所属集合:</b></td><td>%1</td></tr>").arg(seg->setName());
                    info += QString("<tr><td><b>起点坐标:</b></td><td>(%1, %2)</td></tr>").arg(seg->startX, 0, 'f', 3).arg(seg->startY, 0, 'f', 3);
                    info += QString("<tr><td><b>终点坐标:</b></td><td>(%1, %2)</td></tr>").arg(seg->endX, 0, 'f', 3).arg(seg->endY, 0, 'f', 3);
                    info += QString("<tr><td><b>起点顶点组:</b></td><td>%1</td></tr>").arg(seg->startPntGroup == static_cast<tailor::Handle>(-1) ? QString("npos") : QString::number(seg->startPntGroup));
                    info += QString("<tr><td><b>终点顶点组:</b></td><td>%1</td></tr>").arg(seg->endPntGroup == static_cast<tailor::Handle>(-1) ? QString("npos") : QString::number(seg->endPntGroup));
                    info += QString("<tr><td><b>是否反转:</b></td><td>%1</td></tr>").arg(seg->reversed ? "是" : "否");
                    info += QString("<tr><td><b>是否是圆弧:</b></td><td>%1</td></tr>").arg(seg->isArc() ? QString("是 (bulge=%2)").arg(seg->bulge, 0, 'f', 6) : "否");
                    info += QString("<tr><td><b>区域 windB (下方):</b></td><td>%1</td></tr>").arg(seg->windB);
                    info += QString("<tr><td><b>区域 windA (下方):</b></td><td>%1</td></tr>").arg(seg->windA);
                    info += QString("<tr><td><b>聚合边:</b></td><td>%1</td></tr>").arg(seg->isAggregated ? "是" : "否");
                    info += QString("<tr><td><b>windB 贡献:</b></td><td>%1</td></tr>").arg(seg->windBContribution);
                    info += QString("<tr><td><b>windA 贡献:</b></td><td>%1</td></tr>").arg(seg->windAContribution);
                    info += QString("<tr><td><b>end:</b></td><td>%1</td></tr>").arg(seg->end ? "true" : "false");
                    info += QString("<tr><td><b>discarded:</b></td><td>%1</td></tr>").arg(seg->discarded ? "true" : "false");
                    info += "</table>";

                    draftingInfoPanel->setHtml(info);

                    // 高亮调试视图中对应的线段
                    viewContainer->mainView()->setHighlightedDraftingSegment(segIndex);
                }
                hasDraftingSelection = true;
                break;
            }
        }

        // 当未选中 Drafting 线段时清空信息面板和高亮
        if (!hasDraftingSelection) {
            draftingInfoPanel->clear();
            viewContainer->mainView()->clearHighlightedDraftingSegment();
        }

        // 处理结果多边形的高亮
        // 布尔运算结果高亮（支持多个选择）
        if (!booleanResultIndices.isEmpty()) {
            // 高亮第一个选中的结果
            int firstIndex = *booleanResultIndices.begin();
            bottomRightView->setHighlightedResult(firstIndex);
        } else {
            // 如果没有选中布尔运算结果，清除高亮
            bottomRightView->clearHighlightedResult();
        }

        // Clip Pattern 结果高亮（支持多个选择）
        if (!clipPatternResultIndices.isEmpty()) {
            // 高亮第一个选中的结果
            int firstIndex = *clipPatternResultIndices.begin();
            bottomLeftView->setHighlightedResult(firstIndex);
        } else {
            // 如果没有选中 Clip Pattern 结果，清除高亮
            bottomLeftView->clearHighlightedResult();
        }

        // Subject Pattern 结果高亮（支持多个选择）
        if (!subjectPatternResultIndices.isEmpty()) {
            // 高亮第一个选中的结果
            int firstIndex = *subjectPatternResultIndices.begin();
            topRightView->setHighlightedResult(firstIndex);
        } else {
            // 如果没有选中 Subject Pattern 结果，清除高亮
            topRightView->clearHighlightedResult();
        }
        });

    // ==================== 输入模型树右键菜单 ====================
    inputModelTree->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(inputModelTree, &QTreeWidget::customContextMenuRequested, [inputModelTree, view, viewContainer, polylinesItem, polygonsItem](const QPoint& pos) {
        auto selectedItems = inputModelTree->selectedItems();
        if (selectedItems.isEmpty()) {
            return;
        }

        // 检查是否选中了有效的曲线对象（不是父节点）
        bool hasValidSelection = false;
        for (QTreeWidgetItem* item : selectedItems) {
            if (item->parent()) {
                hasValidSelection = true;
                // 输入模型树不应该包含结果多边形（UserRole >= 100）
                int type = item->data(0, Qt::UserRole).toInt();
                if (type >= 100) {
                    qDebug() << "警告：输入模型树中包含结果多边形，类型:" << type;
                }
            }
        }

        if (!hasValidSelection) {
            return;
        }

        // 调试信息
        qDebug() << "输入模型树右键菜单: hasValidSelection =" << hasValidSelection
            << ", 选中项数量:" << selectedItems.size();

        if (!hasValidSelection) {
            return;
        }

        // 创建右键菜单
        QMenu menu;
        QAction* deleteAction = nullptr;
        QAction* exportAction = menu.addAction("导出");

        // 添加 Clip 和 Subject 选项（允许多选）
        QAction* addToClipAction = nullptr;
        QAction* addToSubjectAction = nullptr;
        QAction* removeFromSetsAction = nullptr;
        QAction* changeEdgeColorAction = nullptr;

        // 输入模型树没有结果多边形，可以直接添加删除选项
        deleteAction = menu.addAction("删除");

        // 检查是否有选中的多边形
        bool hasSelectedPolygons = false;
        for (QTreeWidgetItem* item : selectedItems) {
            if (item->parent()) {
                int type = item->data(0, Qt::UserRole).toInt();
                if (type == 1) { // polygon
                    hasSelectedPolygons = true;
                    break;
                }
            }
        }

        // 输入模型树只有普通多边形，显示修改选项
        if (hasValidSelection) {
            menu.addSeparator();
            addToClipAction = menu.addAction("加入 Clip");
            addToSubjectAction = menu.addAction("加入 Subject");
            removeFromSetsAction = menu.addAction("移除集合");
            // 更改边界颜色支持多选（只要有选中多边形就显示）
            if (hasSelectedPolygons) {
                changeEdgeColorAction = menu.addAction("更改边界颜色...");
                qDebug() << "创建了changeEdgeColorAction菜单项";
            }
        } else {
            qDebug() << "不显示修改选项: hasValidSelection =" << hasValidSelection;
        }

        QAction* result = menu.exec(inputModelTree->mapToGlobal(pos));

        // 调试：输出用户选择的菜单项
        if (result) {
            qDebug() << "用户选择了菜单项:" << result->text()
                << ", changeEdgeColorAction指针:" << changeEdgeColorAction
                << ", result == changeEdgeColorAction:" << (result == changeEdgeColorAction);
        } else {
            qDebug() << "用户取消了菜单选择";
        }

        // 处理更改边界颜色操作
        // 只有当result不为空且等于changeEdgeColorAction时才处理
        if (result && result == changeEdgeColorAction) {
            qDebug() << "进入更改边界颜色处理逻辑";

            // 输入模型树不应该包含结果多边形，所以不需要这个安全检查
            // 但仍然需要检查是否有选中的有效多边形
            bool hasPolygonsToColor = false;
            for (QTreeWidgetItem* item : selectedItems) {
                if (item->parent()) {
                    int type = item->data(0, Qt::UserRole).toInt();
                    if (type == 1) { // polygon
                        hasPolygonsToColor = true;
                        break;
                    }
                }
            }

            if (!hasPolygonsToColor) {
                qDebug() << "没有选中的多边形可以更改颜色";
                return;
            }

            qDebug() << "view指针是否有效:" << (view != nullptr);
            if (!view) {
                qDebug() << "错误: view指针为空!";
                return;
            }

            // 获取第一个选中多边形的颜色作为默认值，并收集所有选中多边形的索引
            QColor currentColor = QColor(255, 255, 255);
            QSet<int> polygonIndices;

            // 安全检查：确保view指针有效
            if (!view) {
                qDebug() << "致命错误：view指针无效，无法更改颜色";
                return;
            }

            for (QTreeWidgetItem* item : selectedItems) {
                if (!item->parent()) continue;
                int type = item->data(0, Qt::UserRole).toInt();
                if (type == 1) { // polygon
                    int index = item->parent()->indexOfChild(item);
                    // 安全检查：确保索引有效
                    if (index < 0) {
                        continue;
                    }

                    const auto& polygons = view->polygons();
                    // 安全检查：确保索引在范围内
                    if (index >= polygons.size()) {
                        continue;
                    }

                    if (!polygons[index].vertices.isEmpty()) {
                        currentColor = polygons[index].vertices[0].edgeColor;
                    }
                    polygonIndices.insert(index);
                }
            }
            // 打开颜色选择对话框
            QColor newColor = QColorDialog::getColor(currentColor, inputModelTree, "选择边界颜色");
            if (newColor.isValid()) {
                // 批量设置所有选中多边形的颜色
                view->setPolygonEdgeColorBatch(polygonIndices, newColor, true);
                // 立即同步其他视图
                QCoreApplication::processEvents();
                viewContainer->synchronizeViews();
            }
            return;
        }

        // 处理 Clip/Subject 操作
        if (result && (result == addToClipAction || result == addToSubjectAction || result == removeFromSetsAction)) {
            // 批量处理所有选中的有效项
            for (QTreeWidgetItem* item : selectedItems) {
                if (!item->parent()) {
                    continue; // 跳过顶层节点
                }

                int type = item->data(0, Qt::UserRole).toInt();
                int index = item->parent()->indexOfChild(item);
                bool isPolygon = (type == 1); // 1 = polygon

                if (result == addToClipAction) {
                    view->setPolygonRole(index, Sketch2DView::BooleanRole::Clip, isPolygon);
                    item->setForeground(0, QBrush(QColor(0, 0, 255))); // 蓝色
                } else if (result == addToSubjectAction) {
                    view->setPolygonRole(index, Sketch2DView::BooleanRole::Subject, isPolygon);
                    item->setForeground(0, QBrush(QColor(255, 0, 0))); // 红色
                } else if (result == removeFromSetsAction) {
                    view->setPolygonRole(index, Sketch2DView::BooleanRole::None, isPolygon);
                    item->setForeground(0, QBrush()); // 恢复默认颜色
                }
            }
            // 立即同步其他视图
            viewContainer->synchronizeViews();
            return;
        }

        // 处理删除操作
        if (result == deleteAction) {
            // 检查是否包含结果多边形（不能删除结果多边形）
            bool containsResultPolygons = false;
            for (QTreeWidgetItem* item : selectedItems) {
                if (item->parent()) {
                    int type = item->data(0, Qt::UserRole).toInt();
                    if (type >= 100) { // 100+ 表示结果多边形
                        containsResultPolygons = true;
                        break;
                    }
                }
            }

            if (containsResultPolygons) {
                QMessageBox::warning(inputModelTree, "警告", "不能删除结果多边形！");
                return;
            }

            // 收集需要删除的索引（使用 QSet）
            QSet<int> polylineIndicesToDelete;
            QSet<int> polygonIndicesToDelete;

            for (QTreeWidgetItem* item : selectedItems) {
                if (item->parent()) {
                    int type = item->data(0, Qt::UserRole).toInt();
                    // 使用项在父节点中的位置作为索引
                    int index = item->parent()->indexOfChild(item);

                    if (type == 0) { // polyline
                        polylineIndicesToDelete.insert(index);
                    } else if (type == 1) { // polygon
                        polygonIndicesToDelete.insert(index);
                    }
                }
            }

            // 删除视图中的多段线
            view->deletePolylines(polylineIndicesToDelete);

            // 删除视图中的多边形
            view->deletePolygons(polygonIndicesToDelete);

            // 删除模型树中的项（先收集项指针，避免迭代器失效）
            QList<QTreeWidgetItem*> itemsToDelete;
            for (QTreeWidgetItem* item : selectedItems) {
                if (item->parent()) {
                    itemsToDelete.append(item);
                }
            }

            // 删除模型树中的项
            for (QTreeWidgetItem* item : itemsToDelete) {
                delete item;
            }
            // 立即同步其他视图
            viewContainer->synchronizeViews();
        } else if (result == exportAction) {
            // 导出选中的多边形到文件
            QString filePath = QFileDialog::getSaveFileName(nullptr, "导出多边形", "", "Polygon Files (*.txt);;All Files (*)");
            if (filePath.isEmpty()) {
                return;
            }

            // 调试：输出选中的项信息
            qDebug() << "导出操作，选中项数量:" << selectedItems.size();
            for (int i = 0; i < selectedItems.size(); ++i) {
                QTreeWidgetItem* item = selectedItems[i];
                if (item->parent()) {
                    int type = item->data(0, Qt::UserRole).toInt();
                    qDebug() << "  项" << i << "类型:" << type << "文本:" << item->text(0);
                }
            }

            // 收集需要导出的多边形
            std::vector<tailor_visualization::Polygon> polygonsToExport;

            for (QTreeWidgetItem* item : selectedItems) {
                if (!item->parent()) {
                    continue;
                }

                int type = item->data(0, Qt::UserRole).toInt();
                int index = item->parent()->indexOfChild(item);

                tailor_visualization::Polygon polygon;

                if (type == 0) { // polyline - 导出为封闭形式 (ab, bc, cd, dc, cb, ba)
                    const auto& polylines = view->polylines();
                    if (index >= 0 && index < polylines.size()) {
                        const auto& polyline = polylines[index];
                        int n = polyline.vertices.size();
                        // 正向边: v0->v1, v1->v2, ..., v(n-2)->v(n-1)
                        for (int j = 0; j < n - 1; ++j) {
                            const auto& vertex = polyline.vertices[j];
                            const auto& nextVertex = polyline.vertices[j + 1];
                            tailor_visualization::PolygonEdge edge;
                            edge.startPoint.x = vertex.point.x();
                            edge.startPoint.y = vertex.point.y();
                            edge.endPoint.x = nextVertex.point.x();
                            edge.endPoint.y = nextVertex.point.y();
                            edge.bulge = vertex.bulge;
                            polygon.edges.push_back(edge);
                        }
                        // 反向边: v(n-1)->v(n-2), v(n-2)->v(n-3), ..., v1->v0 (凸度取反)
                        for (int j = n - 1; j > 0; --j) {
                            const auto& vertex = polyline.vertices[j];
                            const auto& prevVertex = polyline.vertices[j - 1];
                            tailor_visualization::PolygonEdge edge;
                            edge.startPoint.x = vertex.point.x();
                            edge.startPoint.y = vertex.point.y();
                            edge.endPoint.x = prevVertex.point.x();
                            edge.endPoint.y = prevVertex.point.y();
                            edge.bulge = -prevVertex.bulge;  // 反向边凸度取反（使用前一顶点的凸度）
                            polygon.edges.push_back(edge);
                        }
                    }
                } else if (type == 1) { // polygon
                    const auto& polygons = view->polygons();
                    if (index >= 0 && index < polygons.size()) {
                        const auto& poly = polygons[index];
                        for (int j = 0; j < poly.vertices.size(); ++j) {
                            const auto& vertex = poly.vertices[j];
                            const auto& nextVertex = poly.vertices[(j + 1) % poly.vertices.size()];

                            tailor_visualization::PolygonEdge edge;
                            edge.startPoint.x = vertex.point.x();
                            edge.startPoint.y = vertex.point.y();
                            edge.endPoint.x = nextVertex.point.x();
                            edge.endPoint.y = nextVertex.point.y();
                            edge.bulge = vertex.bulge;
                            polygon.edges.push_back(edge);
                        }
                    }
                } else if (type >= 100 && type < 200) { // 布尔运算结果 (100-199)
                    int resultIndex = type - 100;
                    if (viewContainer->bottomRightView()) {
                        const auto& booleanResults = viewContainer->bottomRightView()->booleanResults();
                        if (resultIndex >= 0 && resultIndex < booleanResults.size()) {
                            const auto& resultPoly = booleanResults[resultIndex];
                            for (int j = 0; j < resultPoly.vertices.size(); ++j) {
                                const auto& vertex = resultPoly.vertices[j];
                                const auto& nextVertex = resultPoly.vertices[(j + 1) % resultPoly.vertices.size()];

                                tailor_visualization::PolygonEdge edge;
                                edge.startPoint.x = vertex.point.x();
                                edge.startPoint.y = vertex.point.y();
                                edge.endPoint.x = nextVertex.point.x();
                                edge.endPoint.y = nextVertex.point.y();
                                edge.bulge = 0.0; // 结果多边形通常没有凸度
                                polygon.edges.push_back(edge);
                            }
                        }
                    }
                } else if (type >= 200 && type < 300) { // Clip Pattern 结果 (200-299)
                    int resultIndex = type - 200;
                    if (viewContainer->bottomLeftView()) {
                        const auto& clipPatternResults = viewContainer->bottomLeftView()->clipPatternResults();
                        if (resultIndex >= 0 && resultIndex < clipPatternResults.size()) {
                            const auto& resultPoly = clipPatternResults[resultIndex];
                            for (int j = 0; j < resultPoly.vertices.size(); ++j) {
                                const auto& vertex = resultPoly.vertices[j];
                                const auto& nextVertex = resultPoly.vertices[(j + 1) % resultPoly.vertices.size()];

                                tailor_visualization::PolygonEdge edge;
                                edge.startPoint.x = vertex.point.x();
                                edge.startPoint.y = vertex.point.y();
                                edge.endPoint.x = nextVertex.point.x();
                                edge.endPoint.y = nextVertex.point.y();
                                edge.bulge = 0.0; // 结果多边形通常没有凸度
                                polygon.edges.push_back(edge);
                            }
                        }
                    }
                } else if (type >= 300) { // Subject Pattern 结果 (300+)
                    int resultIndex = type - 300;
                    if (viewContainer->topRightView()) {
                        const auto& subjectPatternResults = viewContainer->topRightView()->subjectPatternResults();
                        if (resultIndex >= 0 && resultIndex < subjectPatternResults.size()) {
                            const auto& resultPoly = subjectPatternResults[resultIndex];
                            for (int j = 0; j < resultPoly.vertices.size(); ++j) {
                                const auto& vertex = resultPoly.vertices[j];
                                const auto& nextVertex = resultPoly.vertices[(j + 1) % resultPoly.vertices.size()];

                                tailor_visualization::PolygonEdge edge;
                                edge.startPoint.x = vertex.point.x();
                                edge.startPoint.y = vertex.point.y();
                                edge.endPoint.x = nextVertex.point.x();
                                edge.endPoint.y = nextVertex.point.y();
                                edge.bulge = 0.0; // 结果多边形通常没有凸度
                                polygon.edges.push_back(edge);
                            }
                        }
                    }
                }

                if (!polygon.edges.empty()) {
                    polygonsToExport.push_back(polygon);
                }
            }

            // 导出到文件
            if (!polygonsToExport.empty()) {
                bool success = tailor_visualization::PolygonIO::ExportToFile(filePath.toStdString(), polygonsToExport);
                if (!success) {
                    qDebug() << "导出失败!";
                } else {
                    qDebug() << "导出成功:" << filePath;
                }
            }
        }
        });

    // ==================== 输出模型树右键菜单 ====================
    outputModelTree->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(outputModelTree, &QTreeWidget::customContextMenuRequested, [outputModelTree, viewContainer](const QPoint& pos) {
        auto selectedItems = outputModelTree->selectedItems();
        if (selectedItems.isEmpty()) {
            return;
        }

        // 检查是否选中了有效的曲线对象（不是父节点）
        bool hasValidSelection = false;
        for (QTreeWidgetItem* item : selectedItems) {
            if (item->parent()) {
                hasValidSelection = true;
                break;
            }
        }

        if (!hasValidSelection) {
            return;
        }

        // 输出模型树右键菜单只有"导出"选项
        QMenu menu;
        QAction* exportAction = menu.addAction("导出");

        QAction* result = menu.exec(outputModelTree->mapToGlobal(pos));

        if (result == exportAction) {
            // 导出选中的结果多边形到文件
            QString filePath = QFileDialog::getSaveFileName(nullptr, "导出结果多边形", "", "Polygon Files (*.txt);;All Files (*)");
            if (filePath.isEmpty()) {
                return;
            }

            // 收集需要导出的结果多边形
            std::vector<tailor_visualization::Polygon> polygonsToExport;

            for (QTreeWidgetItem* item : selectedItems) {
                if (!item->parent()) {
                    continue;
                }

                int type = item->data(0, Qt::UserRole).toInt();
                int index = item->parent()->indexOfChild(item);

                tailor_visualization::Polygon polygon;

                if (type >= 100 && type < 200) { // 布尔运算结果 (100-199)
                    int resultIndex = type - 100;
                    if (viewContainer->bottomRightView()) {
                        const auto& booleanResults = viewContainer->bottomRightView()->booleanResults();
                        if (resultIndex >= 0 && resultIndex < booleanResults.size()) {
                            const auto& resultPoly = booleanResults[resultIndex];
                            for (int j = 0; j < resultPoly.vertices.size(); ++j) {
                                const auto& vertex = resultPoly.vertices[j];
                                const auto& nextVertex = resultPoly.vertices[(j + 1) % resultPoly.vertices.size()];

                                tailor_visualization::PolygonEdge edge;
                                edge.startPoint.x = vertex.point.x();
                                edge.startPoint.y = vertex.point.y();
                                edge.endPoint.x = nextVertex.point.x();
                                edge.endPoint.y = nextVertex.point.y();
                                edge.bulge = 0.0; // 结果多边形通常没有凸度
                                polygon.edges.push_back(edge);
                            }
                        }
                    }
                } else if (type >= 200 && type < 300) { // Clip Pattern 结果 (200-299)
                    int resultIndex = type - 200;
                    if (viewContainer->bottomLeftView()) {
                        const auto& clipPatternResults = viewContainer->bottomLeftView()->clipPatternResults();
                        if (resultIndex >= 0 && resultIndex < clipPatternResults.size()) {
                            const auto& resultPoly = clipPatternResults[resultIndex];
                            for (int j = 0; j < resultPoly.vertices.size(); ++j) {
                                const auto& vertex = resultPoly.vertices[j];
                                const auto& nextVertex = resultPoly.vertices[(j + 1) % resultPoly.vertices.size()];

                                tailor_visualization::PolygonEdge edge;
                                edge.startPoint.x = vertex.point.x();
                                edge.startPoint.y = vertex.point.y();
                                edge.endPoint.x = nextVertex.point.x();
                                edge.endPoint.y = nextVertex.point.y();
                                edge.bulge = 0.0; // 结果多边形通常没有凸度
                                polygon.edges.push_back(edge);
                            }
                        }
                    }
                } else if (type >= 300) { // Subject Pattern 结果 (300+)
                    int resultIndex = type - 300;
                    if (viewContainer->topRightView()) {
                        const auto& subjectPatternResults = viewContainer->topRightView()->subjectPatternResults();
                        if (resultIndex >= 0 && resultIndex < subjectPatternResults.size()) {
                            const auto& resultPoly = subjectPatternResults[resultIndex];
                            for (int j = 0; j < resultPoly.vertices.size(); ++j) {
                                const auto& vertex = resultPoly.vertices[j];
                                const auto& nextVertex = resultPoly.vertices[(j + 1) % resultPoly.vertices.size()];

                                tailor_visualization::PolygonEdge edge;
                                edge.startPoint.x = vertex.point.x();
                                edge.startPoint.y = vertex.point.y();
                                edge.endPoint.x = nextVertex.point.x();
                                edge.endPoint.y = nextVertex.point.y();
                                edge.bulge = 0.0; // 结果多边形通常没有凸度
                                polygon.edges.push_back(edge);
                            }
                        }
                    }
                }

                if (!polygon.edges.empty()) {
                    polygonsToExport.push_back(polygon);
                }
            }

            // 导出到文件
            if (!polygonsToExport.empty()) {
                bool success = tailor_visualization::PolygonIO::ExportToFile(filePath.toStdString(), polygonsToExport);
                if (!success) {
                    qDebug() << "导出失败!";
                } else {
                    qDebug() << "导出成功:" << filePath;
                }
            }
        }
        });

    // 改为事件驱动：FourViewContainer 数据变化时更新模型树，不再使用1秒轮询定时器
    QObject::connect(viewContainer, &FourViewContainer::dataChanged, [viewContainer, booleanResultsItem, clipPatternResultsItem, subjectPatternResultsItem, draftingSegmentsItem, outputModelTree]() {
        // 保存当前滚动位置，避免重建时跳动
        int scrollPos = outputModelTree->verticalScrollBar()->value();
        updateAllResultsTrees(viewContainer,
            booleanResultsItem,
            clipPatternResultsItem,
            subjectPatternResultsItem,
            draftingSegmentsItem);
        outputModelTree->verticalScrollBar()->setValue(scrollPos);
        });

    // 初始同步：触发数据计算并通过 dataChanged 信号更新模型树
    viewContainer->synchronizeViews();

    window.show();

    return app.exec();
}
