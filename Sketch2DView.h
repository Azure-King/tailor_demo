#pragma once

#include <QWidget>
#include <QPointF>
#include <QVector>
#include <QMenu>
#include <QSet>
#include <QColor>
#include "BooleanOperations.h"

// Type alias for IConnectType Drafting parameter
using ArcPoint = tailor::Point<double>;
using ArcSegment = tailor::ArcSegment<ArcPoint, double, QRgba64>;
using ArcTailor2 = tailor::Tailor<ArcSegment, tailor::ArcAnalysis<ArcSegment, tailor::ArcSegmentAnalyserCore<ArcSegment, tailor::PrecisionCore<10>>>>;
using ConnectTypeDrafting = ArcTailor2::PatternDrafting;

// Forward declarations
namespace tailor_visualization {
    enum class BooleanOperation;
}

class Sketch2DView : public QWidget {
    Q_OBJECT

public:
    enum class Tool {
        Polyline,
        Polygon
    };

    enum class ViewMode {
        Normal,       // 正常视图，显示所有对象
        ClipFocus,    // 侧重显示 Clip，Clip 填充蓝色，Subject 只显示红色边框
        SubjectFocus, // 侧重显示 Subject，Subject 填充红色，Clip 只显示蓝色边框
        BooleanResult // 显示布尔运算结果
    };

    // Public struct definitions (needed for public methods)
    struct PolygonVertex {
        QPointF point;
        qreal bulge = 0.0;
        QColor edgeColor = QColor(255, 255, 255);  // 边界颜色，默认白色
    };

    struct Polygon {
        QVector<PolygonVertex> vertices;
    };

    // Polyline is similar to Polygon but vertices are not connected (open shape)
    using Polyline = Polygon;

    explicit Sketch2DView(QWidget* parent = nullptr);

    void setTool(Tool tool);
    Tool tool() const { return m_tool; }

    void setViewMode(ViewMode mode) { m_viewMode = mode; update(); }
    ViewMode viewMode() const { return m_viewMode; }

    void setReadOnly(bool readOnly);
    bool isReadOnly() const { return m_readOnly; }

    void clear();

    void setSelectedPolygon(int index, bool isPolygon = true);

    int selectedPolygonIndex() const { return m_selectedPolygonIndex; }

    void clearSelection();

    // 多选支持
    void addSelectedPolygon(int index);
    void removeSelectedPolygon(int index);
    void addSelectedPolyline(int index);
    void removeSelectedPolyline(int index);

    const QSet<int>& selectedPolygons() const { return m_selectedPolygons; }
    const QSet<int>& selectedPolylines() const { return m_selectedPolylines; }
    
    // 结果多边形高亮支持
    void setHighlightedResult(int index);
    void clearHighlightedResult();
    int highlightedResultIndex() const { return m_highlightedResultIndex; }

    // Data access for view synchronization
    const QVector<Polyline>& polylines() const { return m_polylines; }
    const QVector<Polygon>& polygons() const { return m_polygons; }

    void setPolylines(const QVector<Polyline>& polylines);
    void setPolygons(const QVector<Polygon>& polygons);

    // 设置多边形边界颜色
    void setPolygonEdgeColor(int index, const QColor& color, bool isPolygon = true);
    void setPolygonEdgeColorBatch(const QSet<int>& indices, const QColor& color, bool isPolygon);

    // 删除指定的多段线
    void deletePolylines(const QSet<int>& indices);

    // 删除指定的多边形
    void deletePolygons(const QSet<int>& indices);

    // Clip 和 Subject 集合管理
    enum class BooleanRole {
        None,
        Clip,
        Subject
    };

    void setPolygonRole(int index, BooleanRole role, bool isPolygon = true);
    void setPolygonRoleBatch(const QSet<int>& indices, BooleanRole role, bool isPolygon);
    BooleanRole polygonRole(int index, bool isPolygon = true) const;

