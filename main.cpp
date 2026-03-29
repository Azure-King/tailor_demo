#include <QApplication>
#include <QMainWindow>
#include <QSplitter>
#include <QTreeWidget>
#include <QTreeWidgetItem>
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

#include "Sketch2DView.h"
#include "FourViewContainer.h"
#include "PolygonIO.h"
#include "BooleanOperations.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    QMainWindow window;
    window.resize(1200, 700);

    auto* splitter = new QSplitter(Qt::Horizontal, &window);

    auto* modelTree = new QTreeWidget(&window);
    modelTree->setHeaderLabel("模型树");
    modelTree->setMinimumWidth(150);
    modelTree->setMaximumWidth(300);
    modelTree->setSelectionMode(QAbstractItemView::ExtendedSelection);

    auto* polylinesItem = new QTreeWidgetItem(modelTree);
    polylinesItem->setText(0, "多段线");
    polylinesItem->setExpanded(true);


    auto* polygonsItem = new QTreeWidgetItem(modelTree);
    polygonsItem->setText(0, "多边形");
    polygonsItem->setExpanded(true);

    // Use FourViewContainer instead of single Sketch2DView
    auto* viewContainer = new FourViewContainer(&window);
    viewContainer->setMinimumSize(800, 300);

    auto* view = viewContainer->mainView(); // Main view for tool selection

    splitter->addWidget(modelTree);
    splitter->addWidget(viewContainer);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({200, 1000});

    window.setCentralWidget(splitter);

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

    // 设置多边形为默认模式
    polygonAction->setChecked(true);
    view->setTool(Sketch2DView::Tool::Polygon);

    QObject::connect(polylineAction, &QAction::triggered, [&]() {
        polylineAction->setChecked(true);
        polygonAction->setChecked(false);
        view->setTool(Sketch2DView::Tool::Polyline);
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
    QObject::connect(viewContainer, &FourViewContainer::booleanOperationTriggered, [=](int operationIndex) {
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
        view->executeBooleanOperation(operation);
        viewContainer->synchronizeViews();
        });

    QObject::connect(view, &Sketch2DView::polylineAdded, [=](int index, const QString& name) {
        auto* item = new QTreeWidgetItem(polylinesItem);
        item->setText(0, name);
        item->setData(0, Qt::UserRole, 0); // 0 = polyline
        // 不再存储索引，使用在父节点中的位置
        });

    QObject::connect(view, &Sketch2DView::polygonAdded, [=](int index, const QString& name) {
        auto* item = new QTreeWidgetItem(polygonsItem);
        item->setText(0, name);
        item->setData(0, Qt::UserRole, 1); // 1 = polygon
        // 不再存储索引，使用在父节点中的位置
        });

    // 处理 polygonRoleChanged 信号，更新模型树颜色
    QObject::connect(view, &Sketch2DView::polygonRoleChanged, [=](int index, bool isPolygon) {
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
        });

    QObject::connect(modelTree, &QTreeWidget::itemSelectionChanged, [=]() {
        auto selectedItems = modelTree->selectedItems();
        if (selectedItems.isEmpty()) {
            view->clearSelection();
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
        }

        // 清除视图中的选择
        view->clearSelection();

        // 添加到对应的选中集合
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

    // 右键菜单功能
    modelTree->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(modelTree, &QTreeWidget::customContextMenuRequested, [=](const QPoint& pos) {
        auto selectedItems = modelTree->selectedItems();
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

        // 创建右键菜单
        QMenu menu;
        QAction* deleteAction = menu.addAction("删除");
        QAction* exportAction = menu.addAction("导出");

        // 添加 Clip 和 Subject 选项（允许多选）
        QAction* addToClipAction = nullptr;
        QAction* addToSubjectAction = nullptr;
        QAction* removeFromSetsAction = nullptr;
        QAction* changeEdgeColorAction = nullptr;

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

        if (hasValidSelection) {
            menu.addSeparator();
            addToClipAction = menu.addAction("加入 Clip");
            addToSubjectAction = menu.addAction("加入 Subject");
            removeFromSetsAction = menu.addAction("移除集合");
            // 更改边界颜色支持多选（只要有选中多边形就显示）
            if (hasSelectedPolygons) {
                changeEdgeColorAction = menu.addAction("更改边界颜色...");
            }
        }

        QAction* result = menu.exec(modelTree->mapToGlobal(pos));

        // 处理更改边界颜色操作
        if (result == changeEdgeColorAction) {
            // 获取第一个选中多边形的颜色作为默认值，并收集所有选中多边形的索引
            QColor currentColor = QColor(255, 255, 255);
            QSet<int> polygonIndices;
            for (QTreeWidgetItem* item : selectedItems) {
                if (!item->parent()) continue;
                int type = item->data(0, Qt::UserRole).toInt();
                if (type == 1) { // polygon
                    int index = item->parent()->indexOfChild(item);
                    const auto& polygons = view->polygons();
                    if (index >= 0 && index < polygons.size()) {
                        if (!polygons[index].vertices.isEmpty()) {
                            currentColor = polygons[index].vertices[0].edgeColor;
                        }
                        polygonIndices.insert(index);
                    }
                }
            }
            // 打开颜色选择对话框
            QColor newColor = QColorDialog::getColor(currentColor, modelTree, "选择边界颜色");
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

    window.show();

    return app.exec();
}