    const QSet<int>& clipPolygons() const { return m_clipPolygons; }
    const QSet<int>& clipPolylines() const { return m_clipPolylines; }
    const QSet<int>& subjectPolygons() const { return m_subjectPolygons; }
    const QSet<int>& subjectPolylines() const { return m_subjectPolylines; }

    // 设置 Clip/Subject 集合
    void setClipPolygons(const QSet<int>& indices);
    void setClipPolylines(const QSet<int>& indices);
    void setSubjectPolygons(const QSet<int>& indices);
    void setSubjectPolylines(const QSet<int>& indices);

    // 布尔运算结果管理
    struct BooleanResultPolygon {
        QVector<PolygonVertex> vertices;
        QVector<QColor> edgeColors;  // 每条边的颜色
        QColor color;
        bool isHole = false;  // 标识是否为内环（洞）
    };
    void setBooleanResults(const QVector<BooleanResultPolygon>& results);
    const QVector<BooleanResultPolygon>& booleanResults() const { return m_booleanResults; }
    void clearBooleanResults() { m_booleanResults.clear(); update(); }

    // Clip 和 Subject 原始多边形结果（使用 OnlyClipPattern 和 OnlySubjectPattern 处理）
    void setClipPatternResults(const QVector<BooleanResultPolygon>& results);
    const QVector<BooleanResultPolygon>& clipPatternResults() const { return m_clipPatternResults; }
    void setSubjectPatternResults(const QVector<BooleanResultPolygon>& results);
    const QVector<BooleanResultPolygon>& subjectPatternResults() const { return m_subjectPatternResults; }

    // 执行布尔运算
    void executeBooleanOperation(tailor_visualization::BooleanOperation operation, int fillType = 1); // 1: EvenOdd as default
    void executeBooleanOperation(
        tailor_visualization::BooleanOperation operation,
        const tailor_visualization::IFillType* clipFillType,
        const tailor_visualization::IFillType* subjectFillType);
    void executeBooleanOperation(
        tailor_visualization::BooleanOperation operation,
        const tailor_visualization::IFillType* clipFillType,
        const tailor_visualization::IFillType* subjectFillType,
        const tailor_visualization::IConnectType<ConnectTypeDrafting>* connectType);

    // 执行 Clip 和 Subject Pattern 计算
    void executeClipPattern(const tailor_visualization::IFillType* fillType = nullptr);
    void executeSubjectPattern(const tailor_visualization::IFillType* fillType = nullptr);

    // 执行 Clip 和 Subject Pattern 计算（支持 ConnectType）
    void executeClipPattern(
        const tailor_visualization::IFillType* fillType,
        const tailor_visualization::IConnectType<ConnectTypeDrafting>* connectType);
    void executeSubjectPattern(
        const tailor_visualization::IFillType* fillType,
        const tailor_visualization::IConnectType<ConnectTypeDrafting>* connectType);

    // Viewport state
    qreal scale() const { return m_scale; }
    QPointF offset() const { return m_offset; }
    void setScale(qreal scale);
    void setOffset(const QPointF& offset);

    // 导出多边形到文件用于调试
    void debugExportPolygons(const QString& filename, const QSet<int>& indices, const QString& setName) const;
protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    struct Edge {
        int polygonIndex;
        int vertexIndex1;
        int vertexIndex2;
        bool isPolygon;
    };

    // Internal utility struct for arc computation
    struct ArcSegment {
        QPointF center;
        qreal radius = 0.0;
        qreal startAngleDeg = 0.0;
        qreal spanAngleDeg = 0.0;
    };

    static qreal degFromRad(qreal rad);
    static qreal radFromDeg(qreal deg);

    static qreal angleDegAt(const QPointF& center, const QPointF& p);

    // 创建多边形的 QPainterPath
    QPainterPath createPolygonPath(const Polygon& poly) const;
    QPainterPath createResultPolygonPath(const BooleanResultPolygon& poly) const;

    static qreal normalizedSpanDeg(qreal startDeg, qreal endDeg);

    // Bulge utility functions
    static qreal bulgeFromArc(const QPointF& p1, const QPointF& p2, const QPointF& throughPoint);
    static QPointF arcPointFromBulge(const QPointF& p1, const QPointF& p2, qreal bulge);
    static ArcSegment arcSegmentFromBulge(const QPointF& p1, const QPointF& p2, qreal bulge);
    static qreal splitArcBulge(qreal bulge);

    QPointF snapToPixelCenter(const QPointF& p) const;
    QPointF getEdgeMidpoint(const QPointF& p1, const QPointF& p2) const;
    bool isNearPoint(const QPointF& target, const QPointF& pos, qreal threshold = 8.0) const;
    bool isNearEdgeMidpoint(const QPointF& p1, const QPointF& p2, const QPointF& pos, qreal threshold = 8.0) const;
    Edge findNearbyEdgeMidpoint(const QPointF& pos, qreal threshold = 8.0) const;
    Edge findNearbyVertex(const QPointF& pos, qreal threshold = 8.0) const;

    QPointF worldToScreen(const QPointF& worldPos) const;
    QPointF screenToWorld(const QPointF& screenPos) const;

    QRectF getWorldBounds() const;

    Tool m_tool = Tool::Polyline;
    ViewMode m_viewMode = ViewMode::Normal;

    int m_polygonCounter = 0;
    int m_polylineCounter = 0;

    // Polyline and Polygon editing
    QVector<Polyline> m_polylines;
    QVector<Polygon> m_polygons;
    int m_selectedPolygonIndex = -1;
    QSet<int> m_selectedPolygons;   // 多边形的多选索引集合
    QSet<int> m_selectedPolylines;  // 多段线的多选索引集合
    int m_hoveredPolygonIndex = -1;  // 鼠标悬停的高亮多边形（只读模式下）
    int m_hoveredResultIndex = -1;   // 鼠标悬停的高亮结果多边形（只读模式下）
    int m_highlightedResultIndex = -1; // 手动选择的高亮结果多边形

    // Clip 和 Subject 集合
    QSet<int> m_clipPolygons;       // 加入 clip 的多边形
    QSet<int> m_clipPolylines;      // 加入 clip 的多段线
    QSet<int> m_subjectPolygons;   // 加入 subject 的多边形
    QSet<int> m_subjectPolylines;  // 加入 subject 的多段线

    // 布尔运算结果
    QVector<BooleanResultPolygon> m_booleanResults;
    QVector<BooleanResultPolygon> m_clipPatternResults;
    QVector<BooleanResultPolygon> m_subjectPatternResults;

    // Dragging state
    enum class DragMode {
        None,
        Vertex,
        EdgeMidpoint
    };
    DragMode m_dragMode = DragMode::None;
    Edge m_draggedEdge;
    QPointF m_dragStartPos;
    QPointF m_originalPoint;
    int m_splitVertexIndex = -1;
    Qt::MouseButton m_dragButton = Qt::NoButton;

    // Edge to arc conversion
    Edge m_edgeToArc;
    QPointF m_arcThroughPoint;

    // Viewport
    qreal m_scale = 1.0;
    QPointF m_offset;

    bool m_isPanning = false;
    QPointF m_panStart;

    // Read-only mode
    bool m_readOnly = false;

    // Context menu state
    QPointF m_contextMenuPosition;
    bool m_hasDragged = false; // Track if dragging occurred before right click

    // Mouse position for HUD
    QPointF m_mousePos;

    bool computeArcThrough3Points(const QPointF& p1, const QPointF& p2, const QPointF& p3, ArcSegment& outArc) const;

signals:
    void polylineAdded(int index, const QString& name);
    void polygonAdded(int index, const QString& name);
    void polylineRemoved(int index);
    void polygonRemoved(int index);
    void polygonsDeleted(const QSet<int>& polylineIndices, const QSet<int>& polygonIndices);
    void selectionChanged(int polygonIndex);
    void polylineModified();
    void polygonModified();
    void polygonColorChanged(int polygonIndex, const QColor& color);
    void polygonRoleChanged(int index, bool isPolygon);
};
