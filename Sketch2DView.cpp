#include "Sketch2DView.h"
#include "BooleanOperations.h"
#include "PolygonIO.h"

#include <QMouseEvent>
#include <QEvent>
#include <QPainter>
#include <QWheelEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QGuiApplication>
#include <QCoreApplication>
#include <QtMath>
#include <QPainterPath>
#include <random>
#include <cmath>
#include <typeinfo>
#include <QColorDialog>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDir>
#include <limits>
#include <QDebug>

// 辅助函数：将QColor转换为QRgba64
static QRgba64 QColorToQRgba64(const QColor& color) {
    return QRgba64::fromRgba(color.red(), color.green(), color.blue(), color.alpha());
}

Sketch2DView::Sketch2DView(QWidget* parent)
    : QWidget(parent), m_scale(1.0), m_offset(0.0, 0.0) {
    setMouseTracking(true);
    setAutoFillBackground(true);

    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(18, 18, 18));
    setPalette(pal);

    setMinimumSize(400, 300);
}

void Sketch2DView::setTool(Tool tool) {
    if (m_readOnly) return;
    m_tool = tool;
    m_dragMode = DragMode::None;
    m_dragButton = Qt::NoButton;
    update();
}

void Sketch2DView::setScale(qreal scale) {
    m_scale = scale;
    update();
}

void Sketch2DView::setOffset(const QPointF& offset) {
    m_offset = offset;
    update();
}

void Sketch2DView::clear() {
    m_polylines.clear();
    m_polygons.clear();
    m_polygonCounter = 0;
    m_polylineCounter = 0;
    m_dragMode = DragMode::None;
    m_dragButton = Qt::NoButton;
    clearSelection();
    update();
}

qreal Sketch2DView::degFromRad(qreal rad) {
    return rad * 180.0 / M_PI;
}

qreal Sketch2DView::radFromDeg(qreal deg) {
    return deg * M_PI / 180.0;
}

qreal Sketch2DView::bulgeFromArc(const QPointF& p1, const QPointF& p2, const QPointF& throughPoint) {
    // bulge = tan(θ/4), where θ is the arc angle
    // Compute arc angle from the chord and the through point

    QLineF chord(p1, p2);
    qreal chordLength = chord.length();
    if (chordLength < 1e-6) return 0.0;

    // Find the midpoint of the chord
    QPointF midpoint = (p1 + p2) / 2.0;

    // Vector from midpoint to through point (perpendicular to chord at midpoint)
    QPointF sagitta = throughPoint - midpoint;

    // Distance from chord to arc (sagitta)
    qreal sagittaLength = QLineF(QPointF(0, 0), sagitta).length();

    // Determine direction (sign of bulge)
    // Using cross product to determine which side of the chord the through point is on
    // In Qt's coordinate system (y-axis down):
    // - cross > 0 means throughPoint is to the left of chord direction p1->p2
    // - cross < 0 means throughPoint is to the right of chord direction p1->p2
    //
    // User expectation:
    // - Dragging midpoint down (visually) → arc curves downward (concave down)
    // - This means center is visually above the chord (in Qt's smaller y)
    //
    // Tailor library expects:
    // - bulge > 0 → counter-clockwise (CCW)
    //
    // For a concave-down arc (dragged down), the center is above the chord,
    // and traveling from p1 to p2, this requires CCW movement.
    // Therefore: dragging down should give positive bulge.
    //
    // In Qt coordinates, dragging "down" means throughPoint has larger y,
    // which gives cross < 0 (to the right of chord).
    // We need to invert the sign so that dragging down → positive bulge.
    qreal cross = (p2.x() - p1.x()) * (throughPoint.y() - p1.y()) -
        (p2.y() - p1.y()) * (throughPoint.x() - p1.x());
    qreal sign = (cross < 0) ? 1.0 : -1.0;

    // For a circular arc:
    // sagitta = R * (1 - cos(θ/2))
    // chord = 2 * R * sin(θ/2)
    // Therefore: sagitta / chord = (1 - cos(θ/2)) / (2 * sin(θ/2))
    // Using half-angle identities: = tan(θ/4) / 2
    // So: bulge = tan(θ/4) = 2 * sagitta / chord

    qreal bulge = sign * 2.0 * sagittaLength / chordLength;
    return bulge;
}

QPointF Sketch2DView::arcPointFromBulge(const QPointF& p1, const QPointF& p2, qreal bulge) {
    // Given two points and bulge, find a point on the arc
    // The point is at the maximum sagitta position (midpoint of chord + perpendicular offset)

    QLineF chord(p1, p2);
    qreal chordLength = chord.length();
    if (chordLength < 1e-6) return (p1 + p2) / 2.0;

    // Sagitta = bulge * chord / 2
    qreal sagitta = bulge * chordLength / 2.0;

    // Midpoint of chord
    QPointF midpoint = (p1 + p2) / 2.0;

    // Perpendicular direction
    // In Qt's coordinate system (y-axis down), we need to invert the perpendicular
    // so that positive bulge (dragged down) shows the point below the chord
    QPointF perp((p2.y() - p1.y()), -(p2.x() - p1.x()));
    qreal perpLength = QLineF(QPointF(0, 0), perp).length();
    if (perpLength < 1e-6) return midpoint;
    perp = perp / perpLength;

    // Point on arc = midpoint + sagitta * perpendicular
    return midpoint + perp * sagitta;
}

Sketch2DView::ArcSegment Sketch2DView::arcSegmentFromBulge(const QPointF& p1, const QPointF& p2, qreal bulge) {
    // Convert bulge to ArcSegment
    ArcSegment arc;

    // For zero bulge, return degenerate arc (straight line)
    if (qAbs(bulge) < 1e-6) {
        arc.center = (p1 + p2) / 2.0;
        arc.radius = 0.0;
        arc.startAngleDeg = 0.0;
        arc.spanAngleDeg = 0.0;
        return arc;
    }

    // Bulge = tan(θ/4)
    // θ = 4 * atan(bulge)

    qreal theta = 4.0 * qAtan(qAbs(bulge));

    // Chord length
    qreal chordLength = QLineF(p1, p2).length();

    // Radius from chord and central angle
    // chord = 2 * R * sin(θ/2)
    // R = chord / (2 * sin(θ/2))
    qreal sinHalfTheta = qSin(theta / 2.0);
    if (sinHalfTheta < 1e-6) {
        // Degenerate case
        arc.center = (p1 + p2) / 2.0;
        arc.radius = 0.0;
        arc.startAngleDeg = 0.0;
        arc.spanAngleDeg = 0.0;
        return arc;
    }

    qreal radius = chordLength / (2.0 * sinHalfTheta);
    arc.radius = radius;

    // Center is at the intersection of perpendicular bisector of chord
    // and a line from midpoint in the direction of the bulge

    // Midpoint
    QPointF midpoint = (p1 + p2) / 2.0;

    // Direction from chord to center (perpendicular to chord)
    QPointF chordDir = (p2 - p1) / chordLength;
    QPointF perpDir(-chordDir.y(), chordDir.x());

    // Distance from midpoint to center
    // d = R * cos(θ/2)
    qreal distanceToCenter = radius * qCos(theta / 2.0);

    // Sign of bulge determines which side the center is on
    // Tailor library expects: bulge > 0 → counter-clockwise (CCW)
    // For a positive bulge (CCW arc), the center is to the left of chord direction p1->p2
    // For a negative bulge (CW arc), the center is to the right of chord direction p1->p2
    // Since perpDir is chordDir rotated CCW by 90°:
    // - Positive bulge (CCW) → center is in +perpDir direction
    // - Negative bulge (CW) → center is in -perpDir direction
    qreal sign = (bulge >= 0) ? 1.0 : -1.0;
    arc.center = midpoint + perpDir * (distanceToCenter * sign);

    // Start and end angles
    arc.startAngleDeg = angleDegAt(arc.center, p1);
    qreal endAngleDeg = angleDegAt(arc.center, p2);

    // Span angle - we need to ensure the arc goes through the bulge direction
    // Bulge positive means arc is counter-clockwise (CCW, negative span in Qt)
    // Bulge negative means arc is clockwise (CW, positive span in Qt)

    // Calculate normalized span
    qreal spanDeg = normalizedSpanDeg(arc.startAngleDeg, endAngleDeg);

    // Check if this span gives us the correct curvature direction
    QPointF testPoint = arcPointFromBulge(p1, p2, bulge);
    QPointF centerToTest = testPoint - arc.center;
    qreal testAngle = angleDegAt(arc.center, testPoint);

    // Determine if test angle lies on the arc with current span
    auto isAngleOnArc = [](qreal start, qreal span, qreal ang) {
        auto norm = [](qreal v) {
            while (v < 0) v += 360.0;
            while (v >= 360.0) v -= 360.0;
            return v;
            };
        start = norm(start);
        ang = norm(ang);

        if (qFuzzyIsNull(span)) return false;
        if (span > 0) {
            qreal end = norm(start + span);
            if (start <= end) return (ang >= start && ang <= end);
            return (ang >= start || ang <= end);
        } else {
            qreal end = norm(start + span);
            if (end <= start) return (ang <= start && ang >= end);
            return (ang <= start || ang >= end);
        }
        };

    if (!isAngleOnArc(arc.startAngleDeg, spanDeg, testAngle)) {
        // Try the other direction
        spanDeg = (spanDeg > 0) ? (spanDeg - 360.0) : (spanDeg + 360.0);
    }

    arc.spanAngleDeg = spanDeg;
    return arc;
}

qreal Sketch2DView::splitArcBulge(qreal bulge) {
    // When splitting an arc in half, we need to calculate the new bulge value
    // for each half-arc. The half-arc has half the central angle.

    if (qAbs(bulge) < 1e-6) {
        return 0.0;  // Straight line
    }

    // Original arc: bulge = tan(θ/4), where θ is the central angle
    // Half-arc: new_bulge = tan(θ/8)

    // Use half-angle formula: tan(θ/8) = tan(θ/4) / (1 + sqrt(1 + tan²(θ/4)))
    // Or more simply: tan(θ/8) = sin(θ/8) / cos(θ/8)

    // Let's derive from the original bulge:
    // Original: b = tan(θ/4)
    // We want: b' = tan(θ/8)

    // Using tan half-angle formula: tan(x/2) = sin(x) / (1 + cos(x))
    // Let x = θ/4, then tan(θ/8) = tan(θ/4 / 2) = tan(θ/4) / (1 + sqrt(1 + tan²(θ/4)))

    // Actually, a simpler formula using the half-angle identity:
    // tan(x/2) = (1 - cos(x)) / sin(x) = sin(x) / (1 + cos(x))

    // Using b = tan(θ/4), we can find θ/4 = atan(b)
    // Then θ/8 = atan(b) / 2
    // So b' = tan(atan(b) / 2)

    // Using the half-angle formula for tangent:
    // tan(x/2) = (1 - cos(2x)) / sin(2x) = (1 - cos(2x)) / (2 * sin(x) * cos(x))

    // Let's use a numerical approach:
    // θ/4 = atan(|bulge|)
    // θ/8 = atan(|bulge|) / 2
    // |new_bulge| = tan(θ/8) = tan(atan(|bulge|) / 2)

    // Using half-angle formula: tan(x/2) = sin(x) / (1 + cos(x))
    // Let x = atan(|bulge|)
    // tan(x/2) = sin(x) / (1 + cos(x))
    // sin(x) = |bulge| / sqrt(1 + |bulge|²)
    // cos(x) = 1 / sqrt(1 + |bulge|²)
    // tan(x/2) = (|bulge| / sqrt(1 + |bulge|²)) / (1 + 1 / sqrt(1 + |bulge|²))
    //          = |bulge| / (sqrt(1 + |bulge|²) + 1)

    qreal absBulge = qAbs(bulge);
    qreal sqrtTerm = qSqrt(1.0 + absBulge * absBulge);
    qreal newAbsBulge = absBulge / (sqrtTerm + 1.0);

    // Preserve the sign
    return (bulge >= 0) ? newAbsBulge : -newAbsBulge;
}

QPointF Sketch2DView::snapToPixelCenter(const QPointF& p) const {
    return QPointF(qRound(p.x()) + 0.5, qRound(p.y()) + 0.5);
}

QPointF Sketch2DView::worldToScreen(const QPointF& worldPos) const {
    const QPointF center(width() / 2.0, height() / 2.0);
    return center + (worldPos - m_offset) * m_scale;
}

QPointF Sketch2DView::screenToWorld(const QPointF& screenPos) const {
    const QPointF center(width() / 2.0, height() / 2.0);
    return m_offset + (screenPos - center) / m_scale;
}

qreal Sketch2DView::angleDegAt(const QPointF& center, const QPointF& p) {
    // In Qt, +x right, +y down.
    // QLineF::angle() returns degrees clockwise from +x.
    QLineF l(center, p);
    return l.angle();
}

qreal Sketch2DView::normalizedSpanDeg(qreal startDeg, qreal endDeg) {
    // We want the CCW span in Qt's clockwise-angle system.
    // Since angles increase clockwise in QLineF::angle(), a CCW movement is negative.
    qreal span = endDeg - startDeg;
    while (span <= -180.0) span += 360.0;
    while (span > 180.0) span -= 360.0;
    return span;
}

QPointF Sketch2DView::getEdgeMidpoint(const QPointF& p1, const QPointF& p2) const {
    return (p1 + p2) / 2.0;
}

bool Sketch2DView::isNearPoint(const QPointF& target, const QPointF& pos, qreal threshold) const {
    QPointF screenTarget = worldToScreen(target);
    QPointF delta = pos - screenTarget;
    return (delta.x() * delta.x() + delta.y() * delta.y()) < (threshold * threshold);
}

bool Sketch2DView::isNearEdgeMidpoint(const QPointF& p1, const QPointF& p2, const QPointF& pos, qreal threshold) const {
    QPointF midpoint = worldToScreen(getEdgeMidpoint(p1, p2));
    QPointF delta = pos - midpoint;
    return (delta.x() * delta.x() + delta.y() * delta.y()) < (threshold * threshold);
}

Sketch2DView::Edge Sketch2DView::findNearbyEdgeMidpoint(const QPointF& pos, qreal threshold) const {
    // Check polygons
    for (int i = 0; i < m_polygons.size(); ++i) {
        const auto& poly = m_polygons[i];
        for (int j = 0; j < poly.vertices.size(); ++j) {
            int next = (j + 1) % poly.vertices.size();
            const auto& v1 = poly.vertices[j];
            const auto& v2 = poly.vertices[next];

            QPointF checkPoint;
            if (qAbs(v1.bulge) < 1e-6) {
                // Line edge - check midpoint
                checkPoint = getEdgeMidpoint(v1.point, v2.point);
            } else {
                // Arc edge - check point at maximum sagitta
                checkPoint = arcPointFromBulge(v1.point, v2.point, v1.bulge);
            }

            if (isNearPoint(checkPoint, pos, threshold)) {
                return Edge{ i, j, next, true }; // true for polygon
            }
        }
    }
    // Check polylines (don't wrap around)
    for (int i = 0; i < m_polylines.size(); ++i) {
        const auto& poly = m_polylines[i];
        for (int j = 0; j < poly.vertices.size() - 1; ++j) {
            int next = j + 1;
            const auto& v1 = poly.vertices[j];
            const auto& v2 = poly.vertices[next];

            QPointF checkPoint;
            if (qAbs(v1.bulge) < 1e-6) {
                // Line edge - check midpoint
                checkPoint = getEdgeMidpoint(v1.point, v2.point);
            } else {
                // Arc edge - check point at maximum sagitta
                checkPoint = arcPointFromBulge(v1.point, v2.point, v1.bulge);
            }

            if (isNearPoint(checkPoint, pos, threshold)) {
                return Edge{ i, j, next, false }; // false for polyline
            }
        }
    }
    return Edge{ -1, -1, -1, false };
}

Sketch2DView::Edge Sketch2DView::findNearbyVertex(const QPointF& pos, qreal threshold) const {
    // Check polygons first
    for (int i = 0; i < m_polygons.size(); ++i) {
        const auto& poly = m_polygons[i];
        for (int j = 0; j < poly.vertices.size(); ++j) {
            if (isNearPoint(poly.vertices[j].point, pos, threshold)) {
                return Edge{ i, j, -1, true }; // true for polygon
            }
        }
    }
    // Check polylines
    for (int i = 0; i < m_polylines.size(); ++i) {
        const auto& poly = m_polylines[i];
        for (int j = 0; j < poly.vertices.size(); ++j) {
            if (isNearPoint(poly.vertices[j].point, pos, threshold)) {
                return Edge{ i, j, -1, false }; // false for polyline
            }
        }
    }
    return Edge{ -1, -1, -1, false };
}

bool Sketch2DView::computeArcThrough3Points(const QPointF& p1, const QPointF& p2, const QPointF& p3, ArcSegment& outArc) const {
    // Compute circumcircle for 3 points. Return false if nearly collinear.
    const qreal x1 = p1.x(), y1 = p1.y();
    const qreal x2 = p2.x(), y2 = p2.y();
    const qreal x3 = p3.x(), y3 = p3.y();

    const qreal a = x1 - x2;
    const qreal b = y1 - y2;
    const qreal c = x1 - x3;
    const qreal d = y1 - y3;

    const qreal e = ((x1 * x1 - x2 * x2) + (y1 * y1 - y2 * y2)) / 2.0;
    const qreal f = ((x1 * x1 - x3 * x3) + (y1 * y1 - y3 * y3)) / 2.0;

    const qreal det = a * d - b * c;
    if (qAbs(det) < 1e-6) {
        return false;
    }

    const qreal cx = (d * e - b * f) / det;
    const qreal cy = (-c * e + a * f) / det;

    const QPointF center(cx, cy);
    const qreal r = QLineF(center, p1).length();
    if (r < 1e-6) return false;

    const qreal startDeg = angleDegAt(center, p1);
    const qreal midDeg = angleDegAt(center, p3);
    const qreal endDeg = angleDegAt(center, p2);

    // Choose span so that the arc passes through p3.
    // We test both directions: span1 from start->end, span2 the other way.
    qreal span1 = endDeg - startDeg;
    while (span1 <= -360.0) span1 += 360.0;
    while (span1 > 360.0) span1 -= 360.0;

    qreal span2 = (span1 > 0) ? (span1 - 360.0) : (span1 + 360.0);

    auto isAngleOnArc = [](qreal start, qreal span, qreal ang) {
        // Angles are clockwise increasing (Qt). Span can be +/-.
        auto norm = [](qreal v) {
            while (v < 0) v += 360.0;
            while (v >= 360.0) v -= 360.0;
            return v;
            };
        start = norm(start);
        ang = norm(ang);

        if (qFuzzyIsNull(span)) return false;
        if (span > 0) {
            // clockwise
            qreal end = norm(start + span);
            if (start <= end) return (ang >= start && ang <= end);
            return (ang >= start || ang <= end);
        } else {
            // counter-clockwise
            qreal end = norm(start + span);
            if (end <= start) return (ang <= start && ang >= end);
            return (ang <= start || ang >= end);
        }
        };

    const bool midOn1 = isAngleOnArc(startDeg, span1, midDeg);
    const qreal span = midOn1 ? span1 : span2;

    outArc.center = center;
    outArc.radius = r;
    outArc.startAngleDeg = startDeg;
    outArc.spanAngleDeg = span;
    return true;
}



void Sketch2DView::mousePressEvent(QMouseEvent* event) {
    const QPointF p = snapToPixelCenter(screenToWorld(event->position()));
    const QPointF screenPos = event->position();

    // Middle mouse button starts panning (always allowed, even in read-only mode)
    if (event->button() == Qt::MiddleButton) {
        m_isPanning = true;
        m_panStart = event->position();
        setCursor(Qt::ClosedHandCursor);
        QWidget::mousePressEvent(event);
        return;
    }

    // In read-only mode, only allow panning (middle mouse) and zooming (wheel)
    if (m_readOnly) {
        QWidget::mousePressEvent(event);
        return;
    }

    // Only handle left and right buttons for polygon interaction
    if (event->button() != Qt::LeftButton && event->button() != Qt::RightButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    // Reset drag flag when pressing right button
    if (event->button() == Qt::RightButton) {
        m_hasDragged = false;
    }

    // Polygon tool: check for vertex or edge midpoint interaction
    if (m_tool == Tool::Polyline || m_tool == Tool::Polygon) {
        // First check if we're clicking on a vertex (for dragging)
        Edge vertexEdge = findNearbyVertex(screenPos, 8.0);
        if (vertexEdge.polygonIndex >= 0) {
            m_dragMode = DragMode::Vertex;
            m_draggedEdge = vertexEdge;
            m_dragStartPos = screenPos;
            m_dragButton = event->button();
            if (vertexEdge.isPolygon) {
                m_originalPoint = m_polygons[vertexEdge.polygonIndex].vertices[vertexEdge.vertexIndex1].point;
            } else {
                m_originalPoint = m_polylines[vertexEdge.polygonIndex].vertices[vertexEdge.vertexIndex1].point;
            }
            setSelectedPolygon(vertexEdge.polygonIndex, vertexEdge.isPolygon);
            return;
        }

        // Check if we're clicking on an edge midpoint
        Edge edge = findNearbyEdgeMidpoint(screenPos, 8.0);
        if (edge.polygonIndex >= 0) {
            m_dragMode = DragMode::EdgeMidpoint;
            m_draggedEdge = edge;
            m_dragStartPos = screenPos;
            m_dragButton = event->button();
            m_edgeToArc = edge;
            setSelectedPolygon(edge.polygonIndex, edge.isPolygon);
            return;
        }

        // If not interacting with existing shape, do nothing
        // Creation is now done via context menu
        QWidget::mousePressEvent(event);
        return;
    }


}

void Sketch2DView::mouseMoveEvent(QMouseEvent* event) {
    const QPointF screenPos = event->position();
    m_mousePos = screenPos;
    const QPointF p = screenToWorld(screenPos);

    // 只读模式下检测鼠标悬停的多边形
    if (m_readOnly) {
        int oldHoveredResult = m_hoveredResultIndex;
        m_hoveredResultIndex = -1;

        // 根据视图模式检测相应的结果多边形
        const QVector<BooleanResultPolygon>* results = nullptr;
        if (m_viewMode == ViewMode::ClipFocus) {
            results = &m_clipPatternResults;
        } else if (m_viewMode == ViewMode::SubjectFocus) {
            results = &m_subjectPatternResults;
        } else if (m_viewMode == ViewMode::BooleanResult) {
            results = &m_booleanResults;
        }

        if (results) {
            // 查找所有包含该点的结果多边形
            QList<int> containingResults;
            for (int i = 0; i < results->size(); ++i) {
                const auto& resultPoly = (*results)[i];
                QPainterPath path = createResultPolygonPath(resultPoly);
                if (path.contains(p)) {
                    containingResults.append(i);
                }
            }

            // 如果有包含该点的结果多边形，优先高亮最内侧的（最小的，包括内环）
            if (!containingResults.isEmpty()) {
                // 检测嵌套关系：选择最内侧（被其他多边形包含最多）的多边形
                m_hoveredResultIndex = containingResults.first();

                if (containingResults.size() > 1) {
                    // 计算每个结果多边形被其他结果多边形包含的次数
                    QVector<int> nestingCount(containingResults.size(), 0);
                    QVector<QPainterPath> paths;
                    for (int idx : containingResults) {
                        paths.append(createResultPolygonPath((*results)[idx]));
                    }

                    for (int i = 0; i < containingResults.size(); ++i) {
                        const auto& testPoly = (*results)[containingResults[i]];
                        if (!testPoly.vertices.isEmpty()) {
                            QPointF testPoint = testPoly.vertices[0].point;
                            for (int j = 0; j < containingResults.size(); ++j) {
                                if (i != j && paths[j].contains(testPoint)) {
                                    nestingCount[i]++;
                                }
                            }
                        }
                    }

                    // 选择嵌套最多的（最内侧的，包括内环）
                    int maxIndex = 0;
                    for (int i = 1; i < nestingCount.size(); ++i) {
                        if (nestingCount[i] > nestingCount[maxIndex]) {
                            maxIndex = i;
                        }
                    }
                    m_hoveredResultIndex = containingResults[maxIndex];
                }
            }
        }

        if (oldHoveredResult != m_hoveredResultIndex) {
            update();
        }
    }

    // 总是触发更新以实时显示鼠标坐标
    update();

    // Handle panning
    if (m_isPanning) {
        QPointF delta = event->position() - m_panStart;
        m_offset -= delta / m_scale;
        m_panStart = event->position();
        update();
        QWidget::mouseMoveEvent(event);
        return;
    }

    // Handle polygon/polyline vertex dragging
    if (m_dragMode == DragMode::Vertex && event->buttons() & (m_dragButton == Qt::LeftButton ? Qt::LeftButton : Qt::RightButton)) {
        auto& poly = m_draggedEdge.isPolygon ? m_polygons[m_draggedEdge.polygonIndex] : m_polylines[m_draggedEdge.polygonIndex];
        QPointF deltaScreen = screenPos - m_dragStartPos;
        QPointF deltaWorld = deltaScreen / m_scale;
        poly.vertices[m_draggedEdge.vertexIndex1].point = m_originalPoint + deltaWorld;

        // Mark as dragged if using right button
        if (m_dragButton == Qt::RightButton) {
            m_hasDragged = true;
        }

        if (m_draggedEdge.isPolygon) {
            emit polygonModified();
        } else {
            emit polylineModified();
        }
        update();
        QWidget::mouseMoveEvent(event);
        return;
    }

    // Handle edge midpoint dragging
    if (m_dragMode == DragMode::EdgeMidpoint && event->buttons() & (m_dragButton == Qt::LeftButton ? Qt::LeftButton : Qt::RightButton)) {
        auto& poly = m_draggedEdge.isPolygon ? m_polygons[m_draggedEdge.polygonIndex] : m_polylines[m_draggedEdge.polygonIndex];

        QPointF edgeStart = poly.vertices[m_draggedEdge.vertexIndex1].point;
        QPointF edgeEnd = poly.vertices[m_draggedEdge.vertexIndex2].point;

        if (m_dragButton == Qt::RightButton) {
            // Right click: Directly modify bulge (arc conversion)
            m_arcThroughPoint = p;
            qreal newBulge = bulgeFromArc(edgeStart, edgeEnd, m_arcThroughPoint);
            poly.vertices[m_draggedEdge.vertexIndex1].bulge = newBulge;

            // Mark as dragged
            m_hasDragged = true;

            if (m_draggedEdge.isPolygon) {
                emit polygonModified();
            } else {
                emit polylineModified();
            }
        } else {
            // Left click: Split edge (perpendicular drag) or modify bulge (away drag)
            qreal dragDistance = QLineF(m_dragStartPos, screenPos).length();

            if (m_splitVertexIndex == -1) {
                // First movement: decide whether to split or modify bulge
                QPointF edgeDir = edgeEnd - edgeStart;
                QPointF perpDir(-edgeDir.y(), edgeDir.x());

                QPointF dragWorld = p - getEdgeMidpoint(edgeStart, edgeEnd);
                qreal alongEdge = (dragWorld.x() * edgeDir.x() + dragWorld.y() * edgeDir.y()) / QLineF(edgeStart, edgeEnd).length();
                qreal perpDist = qAbs((dragWorld.x() * perpDir.x() + dragWorld.y() * perpDir.y()) / QLineF(QPointF(0, 0), perpDir).length());

                if (perpDist > qAbs(alongEdge)) {
                    // Dragging perpendicular: split the edge
                    m_splitVertexIndex = m_draggedEdge.vertexIndex2;


                    QPointF midpoint;
                    qreal halfBulge;

                    const qreal originalBulge = poly.vertices[m_draggedEdge.vertexIndex1].bulge;
                    const QColor originalEdgeColor = poly.vertices[m_draggedEdge.vertexIndex1].edgeColor;

                    if (qAbs(originalBulge) < 1e-6) {
                        // Straight line: use midpoint of chord
                        midpoint = getEdgeMidpoint(edgeStart, edgeEnd);
                        halfBulge = 0.0;
                    } else {
                        // Arc: use midpoint on the arc
                        midpoint = arcPointFromBulge(edgeStart, edgeEnd, originalBulge);
                        // Calculate bulge for half-arcs
                        halfBulge = splitArcBulge(originalBulge);

                        // Update the first edge's bulge to half the original
                        poly.vertices[m_draggedEdge.vertexIndex1].bulge = halfBulge;
                    }

                    PolygonVertex newVertex;
                    newVertex.point = midpoint;
                    newVertex.bulge = halfBulge;
                    newVertex.edgeColor = originalEdgeColor;
                    poly.vertices.insert(m_splitVertexIndex, newVertex);

                    // Set up to drag the newly inserted vertex
                    m_originalPoint = midpoint;
                    m_dragStartPos = screenPos;
                    m_dragMode = DragMode::Vertex;
                    m_draggedEdge.vertexIndex1 = m_splitVertexIndex; // Drag the new vertex
                    m_draggedEdge.vertexIndex2 = -1;

                    if (m_draggedEdge.isPolygon) {
                        emit polygonModified();
                    } else {
                        emit polylineModified();
                    }
                } else {
                    // Dragging along/away: modify bulge (convert line to arc)
                    // Update bulge in real-time as we drag
                    m_arcThroughPoint = p;
                    qreal newBulge = bulgeFromArc(edgeStart, edgeEnd, m_arcThroughPoint);
                    poly.vertices[m_draggedEdge.vertexIndex1].bulge = newBulge;
                    if (m_draggedEdge.isPolygon) {
                        emit polygonModified();
                    } else {
                        emit polylineModified();
                    }
                }
            } else {
                // Continue splitting/dragging
                poly.vertices[m_splitVertexIndex].point = p;
                if (m_draggedEdge.isPolygon) {
                    emit polygonModified();
                } else {
                    emit polylineModified();
                }
            }
        }
        update();
        QWidget::mouseMoveEvent(event);
        return;
    }

    QWidget::mouseMoveEvent(event);
}

void Sketch2DView::leaveEvent(QEvent* event) {
    m_mousePos = QPointF();
    m_hoveredPolygonIndex = -1;  // 清除悬停状态
    m_hoveredResultIndex = -1;   // 清除结果悬停状态
    update();
    QWidget::leaveEvent(event);
}

void Sketch2DView::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton && m_isPanning) {
        m_isPanning = false;
        setCursor(Qt::ArrowCursor);
    }

    // Reset drag state on left or right button release
    if (event->button() == Qt::LeftButton || event->button() == Qt::RightButton) {
        m_dragMode = DragMode::None;
        m_splitVertexIndex = -1;
        m_dragButton = Qt::NoButton;
        m_edgeToArc = Edge{ -1, -1, -1 };
        m_arcThroughPoint = QPointF();
    }

    QWidget::mouseReleaseEvent(event);
}

void Sketch2DView::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Apply transform
    painter.translate(width() / 2.0, height() / 2.0);
    painter.scale(m_scale, m_scale);
    painter.translate(-m_offset.x(), -m_offset.y());

    // Background grid
    const int grid = 25;
    painter.save();
    painter.setPen(QPen(QColor(40, 40, 40), 1));
    QRectF worldBounds = getWorldBounds();
    for (int x = qFloor(worldBounds.left() / grid) * grid; x <= worldBounds.right(); x += grid) {
        painter.drawLine(x, worldBounds.top(), x, worldBounds.bottom());
    }
    for (int y = qFloor(worldBounds.top() / grid) * grid; y <= worldBounds.bottom(); y += grid) {
        painter.drawLine(worldBounds.left(), y, worldBounds.right(), y);
    }
    painter.restore();

    // Axes
    painter.save();
    painter.setPen(QPen(QColor(80, 80, 80), 1));
    painter.drawLine(0, worldBounds.top(), 0, worldBounds.bottom());
    painter.drawLine(worldBounds.left(), 0, worldBounds.right(), 0);
    painter.restore();

    // Draw polylines
    painter.save();
    for (int i = 0; i < m_polylines.size(); ++i) {
        // BooleanResult 模式下不绘制原始 polylines
        if (m_viewMode == ViewMode::BooleanResult) {
            continue;
        }

        // 根据视图模式和角色确定颜色
        BooleanRole role = polygonRole(i, false); // false = polyline
        bool isClip = (role == BooleanRole::Clip);
        bool isSubject = (role == BooleanRole::Subject);

        QPen pen;
        QColor penColor;
        qreal penWidth = 2 / m_scale;

        if (m_viewMode == ViewMode::ClipFocus) {
            // ClipFocus 模式：Clip 不绘制，Subject 显示灰色边界
            if (isClip) {
                continue;
            } else if (isSubject) {
                penColor = QColor(255, 50, 50); // Subject 红色边界
            } else {
                continue;
            }
        } else if (m_viewMode == ViewMode::SubjectFocus) {
            // SubjectFocus 模式：Subject 不绘制，Clip 显示蓝色边界
            if (isSubject) {
                continue;
            } else if (isClip) {
                penColor = QColor(0, 100, 255); // Clip 蓝色边界
            } else {
                continue;
            }
        } else {
            // Normal 模式
            if (m_selectedPolylines.contains(i)) {
                penColor = QColor(255, 215, 0); // 选中：金色
                penWidth = 3 / m_scale;
            } else if (isClip) {
                penColor = QColor(0, 100, 255); // Clip 集合：蓝色
            } else if (isSubject) {
                penColor = QColor(255, 50, 50); // Subject 集合：红色
            } else {
                penColor = QColor(230, 230, 230); // 默认：灰色
            }
        }

        pen = QPen(penColor, penWidth);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);

        const auto& poly = m_polylines[i];
        QPainterPath path;

        if (poly.vertices.size() > 0) {
            QPointF startPoint = poly.vertices[0].point;
            path.moveTo(startPoint);

            for (int j = 0; j < poly.vertices.size() - 1; ++j) {
                int next = j + 1;
                const auto& v1 = poly.vertices[j];
                const auto& v2 = poly.vertices[next];

                if (qAbs(v1.bulge) < 1e-6) {
                    path.lineTo(v2.point);
                } else {
                    ArcSegment arc = arcSegmentFromBulge(v1.point, v2.point, v1.bulge);
                    const QRectF rect(arc.center.x() - arc.radius,
                        arc.center.y() - arc.radius,
                        arc.radius * 2.0,
                        arc.radius * 2.0);
                    path.arcTo(rect, arc.startAngleDeg, arc.spanAngleDeg);
                }
            }
            painter.drawPath(path);
        }
    }
    painter.restore();

    // Draw polygons
    painter.save();
    for (int i = 0; i < m_polygons.size(); ++i) {
        // BooleanResult 模式下不绘制原始 polygons
        if (m_viewMode == ViewMode::BooleanResult) {
            continue;
        }

        // 根据视图模式和角色确定颜色
        BooleanRole role = polygonRole(i, true); // true = polygon
        bool isClip = (role == BooleanRole::Clip);
        bool isSubject = (role == BooleanRole::Subject);

        // 检查是否是鼠标悬停的多边形（只读模式下）
        bool isHovered = (m_readOnly && i == m_hoveredPolygonIndex);

        QPen pen;
        QColor penColor;
        QColor brushColor;
        qreal penWidth = 2 / m_scale;

        if (m_viewMode == ViewMode::ClipFocus) {
            // ClipFocus 模式：Clip 不绘制（Pattern 结果已显示），Subject 只显示边界
            if (isClip) {
                continue; // Clip 不绘制原始多边形
            } else if (isSubject) {
                if (isHovered) {
                    penColor = QColor(255, 165, 0); // 悬停：橙色
                    penWidth = 4 / m_scale;
                    brushColor = QColor(255, 165, 0, 80);
                } else {
                    penColor = QColor(255, 50, 50); // Subject 红色边界
                    // 不设置 brushColor，让它保持无效状态
                }
            } else {
                continue; // 其他不绘制
            }
        } else if (m_viewMode == ViewMode::SubjectFocus) {
            // SubjectFocus 模式：Subject 不绘制（Pattern 结果已显示），Clip 只显示边界
            if (isSubject) {
                continue; // Subject 不绘制原始多边形
            } else if (isClip) {
                if (isHovered) {
                    penColor = QColor(255, 165, 0); // 悬停：橙色
                    penWidth = 4 / m_scale;
                    brushColor = QColor(255, 165, 0, 80);
                } else {
                    penColor = QColor(0, 100, 255); // Clip 蓝色边界
                    // 不设置 brushColor，让它保持无效状态
                }
            } else {
                continue; // 其他不绘制
            }
        } else {
            // Normal 模式
            if (isHovered) {
                penColor = QColor(255, 165, 0); // 悬停：橙色
                penWidth = 4 / m_scale;
                brushColor = QColor(255, 165, 0, 80);
            } else if (m_selectedPolygons.contains(i)) {
                penColor = QColor(255, 215, 0); // 选中：金色
                penWidth = 3 / m_scale;
                brushColor = QColor(255, 215, 0, 30);
            } else if (isClip) {
                penColor = QColor(0, 100, 255); // Clip 集合：蓝色
                brushColor = QColor(0, 100, 255, 50);
            } else if (isSubject) {
                penColor = QColor(255, 50, 50); // Subject 集合：红色
                brushColor = QColor(255, 50, 50, 50);
            } else {
                penColor = QColor(144, 238, 144); // 默认：绿色
                brushColor = QColor(144, 238, 144, 50);
            }
        }

        pen = QPen(penColor, penWidth);
        painter.setPen(pen);

        const auto& poly = m_polygons[i];

        if (poly.vertices.size() > 0) {
            // 先绘制填充（使用统一颜色）
            QPainterPath fillPath;
            QPointF startPoint = poly.vertices[0].point;
            fillPath.moveTo(startPoint);

            for (int j = 0; j < poly.vertices.size(); ++j) {
                int next = (j + 1) % poly.vertices.size();
                const auto& v1 = poly.vertices[j];
                const auto& v2 = poly.vertices[next];

                if (qAbs(v1.bulge) < 1e-6) {
                    fillPath.lineTo(v2.point);
                } else {
                    ArcSegment arc = arcSegmentFromBulge(v1.point, v2.point, v1.bulge);
                    const QRectF rect(arc.center.x() - arc.radius,
                        arc.center.y() - arc.radius,
                        arc.radius * 2.0,
                        arc.radius * 2.0);
                    fillPath.arcTo(rect, arc.startAngleDeg, arc.spanAngleDeg);
                }
            }

            fillPath.closeSubpath();
            painter.setPen(Qt::NoPen);
            if (brushColor.isValid() && brushColor.alpha() > 0) {
                painter.setBrush(brushColor);
            } else {
                painter.setBrush(Qt::NoBrush);
            }
            painter.drawPath(fillPath);

            // 然后每条边单独绘制边界（使用各边的 edgeColor）
            for (int j = 0; j < poly.vertices.size(); ++j) {
                int next = (j + 1) % poly.vertices.size();
                const auto& v1 = poly.vertices[j];
                const auto& v2 = poly.vertices[next];

                // 确定边界颜色
                QColor edgePenColor = penColor; // 默认使用上面计算的颜色
                if (v1.edgeColor != QColor(255, 255, 255)) {
                    // 如果顶点有自定义颜色，使用它
                    edgePenColor = v1.edgeColor;
                }
                QPen edgePen(edgePenColor, penWidth);
                painter.setPen(edgePen);
                painter.setBrush(Qt::NoBrush);

                if (qAbs(v1.bulge) < 1e-6) {
                    painter.drawLine(v1.point, v2.point);
                } else {
                    ArcSegment arc = arcSegmentFromBulge(v1.point, v2.point, v1.bulge);
                    QPainterPath edgePath;
                    const QRectF rect(arc.center.x() - arc.radius,
                        arc.center.y() - arc.radius,
                        arc.radius * 2.0,
                        arc.radius * 2.0);
                    edgePath.moveTo(v1.point);
                    edgePath.arcTo(rect, arc.startAngleDeg, arc.spanAngleDeg);
                    painter.drawPath(edgePath);
                }
            }
        }
    }
    painter.restore();

    // Draw boolean operation results (only in BooleanResult mode)
    if (m_viewMode == ViewMode::BooleanResult) {
        painter.save();
        for (int i = 0; i < m_booleanResults.size(); ++i) {
            const auto& resultPoly = m_booleanResults[i];

            // 检查是否是悬停的结果多边形
            bool isHovered = (m_readOnly && i == m_hoveredResultIndex);

            // 内环使用黑色填充，悬停时高亮
            if (isHovered) {
                painter.setBrush(resultPoly.isHole ? QColor(0, 0, 0, 150) : resultPoly.color.lighter(130));
            } else {
                painter.setBrush(resultPoly.isHole ? QColor(0, 0, 0) : resultPoly.color);
            }

            if (resultPoly.vertices.size() > 0) {
                // 先绘制填充
                QPainterPath fillPath;
                QPointF startPoint = resultPoly.vertices[0].point;
                fillPath.moveTo(startPoint);

                for (int j = 0; j < resultPoly.vertices.size(); ++j) {
                    int next = (j + 1) % resultPoly.vertices.size();
                    const auto& v1 = resultPoly.vertices[j];
                    const auto& v2 = resultPoly.vertices[next];

                    if (qAbs(v1.bulge) < 1e-6) {
                        fillPath.lineTo(v2.point);
                    } else {
                        ArcSegment arc = arcSegmentFromBulge(v1.point, v2.point, v1.bulge);
                        const QRectF rect(arc.center.x() - arc.radius,
                            arc.center.y() - arc.radius,
                            arc.radius * 2.0,
                            arc.radius * 2.0);
                        fillPath.arcTo(rect, arc.startAngleDeg, arc.spanAngleDeg);
                    }
                }
                fillPath.closeSubpath();
                painter.setPen(Qt::NoPen);
                painter.drawPath(fillPath);

                // 然后每条边单独绘制边界
                // 确定多边形的主要边颜色（使用第一条非黑色的边颜色）
                QColor primaryEdgeColor = QColor(255, 255, 255); // 默认白色
                for (const QColor& color : resultPoly.edgeColors) {
                    if (color != QColor(0, 0, 0) && color != QColor(255, 255, 255)) {
                        primaryEdgeColor = color;
                        break;
                    }
                }

                for (int j = 0; j < resultPoly.vertices.size(); ++j) {
                    int next = (j + 1) % resultPoly.vertices.size();
                    const auto& v1 = resultPoly.vertices[j];
                    const auto& v2 = resultPoly.vertices[next];

                    // 使用每条边的颜色，优先使用 edgeColors 数组，如果没有则使用 vertex 的 edgeColor
                    QColor edgeColor;
                    if (j < resultPoly.edgeColors.size()) {
                        edgeColor = resultPoly.edgeColors[j];
                        // 如果边颜色是黑色，使用多边形的主要边颜色
                        if (edgeColor == QColor(0, 0, 0)) {
                            edgeColor = primaryEdgeColor;
                        }
                    } else if (!v1.edgeColor.isValid() || v1.edgeColor == QColor(255, 255, 255)) {
                        edgeColor = QColor(255, 255, 255);
                    } else {
                        edgeColor = v1.edgeColor;
                    }
                    // 悬停时使用橙色高亮
                    if (isHovered) {
                        edgeColor = QColor(255, 165, 0);
                    }
                    QPen pen(edgeColor, isHovered ? 4 / m_scale : 2 / m_scale);
                    painter.setPen(pen);
                    painter.setBrush(Qt::NoBrush);

                    if (qAbs(v1.bulge) < 1e-6) {
                        painter.drawLine(v1.point, v2.point);
                    } else {
                        ArcSegment arc = arcSegmentFromBulge(v1.point, v2.point, v1.bulge);
                        QPainterPath edgePath;
                        const QRectF rect(arc.center.x() - arc.radius,
                            arc.center.y() - arc.radius,
                            arc.radius * 2.0,
                            arc.radius * 2.0);
                        edgePath.moveTo(v1.point);
                        edgePath.arcTo(rect, arc.startAngleDeg, arc.spanAngleDeg);
                        painter.drawPath(edgePath);
                    }
                }
            }
        }
        painter.restore();
    }

    // Draw Clip pattern results (only in ClipFocus mode)
    if (m_viewMode == ViewMode::ClipFocus) {
        painter.save();
        for (int i = 0; i < m_clipPatternResults.size(); ++i) {
            const auto& resultPoly = m_clipPatternResults[i];

            // 检查是否是悬停的结果多边形
            bool isHovered = (m_readOnly && i == m_hoveredResultIndex);

            // 内环使用黑色填充，悬停时高亮
            if (isHovered) {
                painter.setBrush(resultPoly.isHole ? QColor(0, 0, 0, 150) : resultPoly.color.lighter(130));
            } else {
                painter.setBrush(resultPoly.isHole ? QColor(0, 0, 0) : resultPoly.color);
            }

            if (resultPoly.vertices.size() > 0) {
                // 先绘制填充
                QPainterPath fillPath;
                QPointF startPoint = resultPoly.vertices[0].point;
                fillPath.moveTo(startPoint);

                for (int j = 0; j < resultPoly.vertices.size(); ++j) {
                    int next = (j + 1) % resultPoly.vertices.size();
                    const auto& v1 = resultPoly.vertices[j];
                    const auto& v2 = resultPoly.vertices[next];

                    if (qAbs(v1.bulge) < 1e-6) {
                        fillPath.lineTo(v2.point);
                    } else {
                        ArcSegment arc = arcSegmentFromBulge(v1.point, v2.point, v1.bulge);
                        const QRectF rect(arc.center.x() - arc.radius,
                            arc.center.y() - arc.radius,
                            arc.radius * 2.0,
                            arc.radius * 2.0);
                        fillPath.arcTo(rect, arc.startAngleDeg, arc.spanAngleDeg);
                    }
                }
                fillPath.closeSubpath();
                painter.setPen(Qt::NoPen);
                painter.drawPath(fillPath);

                // 然后每条边单独绘制边界
                // 确定多边形的主要边颜色（使用第一条非黑色的边颜色）
                QColor primaryEdgeColor = QColor(255, 255, 255); // 默认白色
                for (const QColor& color : resultPoly.edgeColors) {
                    if (color != QColor(0, 0, 0) && color != QColor(255, 255, 255)) {
                        primaryEdgeColor = color;
                        break;
                    }
                }

                for (int j = 0; j < resultPoly.vertices.size(); ++j) {
                    int next = (j + 1) % resultPoly.vertices.size();
                    const auto& v1 = resultPoly.vertices[j];
                    const auto& v2 = resultPoly.vertices[next];

                    // 使用每条边的颜色，优先使用 edgeColors 数组，如果没有则使用 vertex 的 edgeColor
                    QColor edgeColor;
                    if (j < resultPoly.edgeColors.size()) {
                        edgeColor = resultPoly.edgeColors[j];
                        // 如果边颜色是黑色，使用多边形的主要边颜色
                        if (edgeColor == QColor(0, 0, 0)) {
                            edgeColor = primaryEdgeColor;
                        }
                    } else if (!v1.edgeColor.isValid() || v1.edgeColor == QColor(255, 255, 255)) {
                        edgeColor = QColor(255, 255, 255);
                    } else {
                        edgeColor = v1.edgeColor;
                    }
                    // 悬停时使用橙色高亮
                    if (isHovered) {
                        edgeColor = QColor(255, 165, 0);
                    }
                    QPen pen(edgeColor, isHovered ? 4 / m_scale : 2 / m_scale);
                    painter.setPen(pen);
                    painter.setBrush(Qt::NoBrush);

                    if (qAbs(v1.bulge) < 1e-6) {
                        painter.drawLine(v1.point, v2.point);
                    } else {
                        ArcSegment arc = arcSegmentFromBulge(v1.point, v2.point, v1.bulge);
                        QPainterPath edgePath;
                        const QRectF rect(arc.center.x() - arc.radius,
                            arc.center.y() - arc.radius,
                            arc.radius * 2.0,
                            arc.radius * 2.0);
                        edgePath.moveTo(v1.point);
                        edgePath.arcTo(rect, arc.startAngleDeg, arc.spanAngleDeg);
                        painter.drawPath(edgePath);
                    }
                }
            }
        }
        painter.restore();
    }

    // Draw Subject pattern results (only in SubjectFocus mode)
    if (m_viewMode == ViewMode::SubjectFocus) {
        painter.save();
        for (int i = 0; i < m_subjectPatternResults.size(); ++i) {
            const auto& resultPoly = m_subjectPatternResults[i];

            // 检查是否是悬停的结果多边形
            bool isHovered = (m_readOnly && i == m_hoveredResultIndex);

            // 内环使用黑色填充，悬停时高亮
            if (isHovered) {
                painter.setBrush(resultPoly.isHole ? QColor(0, 0, 0, 150) : resultPoly.color.lighter(130));
            } else {
                painter.setBrush(resultPoly.isHole ? QColor(0, 0, 0) : resultPoly.color);
            }

            if (resultPoly.vertices.size() > 0) {
                // 先绘制填充
                QPainterPath fillPath;
                QPointF startPoint = resultPoly.vertices[0].point;
                fillPath.moveTo(startPoint);

                for (int j = 0; j < resultPoly.vertices.size(); ++j) {
                    int next = (j + 1) % resultPoly.vertices.size();
                    const auto& v1 = resultPoly.vertices[j];
                    const auto& v2 = resultPoly.vertices[next];

                    if (qAbs(v1.bulge) < 1e-6) {
                        fillPath.lineTo(v2.point);
                    } else {
                        ArcSegment arc = arcSegmentFromBulge(v1.point, v2.point, v1.bulge);
                        const QRectF rect(arc.center.x() - arc.radius,
                            arc.center.y() - arc.radius,
                            arc.radius * 2.0,
                            arc.radius * 2.0);
                        fillPath.arcTo(rect, arc.startAngleDeg, arc.spanAngleDeg);
                    }
                }
                fillPath.closeSubpath();
                painter.setPen(Qt::NoPen);
                painter.drawPath(fillPath);

                // 然后每条边单独绘制边界
                // 确定多边形的主要边颜色（使用第一条非黑色的边颜色）
                QColor primaryEdgeColor = QColor(255, 255, 255); // 默认白色
                for (const QColor& color : resultPoly.edgeColors) {
                    if (color != QColor(0, 0, 0) && color != QColor(255, 255, 255)) {
                        primaryEdgeColor = color;
                        break;
                    }
                }

                for (int j = 0; j < resultPoly.vertices.size(); ++j) {
                    int next = (j + 1) % resultPoly.vertices.size();
                    const auto& v1 = resultPoly.vertices[j];
                    const auto& v2 = resultPoly.vertices[next];

                    // 使用每条边的颜色，优先使用 edgeColors 数组，如果没有则使用 vertex 的 edgeColor
                    QColor edgeColor;
                    if (j < resultPoly.edgeColors.size()) {
                        edgeColor = resultPoly.edgeColors[j];
                        // 如果边颜色是黑色，使用多边形的主要边颜色
                        if (edgeColor == QColor(0, 0, 0)) {
                            edgeColor = primaryEdgeColor;
                        }
                    } else if (!v1.edgeColor.isValid() || v1.edgeColor == QColor(255, 255, 255)) {
                        edgeColor = QColor(255, 255, 255);
                    } else {
                        edgeColor = v1.edgeColor;
                    }
                    // 悬停时使用橙色高亮
                    if (isHovered) {
                        edgeColor = QColor(255, 165, 0);
                    }
                    QPen pen(edgeColor, isHovered ? 4 / m_scale : 2 / m_scale);
                    painter.setPen(pen);
                    painter.setBrush(Qt::NoBrush);

                    if (qAbs(v1.bulge) < 1e-6) {
                        painter.drawLine(v1.point, v2.point);
                    } else {
                        ArcSegment arc = arcSegmentFromBulge(v1.point, v2.point, v1.bulge);
                        QPainterPath edgePath;
                        const QRectF rect(arc.center.x() - arc.radius,
                            arc.center.y() - arc.radius,
                            arc.radius * 2.0,
                            arc.radius * 2.0);
                        edgePath.moveTo(v1.point);
                        edgePath.arcTo(rect, arc.startAngleDeg, arc.spanAngleDeg);
                        painter.drawPath(edgePath);
                    }
                }
            }
        }
        painter.restore();
    }

    // Draw polyline vertices and edge midpoints (for editing)
    painter.save();
    for (int i = 0; i < m_polylines.size(); ++i) {
        // BooleanResult、ClipFocus 和 SubjectFocus 模式下不显示顶点和边中点
        if (m_viewMode == ViewMode::BooleanResult ||
            m_viewMode == ViewMode::ClipFocus ||
            m_viewMode == ViewMode::SubjectFocus) {
            continue;
        }

        const auto& poly = m_polylines[i];

        // Draw vertices
        painter.setPen(QPen(Qt::NoPen));
        for (const auto& v : poly.vertices) {
            painter.setBrush(QColor(230, 230, 230));
            painter.drawEllipse(v.point, 4 / m_scale, 4 / m_scale);
        }

        // Draw edge midpoints (don't wrap around for polylines)
        painter.setBrush(QColor(255, 255, 0));
        for (int j = 0; j < poly.vertices.size() - 1; ++j) {
            int next = j + 1;
            QPointF p1 = poly.vertices[j].point;
            QPointF p2 = poly.vertices[next].point;
            qreal bulge = poly.vertices[j].bulge;
            if (qAbs(bulge) < 1e-6) {
                // Line edge midpoint
                QPointF mid = getEdgeMidpoint(p1, p2);
                painter.drawEllipse(mid, 3 / m_scale, 3 / m_scale);
            } else {
                // Arc edge - show the point at maximum sagitta
                QPointF arcPoint = arcPointFromBulge(p1, p2, bulge);
                painter.setBrush(QColor(255, 150, 0));
                painter.drawEllipse(arcPoint, 3 / m_scale, 3 / m_scale);
                painter.setBrush(QColor(255, 255, 0));
            }
        }
    }
    painter.restore();

    // Draw polygon vertices and edge midpoints (for editing)
    painter.save();
    for (int i = 0; i < m_polygons.size(); ++i) {
        // BooleanResult、ClipFocus 和 SubjectFocus 模式下不显示顶点和边中点
        if (m_viewMode == ViewMode::BooleanResult ||
            m_viewMode == ViewMode::ClipFocus ||
            m_viewMode == ViewMode::SubjectFocus) {
            continue;
        }

        const auto& poly = m_polygons[i];

        // Draw vertices
        painter.setPen(QPen(Qt::NoPen));
        for (const auto& v : poly.vertices) {
            painter.setBrush(QColor(144, 238, 144));
            painter.drawEllipse(v.point, 4 / m_scale, 4 / m_scale);
        }

        // Draw edge midpoints
        painter.setBrush(QColor(255, 255, 0));
        for (int j = 0; j < poly.vertices.size(); ++j) {
            int next = (j + 1) % poly.vertices.size();
            QPointF p1 = poly.vertices[j].point;
            QPointF p2 = poly.vertices[next].point;
            qreal bulge = poly.vertices[j].bulge;
            if (qAbs(bulge) < 1e-6) {
                // Line edge midpoint
                QPointF mid = getEdgeMidpoint(p1, p2);
                painter.drawEllipse(mid, 3 / m_scale, 3 / m_scale);
            } else {
                // Arc edge - show the point at maximum sagitta
                QPointF arcPoint = arcPointFromBulge(p1, p2, bulge);
                painter.setBrush(QColor(255, 150, 0));
                painter.drawEllipse(arcPoint, 3 / m_scale, 3 / m_scale);
                painter.setBrush(QColor(255, 255, 0));
            }
        }
    }
    painter.restore();

    // Reset transform for HUD
    painter.resetTransform();

    // HUD - 左下角显示缩放等级，右下角显示鼠标坐标
    painter.save();
    painter.setPen(QPen(QColor(200, 200, 200), 1));
    painter.setBrush(Qt::NoBrush);
    painter.setFont(QFont("Segoe UI", 9));

    // 左下角：缩放等级
    painter.drawText(10, height() - 10, QString("Scale: %1x").arg(m_scale, 0, 'f', 2));

    // 右下角：鼠标坐标
    if (!m_mousePos.isNull()) {
        QPointF worldPos = screenToWorld(m_mousePos);
        QString coordText = QString("X: %1  Y: %2").arg(worldPos.x(), 0, 'f', 2).arg(worldPos.y(), 0, 'f', 2);
        QRectF textRect(0, height() - 20, width() - 10, 15);
        painter.drawText(textRect, Qt::AlignRight | Qt::AlignBottom, coordText);
    }
    painter.restore();
}

void Sketch2DView::wheelEvent(QWheelEvent* event) {
    const qreal delta = event->angleDelta().y();
    const qreal zoomFactor = qPow(1.1, delta / 120.0);

    const qreal oldScale = m_scale;
    m_scale *= zoomFactor;
    m_scale = qBound(0.1, m_scale, 10.0);

    const QPointF screenCenter(width() / 2.0, height() / 2.0);
    const QPointF mousePos = event->position();

    const QPointF oldWorldPos = m_offset + (mousePos - screenCenter) / oldScale;
    const QPointF newWorldPos = m_offset + (mousePos - screenCenter) / m_scale;

    m_offset += oldWorldPos - newWorldPos;

    update();
    event->accept();
}

QRectF Sketch2DView::getWorldBounds() const {
    const QPointF topLeft = screenToWorld(QPointF(0, 0));
    const QPointF bottomRight = screenToWorld(QPointF(width(), height()));
    return QRectF(topLeft, bottomRight);
}

void Sketch2DView::setSelectedPolygon(int index, bool isPolygon) {
    if (m_selectedPolygonIndex != index) {
        m_selectedPolygonIndex = index;
        // 清除所有选择，然后添加当前选中的对象
        m_selectedPolygons.clear();
        m_selectedPolylines.clear();
        if (index >= 0) {
            if (isPolygon) {
                m_selectedPolygons.insert(index);
            } else {
                m_selectedPolylines.insert(index);
            }
        }
        emit selectionChanged(m_selectedPolygonIndex);
        update();
    }
}

void Sketch2DView::addSelectedPolygon(int index) {
    if (!m_selectedPolygons.contains(index)) {
        m_selectedPolygons.insert(index);
        update();
    }
}

void Sketch2DView::removeSelectedPolygon(int index) {
    if (m_selectedPolygons.contains(index)) {
        m_selectedPolygons.remove(index);
        update();
    }
}

void Sketch2DView::addSelectedPolyline(int index) {
    if (!m_selectedPolylines.contains(index)) {
        m_selectedPolylines.insert(index);
        update();
    }
}

void Sketch2DView::removeSelectedPolyline(int index) {
    if (m_selectedPolylines.contains(index)) {
        m_selectedPolylines.remove(index);
        update();
    }
}

void Sketch2DView::clearSelection() {
    if (m_selectedPolygonIndex != -1 || !m_selectedPolygons.isEmpty() || !m_selectedPolylines.isEmpty()) {
        m_selectedPolygonIndex = -1;
        m_selectedPolygons.clear();
        m_selectedPolylines.clear();
        emit selectionChanged(m_selectedPolygonIndex);
        update();
    }
}

void Sketch2DView::deletePolylines(const QSet<int>& indices) {
    if (indices.isEmpty()) {
        return;
    }

    // 按索引降序排序，从后往前删除
    QList<int> sortedIndices = indices.values();
    std::sort(sortedIndices.begin(), sortedIndices.end(), std::greater<int>());

    for (int index : sortedIndices) {
        if (index >= 0 && index < m_polylines.size()) {
            m_polylines.removeAt(index);
            emit polylineRemoved(index);
        }
    }

    // 清除被删除对象的选择
    for (int index : indices) {
        m_selectedPolylines.remove(index);
        m_clipPolylines.remove(index);
        m_subjectPolylines.remove(index);
        if (m_selectedPolygonIndex == index) {
            m_selectedPolygonIndex = -1;
        }
    }

    // 更新集合中的索引（由于删除了多段线，索引需要调整）
    QSet<int> updatedClipPolylines;
    QSet<int> updatedSubjectPolylines;
    QSet<int> updatedSelectedPolylines;

    for (int index : m_clipPolylines) {
        int newIndex = index - std::count_if(indices.begin(), indices.end(), [index](int deleted) { return deleted < index; });
        updatedClipPolylines.insert(newIndex);
    }
    for (int index : m_subjectPolylines) {
        int newIndex = index - std::count_if(indices.begin(), indices.end(), [index](int deleted) { return deleted < index; });
        updatedSubjectPolylines.insert(newIndex);
    }
    for (int index : m_selectedPolylines) {
        int newIndex = index - std::count_if(indices.begin(), indices.end(), [index](int deleted) { return deleted < index; });
        updatedSelectedPolylines.insert(newIndex);
    }

    m_clipPolylines = updatedClipPolylines;
    m_subjectPolylines = updatedSubjectPolylines;
    m_selectedPolylines = updatedSelectedPolylines;

    update();
}

void Sketch2DView::deletePolygons(const QSet<int>& indices) {
    if (indices.isEmpty()) {
        return;
    }

    // 按索引降序排序，从后往前删除
    QList<int> sortedIndices = indices.values();
    std::sort(sortedIndices.begin(), sortedIndices.end(), std::greater<int>());

    for (int index : sortedIndices) {
        if (index >= 0 && index < m_polygons.size()) {
            m_polygons.removeAt(index);
            emit polygonRemoved(index);
        }
    }

    // 清除被删除对象的选择
    for (int index : indices) {
        m_selectedPolygons.remove(index);
        m_clipPolygons.remove(index);
        m_subjectPolygons.remove(index);
        if (m_selectedPolygonIndex == index) {
            m_selectedPolygonIndex = -1;
        }
    }

    // 更新集合中的索引（由于删除了多边形，索引需要调整）
    QSet<int> updatedClipPolygons;
    QSet<int> updatedSubjectPolygons;
    QSet<int> updatedSelectedPolygons;

    int shift = indices.size();
    for (int index : m_clipPolygons) {
        int newIndex = index - std::count_if(indices.begin(), indices.end(), [index](int deleted) { return deleted < index; });
        updatedClipPolygons.insert(newIndex);
    }
    for (int index : m_subjectPolygons) {
        int newIndex = index - std::count_if(indices.begin(), indices.end(), [index](int deleted) { return deleted < index; });
        updatedSubjectPolygons.insert(newIndex);
    }
    for (int index : m_selectedPolygons) {
        int newIndex = index - std::count_if(indices.begin(), indices.end(), [index](int deleted) { return deleted < index; });
        updatedSelectedPolygons.insert(newIndex);
    }

    m_clipPolygons = updatedClipPolygons;
    m_subjectPolygons = updatedSubjectPolygons;
    m_selectedPolygons = updatedSelectedPolygons;

    update();
}

void Sketch2DView::setPolygonRole(int index, BooleanRole role, bool isPolygon) {
    // 先清除该对象在所有集合中的状态
    if (isPolygon) {
        m_clipPolygons.remove(index);
        m_subjectPolygons.remove(index);

        if (role == BooleanRole::Clip) {
            m_clipPolygons.insert(index);
        } else if (role == BooleanRole::Subject) {
            m_subjectPolygons.insert(index);
        }
    } else {
        m_clipPolylines.remove(index);
        m_subjectPolylines.remove(index);

        if (role == BooleanRole::Clip) {
            m_clipPolylines.insert(index);
        } else if (role == BooleanRole::Subject) {
            m_subjectPolylines.insert(index);
        }
    }
    update();
    repaint();  // 强制立即刷新视图
    emit polygonRoleChanged(index, isPolygon);  // 通知模型树更新颜色
}

void Sketch2DView::setPolygonRoleBatch(const QSet<int>& indices, BooleanRole role, bool isPolygon) {
    // 批量设置角色，只触发一次刷新
    if (isPolygon) {
        for (int index : indices) {
            m_clipPolygons.remove(index);
            m_subjectPolygons.remove(index);
            if (role == BooleanRole::Clip) {
                m_clipPolygons.insert(index);
            } else if (role == BooleanRole::Subject) {
                m_subjectPolygons.insert(index);
            }
        }
    } else {
        for (int index : indices) {
            m_clipPolylines.remove(index);
            m_subjectPolylines.remove(index);
            if (role == BooleanRole::Clip) {
                m_clipPolylines.insert(index);
            } else if (role == BooleanRole::Subject) {
                m_subjectPolylines.insert(index);
            }
        }
    }
    // 批量修改后，只刷新一次
    update();
    repaint();  // 强制立即刷新视图
    // 批量发射角色变更信号
    for (int index : indices) {
        emit polygonRoleChanged(index, isPolygon);
    }
}

Sketch2DView::BooleanRole Sketch2DView::polygonRole(int index, bool isPolygon) const {
    if (isPolygon) {
        if (m_clipPolygons.contains(index)) {
            return BooleanRole::Clip;
        } else if (m_subjectPolygons.contains(index)) {
            return BooleanRole::Subject;
        }
    } else {
        if (m_clipPolylines.contains(index)) {
            return BooleanRole::Clip;
        } else if (m_subjectPolylines.contains(index)) {
            return BooleanRole::Subject;
        }
    }
    return BooleanRole::None;
}

void Sketch2DView::setClipPolygons(const QSet<int>& indices) {
    m_clipPolygons = indices;
    update();
}

void Sketch2DView::setClipPolylines(const QSet<int>& indices) {
    m_clipPolylines = indices;
    update();
}

void Sketch2DView::setSubjectPolygons(const QSet<int>& indices) {
    m_subjectPolygons = indices;
    update();
}

void Sketch2DView::setSubjectPolylines(const QSet<int>& indices) {
    m_subjectPolylines = indices;
    update();
}

void Sketch2DView::setBooleanResults(const QVector<BooleanResultPolygon>& results) {
    m_booleanResults = results;
    update();
}

void Sketch2DView::setClipPatternResults(const QVector<BooleanResultPolygon>& results) {
    m_clipPatternResults = results;
    update();
}

void Sketch2DView::setSubjectPatternResults(const QVector<BooleanResultPolygon>& results) {
    m_subjectPatternResults = results;
    update();
}

void Sketch2DView::executeBooleanOperation(tailor_visualization::BooleanOperation operation, int fillType) {
    // 清空当前结果
    m_booleanResults.clear();

    // 创建布尔运算对象
    tailor_visualization::BooleanOperations booleanOp;

    // 转换 Clip 集合的多边形到 BooleanOperations 格式
    std::vector<std::vector<tailor_visualization::Arc>> clipArcs;
    for (int index : m_clipPolygons) {
        if (index >= 0 && index < m_polygons.size()) {
            const auto& poly = m_polygons[index];
            std::vector<tailor_visualization::Arc> arcs;
            for (int j = 0; j < poly.vertices.size(); ++j) {
                int next = (j + 1) % poly.vertices.size();
                const auto& v1 = poly.vertices[j];
                const auto& v2 = poly.vertices[next];

                // 使用顶点的edgeColor，如果为默认白色则使用蓝色
                QRgba64 edgeColor;
                if (v1.edgeColor == QColor(255, 255, 255)) {
                    edgeColor = QRgba64::fromRgba(0, 100, 255, 255);
                } else {
                    edgeColor = QRgba64::fromRgba(v1.edgeColor.red(), v1.edgeColor.green(),
                        v1.edgeColor.blue(), v1.edgeColor.alpha());
                }
                tailor_visualization::Arc arc(
                    tailor_visualization::ArcPoint(v1.point.x(), v1.point.y()),
                    tailor_visualization::ArcPoint(v2.point.x(), v2.point.y()),
                    v1.bulge,
                    edgeColor
                );
                arcs.push_back(arc);
            }
            clipArcs.push_back(arcs);
        }
    }
    // 批量添加 Clip 多边形
    booleanOp.AddClipPolygons(clipArcs);

    // 转换 Clip 集合的多段线为封闭形式 (ab, bc, cd, dc, cb, ba)
    std::vector<std::vector<tailor_visualization::Arc>> clipPolylineArcs;
    for (int index : m_clipPolylines) {
        if (index >= 0 && index < m_polylines.size()) {
            const auto& polyline = m_polylines[index];
            std::vector<tailor_visualization::Arc> arcs;
            int n = polyline.vertices.size();
            if (n < 2) continue;

            // 正向边: v0->v1, v1->v2, ..., v(n-2)->v(n-1)
            for (int j = 0; j < n - 1; ++j) {
                const auto& v1 = polyline.vertices[j];
                const auto& v2 = polyline.vertices[j + 1];
                QRgba64 edgeColor = QRgba64::fromRgba(0, 100, 255, 255);
                tailor_visualization::Arc arc(
                    tailor_visualization::ArcPoint(v1.point.x(), v1.point.y()),
                    tailor_visualization::ArcPoint(v2.point.x(), v2.point.y()),
                    v1.bulge,
                    edgeColor
                );
                arcs.push_back(arc);
            }
            // 反向边: v(n-1)->v(n-2), v(n-2)->v(n-3), ..., v1->v0 (凸度取反)
            for (int j = n - 1; j > 0; --j) {
                const auto& v1 = polyline.vertices[j];
                const auto& v2 = polyline.vertices[j - 1];
                QRgba64 edgeColor = QRgba64::fromRgba(0, 100, 255, 255);
                tailor_visualization::Arc arc(
                    tailor_visualization::ArcPoint(v1.point.x(), v1.point.y()),
                    tailor_visualization::ArcPoint(v2.point.x(), v2.point.y()),
                    -v2.bulge,  // 反向边凸度取反（使用前一顶点的凸度）
                    edgeColor
                );
                arcs.push_back(arc);
            }
            if (!arcs.empty()) {
                clipPolylineArcs.push_back(arcs);
            }
        }
    }
    // 批量添加 Clip 多段线（封闭形式）
    booleanOp.AddClipPolygons(clipPolylineArcs);

    // 转换 Subject 集合的多边形到 BooleanOperations 格式
    std::vector<std::vector<tailor_visualization::Arc>> subjectArcs;
    for (int index : m_subjectPolygons) {
        if (index >= 0 && index < m_polygons.size()) {
            const auto& poly = m_polygons[index];
            std::vector<tailor_visualization::Arc> arcs;
            for (int j = 0; j < poly.vertices.size(); ++j) {
                int next = (j + 1) % poly.vertices.size();
                const auto& v1 = poly.vertices[j];
                const auto& v2 = poly.vertices[next];

                // 使用顶点的edgeColor，如果为默认白色则使用红色
                QRgba64 edgeColor;
                if (v1.edgeColor == QColor(255, 255, 255)) {
                    edgeColor = QRgba64::fromRgba(255, 50, 50, 255);
                } else {
                    edgeColor = QRgba64::fromRgba(v1.edgeColor.red(), v1.edgeColor.green(),
                        v1.edgeColor.blue(), v1.edgeColor.alpha());
                }
                tailor_visualization::Arc arc(
                    tailor_visualization::ArcPoint(v1.point.x(), v1.point.y()),
                    tailor_visualization::ArcPoint(v2.point.x(), v2.point.y()),
                    v1.bulge,
                    edgeColor
                );
                arcs.push_back(arc);
            }
            subjectArcs.push_back(arcs);
        }
    }
    // 批量添加 Subject 多边形
    booleanOp.AddSubjectPolygons(subjectArcs);

    // 转换 Subject 集合的多段线为封闭形式 (ab, bc, cd, dc, cb, ba)
    std::vector<std::vector<tailor_visualization::Arc>> subjectPolylineArcs;
    for (int index : m_subjectPolylines) {
        if (index >= 0 && index < m_polylines.size()) {
            const auto& polyline = m_polylines[index];
            std::vector<tailor_visualization::Arc> arcs;
            int n = polyline.vertices.size();
            if (n < 2) continue;

            // 正向边: v0->v1, v1->v2, ..., v(n-2)->v(n-1)
            for (int j = 0; j < n - 1; ++j) {
                const auto& v1 = polyline.vertices[j];
                const auto& v2 = polyline.vertices[j + 1];
                QRgba64 edgeColor = QRgba64::fromRgba(255, 50, 50, 255);
                tailor_visualization::Arc arc(
                    tailor_visualization::ArcPoint(v1.point.x(), v1.point.y()),
                    tailor_visualization::ArcPoint(v2.point.x(), v2.point.y()),
                    v1.bulge,
                    edgeColor
                );
                arcs.push_back(arc);
            }
            // 反向边: v(n-1)->v(n-2), v(n-2)->v(n-3), ..., v1->v0 (凸度取反)
            for (int j = n - 1; j > 0; --j) {
                const auto& v1 = polyline.vertices[j];
                const auto& v2 = polyline.vertices[j - 1];
                QRgba64 edgeColor = QRgba64::fromRgba(255, 50, 50, 255);
                tailor_visualization::Arc arc(
                    tailor_visualization::ArcPoint(v1.point.x(), v1.point.y()),
                    tailor_visualization::ArcPoint(v2.point.x(), v2.point.y()),
                    -v2.bulge,  // 反向边凸度取反（使用前一顶点的凸度）
                    edgeColor
                );
                arcs.push_back(arc);
            }
            if (!arcs.empty()) {
                subjectPolylineArcs.push_back(arcs);
            }
        }
    }
    // 批量添加 Subject 多段线（封闭形式）
    booleanOp.AddSubjectPolygons(subjectPolylineArcs);

    // 生成不同的颜色用于每个结果多边形
    std::vector<QColor> colorPalette = {
        QColor(255, 100, 100, 150),  // 红色半透明
        QColor(100, 255, 100, 150),  // 绿色半透明
        QColor(100, 100, 255, 150),  // 蓝色半透明
        QColor(255, 255, 100, 150),  // 黄色半透明
        QColor(255, 100, 255, 150),  // 品红半透明
        QColor(100, 255, 255, 150),  // 青色半透明
        QColor(255, 128, 0, 150),    // 橙色半透明
        QColor(128, 0, 255, 150),    // 紫色半透明
        QColor(255, 192, 203, 150),  // 粉色半透明
        QColor(0, 255, 127, 150)     // 春绿半透明
    };

    // 执行布尔运算并获取结果（使用指定的 fillType）
    std::vector<std::vector<tailor_visualization::Arc>> resultPolygons = booleanOp.Execute(operation, fillType);

    // 转换结果为 BooleanResultPolygon
    for (size_t i = 0; i < resultPolygons.size(); ++i) {
        const auto& resultArcs = resultPolygons[i];
        BooleanResultPolygon resultPoly;
        resultPoly.color = colorPalette[i % colorPalette.size()];

        // 转换每个边到顶点
        for (const auto& edge : resultArcs) {
            const auto& start = edge.Point0();
            double bulge = edge.Bulge();
            QRgba64 colorData = edge.Data();

            PolygonVertex vertex;
            vertex.point = QPointF(start.x, start.y);
            vertex.bulge = bulge;
            // 从Arc的颜色数据中提取颜色
            QColor edgeColor = QColor::fromRgba64(colorData);
            vertex.edgeColor = edgeColor;
            resultPoly.vertices.append(vertex);
            resultPoly.edgeColors.append(edgeColor);
        }

        m_booleanResults.append(resultPoly);
    }

    // 触发视图更新以显示布尔运算结果
    update();
}

void Sketch2DView::executeBooleanOperation(
    tailor_visualization::BooleanOperation operation,
    const tailor_visualization::IFillType* clipFillType,
    const tailor_visualization::IFillType* subjectFillType) {

    // 清空当前结果
    m_booleanResults.clear();

    // 创建布尔运算对象
    tailor_visualization::BooleanOperations booleanOp;

    // 转换 Clip 集合的多边形到 BooleanOperations 格式
    std::vector<std::vector<tailor_visualization::Arc>> clipArcs;
    for (int index : m_clipPolygons) {
        if (index >= 0 && index < m_polygons.size()) {
            const auto& poly = m_polygons[index];
            std::vector<tailor_visualization::Arc> arcs;
            for (int j = 0; j < poly.vertices.size(); ++j) {
                int next = (j + 1) % poly.vertices.size();
                const auto& v1 = poly.vertices[j];
                const auto& v2 = poly.vertices[next];

                // 使用顶点的edgeColor，如果为默认白色则使用蓝色
                QRgba64 edgeColor;
                if (v1.edgeColor == QColor(255, 255, 255)) {
                    edgeColor = QRgba64::fromRgba(0, 100, 255, 255);
                } else {
                    edgeColor = QRgba64::fromRgba(v1.edgeColor.red(), v1.edgeColor.green(),
                        v1.edgeColor.blue(), v1.edgeColor.alpha());
                }
                tailor_visualization::Arc arc(
                    tailor_visualization::ArcPoint(v1.point.x(), v1.point.y()),
                    tailor_visualization::ArcPoint(v2.point.x(), v2.point.y()),
                    v1.bulge,
                    edgeColor
                );
                arcs.push_back(arc);
            }
            clipArcs.push_back(arcs);
        }
    }
    // 批量添加 Clip 多边形
    booleanOp.AddClipPolygons(clipArcs);

    // 转换 Clip 集合的多段线为封闭形式 (ab, bc, cd, dc, cb, ba)
    std::vector<std::vector<tailor_visualization::Arc>> clipPolylineArcs;
    for (int index : m_clipPolylines) {
        if (index >= 0 && index < m_polylines.size()) {
            const auto& polyline = m_polylines[index];
            std::vector<tailor_visualization::Arc> arcs;
            int n = polyline.vertices.size();
            if (n < 2) continue;
            for (int j = 0; j < n - 1; ++j) {
                const auto& v1 = polyline.vertices[j];
                const auto& v2 = polyline.vertices[j + 1];
                QRgba64 edgeColor = QRgba64::fromRgba(0, 100, 255, 255);
                tailor_visualization::Arc arc(
                    tailor_visualization::ArcPoint(v1.point.x(), v1.point.y()),
                    tailor_visualization::ArcPoint(v2.point.x(), v2.point.y()),
                    v1.bulge, edgeColor);
                arcs.push_back(arc);
            }
            for (int j = n - 1; j > 0; --j) {
                const auto& v1 = polyline.vertices[j];
                const auto& v2 = polyline.vertices[j - 1];
                QRgba64 edgeColor = QRgba64::fromRgba(0, 100, 255, 255);
                tailor_visualization::Arc arc(
                    tailor_visualization::ArcPoint(v1.point.x(), v1.point.y()),
                    tailor_visualization::ArcPoint(v2.point.x(), v2.point.y()),
                    -v2.bulge, edgeColor);  // 反向边凸度取反
                arcs.push_back(arc);
            }
            if (!arcs.empty()) clipPolylineArcs.push_back(arcs);
        }
    }
    booleanOp.AddClipPolygons(clipPolylineArcs);

    // 转换 Subject 集合的多边形到 BooleanOperations 格式
    std::vector<std::vector<tailor_visualization::Arc>> subjectArcs;
    for (int index : m_subjectPolygons) {
        if (index >= 0 && index < m_polygons.size()) {
            const auto& poly = m_polygons[index];
            std::vector<tailor_visualization::Arc> arcs;
            for (int j = 0; j < poly.vertices.size(); ++j) {
                int next = (j + 1) % poly.vertices.size();
                const auto& v1 = poly.vertices[j];
                const auto& v2 = poly.vertices[next];

                // 使用顶点的edgeColor，如果为默认白色则使用红色
                QRgba64 edgeColor;
                if (v1.edgeColor == QColor(255, 255, 255)) {
                    edgeColor = QRgba64::fromRgba(255, 50, 50, 255);
                } else {
                    edgeColor = QRgba64::fromRgba(v1.edgeColor.red(), v1.edgeColor.green(),
                        v1.edgeColor.blue(), v1.edgeColor.alpha());
                }
                tailor_visualization::Arc arc(
                    tailor_visualization::ArcPoint(v1.point.x(), v1.point.y()),
                    tailor_visualization::ArcPoint(v2.point.x(), v2.point.y()),
                    v1.bulge,
                    edgeColor
                );
                arcs.push_back(arc);
            }
            subjectArcs.push_back(arcs);
        }
    }
    // 批量添加 Subject 多边形
    booleanOp.AddSubjectPolygons(subjectArcs);

    // 转换 Subject 集合的多段线为封闭形式
    std::vector<std::vector<tailor_visualization::Arc>> subjectPolylineArcs;
    for (int index : m_subjectPolylines) {
        if (index >= 0 && index < m_polylines.size()) {
            const auto& polyline = m_polylines[index];
            std::vector<tailor_visualization::Arc> arcs;
            int n = polyline.vertices.size();
            if (n < 2) continue;
            for (int j = 0; j < n - 1; ++j) {
                const auto& v1 = polyline.vertices[j];
                const auto& v2 = polyline.vertices[j + 1];
                QRgba64 edgeColor = QRgba64::fromRgba(255, 50, 50, 255);
                tailor_visualization::Arc arc(
                    tailor_visualization::ArcPoint(v1.point.x(), v1.point.y()),
                    tailor_visualization::ArcPoint(v2.point.x(), v2.point.y()),
                    v1.bulge, edgeColor);
                arcs.push_back(arc);
            }
            for (int j = n - 1; j > 0; --j) {
                const auto& v1 = polyline.vertices[j];
                const auto& v2 = polyline.vertices[j - 1];
                QRgba64 edgeColor = QRgba64::fromRgba(255, 50, 50, 255);
                tailor_visualization::Arc arc(
                    tailor_visualization::ArcPoint(v1.point.x(), v1.point.y()),
                    tailor_visualization::ArcPoint(v2.point.x(), v2.point.y()),
                    -v2.bulge, edgeColor);  // 反向边凸度取反
                arcs.push_back(arc);
            }
            if (!arcs.empty()) subjectPolylineArcs.push_back(arcs);
        }
    }
    booleanOp.AddSubjectPolygons(subjectPolylineArcs);

    // 生成不同的颜色用于每个结果多边形
    std::vector<QColor> colorPalette = {
        QColor(255, 100, 100, 150),  // 红色半透明
        QColor(100, 255, 100, 150),  // 绿色半透明
        QColor(100, 100, 255, 150),  // 蓝色半透明
        QColor(255, 255, 100, 150),  // 黄色半透明
        QColor(255, 100, 255, 150),  // 品红半透明
        QColor(100, 255, 255, 150),  // 青色半透明
        QColor(255, 128, 0, 150),    // 橙色半透明
        QColor(128, 0, 255, 150),    // 紫色半透明
        QColor(255, 192, 203, 150),  // 粉色半透明
        QColor(0, 255, 127, 150)     // 春绿半透明
    };

    // 执行布尔运算并获取结果（使用指定的 FillType 指针）
    std::vector<std::vector<tailor_visualization::Arc>> resultPolygons = booleanOp.Execute(operation, clipFillType, subjectFillType);

    // 转换结果为 BooleanResultPolygon
    for (size_t i = 0; i < resultPolygons.size(); ++i) {
        const auto& resultArcs = resultPolygons[i];
        BooleanResultPolygon resultPoly;
        resultPoly.color = colorPalette[i % colorPalette.size()];

        // 转换每个边到顶点
        for (const auto& edge : resultArcs) {
            const auto& start = edge.Point0();
            double bulge = edge.Bulge();
            QRgba64 colorData = edge.Data();

            PolygonVertex vertex;
            vertex.point = QPointF(start.x, start.y);
            vertex.bulge = bulge;
            // 从Arc的颜色数据中提取颜色
            QColor edgeColor = QColor::fromRgba64(colorData);
            vertex.edgeColor = edgeColor;
            resultPoly.vertices.append(vertex);
            resultPoly.edgeColors.append(edgeColor);
        }

        m_booleanResults.append(resultPoly);
    }

    // 触发视图更新以显示布尔运算结果
    update();
}

// 第二个executeBooleanOperation函数结束

void Sketch2DView::executeBooleanOperation(
    tailor_visualization::BooleanOperation operation,
    const tailor_visualization::IFillType* clipFillType,
    const tailor_visualization::IFillType* subjectFillType,
    const tailor_visualization::IConnectType<ConnectTypeDrafting>* connectType) {
    // 清空当前结果
    m_booleanResults.clear();

    // 创建布尔运算对象
    tailor_visualization::BooleanOperations booleanOp;

    // 转换 Clip 集合的多边形到 BooleanOperations 格式
    std::vector<std::vector<tailor_visualization::Arc>> clipArcs;
    for (int index : m_clipPolygons) {
        if (index >= 0 && index < m_polygons.size()) {
            const auto& poly = m_polygons[index];
            std::vector<tailor_visualization::Arc> arcs;
            for (int j = 0; j < poly.vertices.size(); ++j) {
                int next = (j + 1) % poly.vertices.size();
                const auto& v1 = poly.vertices[j];
                const auto& v2 = poly.vertices[next];

                // 使用顶点的edgeColor，如果为默认白色则使用蓝色
                QRgba64 edgeColor = (v1.edgeColor == QColor(255, 255, 255))
                    ? QRgba64::fromRgba(0, 100, 255, 255)
                    : QColorToQRgba64(v1.edgeColor);
                tailor_visualization::Arc arc(
                    tailor_visualization::ArcPoint(v1.point.x(), v1.point.y()),
                    tailor_visualization::ArcPoint(v2.point.x(), v2.point.y()),
                    v1.bulge,
                    edgeColor
                );
                arcs.push_back(arc);
            }
            clipArcs.push_back(arcs);
        }
    }
    // 批量添加 Clip 多边形
    booleanOp.AddClipPolygons(clipArcs);

    // 转换 Clip 集合的多段线为封闭形式 (ab, bc, cd, dc, cb, ba)
    std::vector<std::vector<tailor_visualization::Arc>> clipPolylineArcs;
    for (int index : m_clipPolylines) {
        if (index >= 0 && index < m_polylines.size()) {
            const auto& polyline = m_polylines[index];
            std::vector<tailor_visualization::Arc> arcs;
            int n = polyline.vertices.size();
            if (n < 2) continue;
            for (int j = 0; j < n - 1; ++j) {
                const auto& v1 = polyline.vertices[j];
                const auto& v2 = polyline.vertices[j + 1];
                QRgba64 edgeColor = QRgba64::fromRgba(0, 100, 255, 255);
                tailor_visualization::Arc arc(
                    tailor_visualization::ArcPoint(v1.point.x(), v1.point.y()),
                    tailor_visualization::ArcPoint(v2.point.x(), v2.point.y()),
                    v1.bulge, edgeColor);
                arcs.push_back(arc);
            }
            for (int j = n - 1; j > 0; --j) {
                const auto& v1 = polyline.vertices[j];
                const auto& v2 = polyline.vertices[j - 1];
                QRgba64 edgeColor = QRgba64::fromRgba(0, 100, 255, 255);
                tailor_visualization::Arc arc(
                    tailor_visualization::ArcPoint(v1.point.x(), v1.point.y()),
                    tailor_visualization::ArcPoint(v2.point.x(), v2.point.y()),
                    -v2.bulge, edgeColor);  // 反向边凸度取反
                arcs.push_back(arc);
            }
            if (!arcs.empty()) clipPolylineArcs.push_back(arcs);
        }
    }
    booleanOp.AddClipPolygons(clipPolylineArcs);

    // 转换 Subject 集合的多边形到 BooleanOperations 格式
    std::vector<std::vector<tailor_visualization::Arc>> subjectArcs;
    for (int index : m_subjectPolygons) {
        if (index >= 0 && index < m_polygons.size()) {
            const auto& poly = m_polygons[index];
            std::vector<tailor_visualization::Arc> arcs;
            for (int j = 0; j < poly.vertices.size(); ++j) {
                int next = (j + 1) % poly.vertices.size();
                const auto& v1 = poly.vertices[j];
                const auto& v2 = poly.vertices[next];

                // 使用顶点的edgeColor，如果为默认白色则使用红色
                QRgba64 edgeColor = (v1.edgeColor == QColor(255, 255, 255))
                    ? QRgba64::fromRgba(255, 50, 50, 255)
                    : QColorToQRgba64(v1.edgeColor);
                tailor_visualization::Arc arc(
                    tailor_visualization::ArcPoint(v1.point.x(), v1.point.y()),
                    tailor_visualization::ArcPoint(v2.point.x(), v2.point.y()),
                    v1.bulge,
                    edgeColor
                );
                arcs.push_back(arc);
            }
            subjectArcs.push_back(arcs);
        }
    }
    // 批量添加 Subject 多边形
    booleanOp.AddSubjectPolygons(subjectArcs);

    // 转换 Subject 集合的多段线为封闭形式
    std::vector<std::vector<tailor_visualization::Arc>> subjectPolylineArcs;
    for (int index : m_subjectPolylines) {
        if (index >= 0 && index < m_polylines.size()) {
            const auto& polyline = m_polylines[index];
            std::vector<tailor_visualization::Arc> arcs;
            int n = polyline.vertices.size();
            if (n < 2) continue;
            for (int j = 0; j < n - 1; ++j) {
                const auto& v1 = polyline.vertices[j];
                const auto& v2 = polyline.vertices[j + 1];
                QRgba64 edgeColor = QRgba64::fromRgba(255, 50, 50, 255);
                tailor_visualization::Arc arc(
                    tailor_visualization::ArcPoint(v1.point.x(), v1.point.y()),
                    tailor_visualization::ArcPoint(v2.point.x(), v2.point.y()),
                    v1.bulge, edgeColor);
                arcs.push_back(arc);
            }
            for (int j = n - 1; j > 0; --j) {
                const auto& v1 = polyline.vertices[j];
                const auto& v2 = polyline.vertices[j - 1];
                QRgba64 edgeColor = QRgba64::fromRgba(255, 50, 50, 255);
                tailor_visualization::Arc arc(
                    tailor_visualization::ArcPoint(v1.point.x(), v1.point.y()),
                    tailor_visualization::ArcPoint(v2.point.x(), v2.point.y()),
                    -v2.bulge, edgeColor);  // 反向边凸度取反
                arcs.push_back(arc);
            }
            if (!arcs.empty()) subjectPolylineArcs.push_back(arcs);
        }
    }
    booleanOp.AddSubjectPolygons(subjectPolylineArcs);

    // 生成不同的颜色用于每个结果多边形
    std::vector<QColor> colorPalette = {
        QColor(255, 100, 100, 150),  // 红色半透明
        QColor(100, 255, 100, 150),  // 绿色半透明
        QColor(100, 100, 255, 150),  // 蓝色半透明
        QColor(255, 255, 100, 150),  // 黄色半透明
        QColor(255, 100, 255, 150),  // 品红半透明
        QColor(100, 255, 255, 150),  // 青色半透明
        QColor(255, 128, 0, 150),    // 橙色半透明
        QColor(128, 0, 255, 150),    // 紫色半透明
        QColor(255, 192, 203, 150),  // 粉色半透明
        QColor(0, 255, 127, 150)     // 春绿半透明
    };

    // 执行布尔运算并获取结果（使用指定的 FillType 指针和 ConnectType 指针，带内环信息）
    auto resultPolygonsWithHoles = booleanOp.ExecuteWithHoles(operation, clipFillType, subjectFillType, connectType);

    // 转换结果为 BooleanResultPolygon
    for (size_t i = 0; i < resultPolygonsWithHoles.size(); ++i) {
        const auto& polyWithHole = resultPolygonsWithHoles[i];
        const auto& resultArcs = polyWithHole.vertices;
        BooleanResultPolygon resultPoly;
        for (const auto& arc : resultArcs) {
            PolygonVertex v;
            v.point = QPointF(arc.Point0().x, arc.Point0().y);
            v.bulge = arc.Bulge();
            // 从Arc的颜色数据中提取颜色
            QColor edgeColor = QColor::fromRgba64(arc.Data());
            v.edgeColor = edgeColor;
            resultPoly.vertices.push_back(v);
            resultPoly.edgeColors.append(edgeColor);
        }
        resultPoly.color = colorPalette[i % colorPalette.size()];
        resultPoly.isHole = polyWithHole.isHole;  // 设置内环标记
        m_booleanResults.push_back(resultPoly);
    }
    qDebug() << "Boolean result count:" << m_booleanResults.size();
}

void Sketch2DView::executeClipPattern(const tailor_visualization::IFillType* fillType) {
    // 清空当前结果
    m_clipPatternResults.clear();

    // 调试：导出输入多边形到单独文件
    // debugExportPolygons("D://debug_clip_polygons.txt", m_clipPolygons, "Clip");

    // 创建布尔运算对象
    tailor_visualization::BooleanOperations booleanOp;

    // 转换 Clip 集合的多边形到 BooleanOperations 格式
    std::vector<std::vector<tailor_visualization::Arc>> clipArcs;
    for (int index : m_clipPolygons) {
        if (index >= 0 && index < m_polygons.size()) {
            const auto& poly = m_polygons[index];
            std::vector<tailor_visualization::Arc> arcs;
            for (int j = 0; j < poly.vertices.size(); ++j) {
                int next = (j + 1) % poly.vertices.size();
                const auto& v1 = poly.vertices[j];
                const auto& v2 = poly.vertices[next];

                // 使用顶点的edgeColor，如果为默认白色则使用蓝色
                QRgba64 edgeColor = (v1.edgeColor == QColor(255, 255, 255))
                    ? QRgba64::fromRgba(0, 100, 255, 255)
                    : QColorToQRgba64(v1.edgeColor);
                tailor_visualization::Arc arc(
                    tailor_visualization::ArcPoint(v1.point.x(), v1.point.y()),
                    tailor_visualization::ArcPoint(v2.point.x(), v2.point.y()),
                    v1.bulge,
                    edgeColor
                );
                arcs.push_back(arc);
            }
            clipArcs.push_back(arcs);
        }
    }
    // 批量添加 Clip 多边形
    booleanOp.AddClipPolygons(clipArcs);

    // 转换 Clip 集合的多段线为封闭形式 (ab, bc, cd, dc, cb, ba)
    std::vector<std::vector<tailor_visualization::Arc>> clipPolylineArcs;
    for (int index : m_clipPolylines) {
        if (index >= 0 && index < m_polylines.size()) {
            const auto& polyline = m_polylines[index];
            std::vector<tailor_visualization::Arc> arcs;
            int n = polyline.vertices.size();
            if (n < 2) continue;
            for (int j = 0; j < n - 1; ++j) {
                const auto& v1 = polyline.vertices[j];
                const auto& v2 = polyline.vertices[j + 1];
                QRgba64 edgeColor = QRgba64::fromRgba(0, 100, 255, 255);
                tailor_visualization::Arc arc(
                    tailor_visualization::ArcPoint(v1.point.x(), v1.point.y()),
                    tailor_visualization::ArcPoint(v2.point.x(), v2.point.y()),
                    v1.bulge, edgeColor);
                arcs.push_back(arc);
            }
            for (int j = n - 1; j > 0; --j) {
                const auto& v1 = polyline.vertices[j];
                const auto& v2 = polyline.vertices[j - 1];
                QRgba64 edgeColor = QRgba64::fromRgba(0, 100, 255, 255);
                tailor_visualization::Arc arc(
                    tailor_visualization::ArcPoint(v1.point.x(), v1.point.y()),
                    tailor_visualization::ArcPoint(v2.point.x(), v2.point.y()),
                    -v2.bulge, edgeColor);  // 反向边凸度取反
                arcs.push_back(arc);
            }
            if (!arcs.empty()) clipPolylineArcs.push_back(arcs);
        }
    }
    booleanOp.AddClipPolygons(clipPolylineArcs);

    // 生成不同的颜色用于每个结果多边形（蓝色系）
    std::vector<QColor> colorPalette = {
        QColor(0, 100, 255, 200),     // 蓝色
        QColor(50, 150, 255, 200),     // 浅蓝色
        QColor(100, 200, 255, 200),    // 更浅的蓝色
        QColor(0, 150, 200, 200),      // 青蓝色
        QColor(0, 50, 255, 200)        // 深蓝色
    };

    // 执行 OnlyClipPatternWithHoles (使用指定的 fillType 参数，获取内环信息)
    auto resultPolygonsWithHoles = booleanOp.ExecuteOnlyClipPatternWithHoles(fillType, nullptr);

    // 转换结果为 BooleanResultPolygon
    for (size_t i = 0; i < resultPolygonsWithHoles.size(); ++i) {
        const auto& polyWithHole = resultPolygonsWithHoles[i];
        const auto& resultArcs = polyWithHole.vertices;
        BooleanResultPolygon resultPoly;
        resultPoly.color = colorPalette[i % colorPalette.size()];
        resultPoly.isHole = polyWithHole.isHole;  // 设置内环标记

        // 转换每个边到顶点
        for (const auto& edge : resultArcs) {
            const auto& start = edge.Point0();
            double bulge = edge.Bulge();
            QRgba64 colorData = edge.Data();

            PolygonVertex vertex;
            vertex.point = QPointF(start.x, start.y);
            vertex.bulge = bulge;
            // 从Arc的颜色数据中提取颜色
            QColor edgeColor = QColor::fromRgba64(colorData);
            vertex.edgeColor = edgeColor;
            resultPoly.vertices.append(vertex);
            resultPoly.edgeColors.append(edgeColor);
        }

        m_clipPatternResults.append(resultPoly);
    }

    // 触发视图更新
    update();
}

void Sketch2DView::executeSubjectPattern(const tailor_visualization::IFillType* fillType) {
    // 清空当前结果
    m_subjectPatternResults.clear();

    // 调试：导出输入多边形到单独文件
    // debugExportPolygons("D://debug_subject_polygons.txt", m_subjectPolygons, "Subject");

    // 创建布尔运算对象
    tailor_visualization::BooleanOperations booleanOp;

    // 转换 Subject 集合的多边形到 BooleanOperations 格式
    std::vector<std::vector<tailor_visualization::Arc>> subjectArcs;
    for (int index : m_subjectPolygons) {
        if (index >= 0 && index < m_polygons.size()) {
            const auto& poly = m_polygons[index];
            std::vector<tailor_visualization::Arc> arcs;
            for (int j = 0; j < poly.vertices.size(); ++j) {
                int next = (j + 1) % poly.vertices.size();
                const auto& v1 = poly.vertices[j];
                const auto& v2 = poly.vertices[next];

                // 使用顶点的edgeColor，如果为默认白色则使用红色
                QRgba64 edgeColor = (v1.edgeColor == QColor(255, 255, 255))
                    ? QRgba64::fromRgba(255, 50, 50, 255)
                    : QColorToQRgba64(v1.edgeColor);
                tailor_visualization::Arc arc(
                    tailor_visualization::ArcPoint(v1.point.x(), v1.point.y()),
                    tailor_visualization::ArcPoint(v2.point.x(), v2.point.y()),
                    v1.bulge,
                    edgeColor
                );
                arcs.push_back(arc);
            }
            subjectArcs.push_back(arcs);
        }
    }
    // 批量添加 Subject 多边形
    booleanOp.AddSubjectPolygons(subjectArcs);

    // 转换 Subject 集合的多段线为封闭形式
    std::vector<std::vector<tailor_visualization::Arc>> subjectPolylineArcs;
    for (int index : m_subjectPolylines) {
        if (index >= 0 && index < m_polylines.size()) {
            const auto& polyline = m_polylines[index];
            std::vector<tailor_visualization::Arc> arcs;
            int n = polyline.vertices.size();
            if (n < 2) continue;
            for (int j = 0; j < n - 1; ++j) {
                const auto& v1 = polyline.vertices[j];
                const auto& v2 = polyline.vertices[j + 1];
                QRgba64 edgeColor = QRgba64::fromRgba(255, 50, 50, 255);
                tailor_visualization::Arc arc(
                    tailor_visualization::ArcPoint(v1.point.x(), v1.point.y()),
                    tailor_visualization::ArcPoint(v2.point.x(), v2.point.y()),
                    v1.bulge, edgeColor);
                arcs.push_back(arc);
            }
            for (int j = n - 1; j > 0; --j) {
                const auto& v1 = polyline.vertices[j];
                const auto& v2 = polyline.vertices[j - 1];
                QRgba64 edgeColor = QRgba64::fromRgba(255, 50, 50, 255);
                tailor_visualization::Arc arc(
                    tailor_visualization::ArcPoint(v1.point.x(), v1.point.y()),
                    tailor_visualization::ArcPoint(v2.point.x(), v2.point.y()),
                    -v2.bulge, edgeColor);  // 反向边凸度取反
                arcs.push_back(arc);
            }
            if (!arcs.empty()) subjectPolylineArcs.push_back(arcs);
        }
    }
    booleanOp.AddSubjectPolygons(subjectPolylineArcs);

    // 生成不同的颜色用于每个结果多边形（红色系）
    std::vector<QColor> colorPalette = {
        QColor(255, 50, 50, 200),     // 红色
        QColor(255, 100, 100, 200),   // 浅红色
        QColor(255, 150, 150, 200),   // 更浅的红色
        QColor(255, 0, 100, 200),    // 品红色
        QColor(200, 0, 50, 200)      // 深红色
    };

    // 执行 OnlySubjectPatternWithHoles (使用指定的 fillType 参数，获取内环信息)
    auto resultPolygonsWithHoles = booleanOp.ExecuteOnlySubjectPatternWithHoles(fillType, nullptr);

    // 转换结果为 BooleanResultPolygon
    for (size_t i = 0; i < resultPolygonsWithHoles.size(); ++i) {
        const auto& polyWithHole = resultPolygonsWithHoles[i];
        const auto& resultArcs = polyWithHole.vertices;
        BooleanResultPolygon resultPoly;
        resultPoly.color = colorPalette[i % colorPalette.size()];
        resultPoly.isHole = polyWithHole.isHole;  // 设置内环标记

        // 转换每个边到顶点
        for (const auto& edge : resultArcs) {
            const auto& start = edge.Point0();
            double bulge = edge.Bulge();
            QRgba64 colorData = edge.Data();

            PolygonVertex vertex;
            vertex.point = QPointF(start.x, start.y);
            vertex.bulge = bulge;
            // 从Arc的颜色数据中提取颜色
            QColor edgeColor = QColor::fromRgba64(colorData);
            vertex.edgeColor = edgeColor;
            resultPoly.vertices.append(vertex);
            resultPoly.edgeColors.append(edgeColor);
        }

        m_subjectPatternResults.append(resultPoly);
    }

    // 触发视图更新
    update();
}

void Sketch2DView::executeClipPattern(
    const tailor_visualization::IFillType* fillType,
    const tailor_visualization::IConnectType<ConnectTypeDrafting>* connectType) {
    // 清空当前结果
    m_clipPatternResults.clear();

    // 创建布尔运算对象
    tailor_visualization::BooleanOperations booleanOp;

    // 转换 Clip 集合的多边形到 BooleanOperations 格式
    std::vector<std::vector<tailor_visualization::Arc>> clipArcs;
    for (int index : m_clipPolygons) {
        if (index >= 0 && index < m_polygons.size()) {
            const auto& poly = m_polygons[index];
            std::vector<tailor_visualization::Arc> arcs;
            for (int j = 0; j < poly.vertices.size(); ++j) {
                int next = (j + 1) % poly.vertices.size();
                const auto& v1 = poly.vertices[j];
                const auto& v2 = poly.vertices[next];

                // 使用顶点的edgeColor，如果为默认白色则使用蓝色
                QRgba64 edgeColor = (v1.edgeColor == QColor(255, 255, 255))
                    ? QRgba64::fromRgba(0, 100, 255, 255)
                    : QColorToQRgba64(v1.edgeColor);
                tailor_visualization::Arc arc(
                    tailor_visualization::ArcPoint(v1.point.x(), v1.point.y()),
                    tailor_visualization::ArcPoint(v2.point.x(), v2.point.y()),
                    v1.bulge,
                    edgeColor
                );
                arcs.push_back(arc);
            }
            clipArcs.push_back(arcs);
        }
    }
    // 批量添加 Clip 多边形
    booleanOp.AddClipPolygons(clipArcs);

    // 转换 Clip 集合的多段线为封闭形式 (ab, bc, cd, dc, cb, ba)
    std::vector<std::vector<tailor_visualization::Arc>> clipPolylineArcs;
    for (int index : m_clipPolylines) {
        if (index >= 0 && index < m_polylines.size()) {
            const auto& polyline = m_polylines[index];
            std::vector<tailor_visualization::Arc> arcs;
            int n = polyline.vertices.size();
            if (n < 2) continue;
            for (int j = 0; j < n - 1; ++j) {
                const auto& v1 = polyline.vertices[j];
                const auto& v2 = polyline.vertices[j + 1];
                QRgba64 edgeColor = QRgba64::fromRgba(0, 100, 255, 255);
                tailor_visualization::Arc arc(
                    tailor_visualization::ArcPoint(v1.point.x(), v1.point.y()),
                    tailor_visualization::ArcPoint(v2.point.x(), v2.point.y()),
                    v1.bulge, edgeColor);
                arcs.push_back(arc);
            }
            for (int j = n - 1; j > 0; --j) {
                const auto& v1 = polyline.vertices[j];
                const auto& v2 = polyline.vertices[j - 1];
                QRgba64 edgeColor = QRgba64::fromRgba(0, 100, 255, 255);
                tailor_visualization::Arc arc(
                    tailor_visualization::ArcPoint(v1.point.x(), v1.point.y()),
                    tailor_visualization::ArcPoint(v2.point.x(), v2.point.y()),
                    -v2.bulge, edgeColor);  // 反向边凸度取反
                arcs.push_back(arc);
            }
            if (!arcs.empty()) clipPolylineArcs.push_back(arcs);
        }
    }
    booleanOp.AddClipPolygons(clipPolylineArcs);

    // 生成不同的颜色用于每个结果多边形（蓝色系）
    std::vector<QColor> colorPalette = {
        QColor(0, 100, 255, 200),     // 蓝色
        QColor(50, 150, 255, 200),     // 浅蓝色
        QColor(100, 200, 255, 200),    // 更浅的蓝色
        QColor(0, 150, 200, 200),      // 青蓝色
        QColor(0, 50, 255, 200)        // 深蓝色
    };

    // 执行 OnlyClipPatternWithHoles (使用指定的 fillType 和 connectType 参数，获取内环信息)
    auto resultPolygonsWithHoles =
        booleanOp.ExecuteOnlyClipPatternWithHoles(fillType, connectType);

    // 转换结果为 BooleanResultPolygon
    for (size_t i = 0; i < resultPolygonsWithHoles.size(); ++i) {
        const auto& polyWithHole = resultPolygonsWithHoles[i];
        const auto& resultArcs = polyWithHole.vertices;
        BooleanResultPolygon resultPoly;
        resultPoly.color = colorPalette[i % colorPalette.size()];
        resultPoly.isHole = polyWithHole.isHole;  // 设置内环标记

        // 转换每个边到顶点
        for (const auto& edge : resultArcs) {
            const auto& start = edge.Point0();
            double bulge = edge.Bulge();
            QRgba64 colorData = edge.Data();

            PolygonVertex vertex;
            vertex.point = QPointF(start.x, start.y);
            vertex.bulge = bulge;
            // 从Arc的颜色数据中提取颜色
            QColor edgeColor = QColor::fromRgba64(colorData);
            vertex.edgeColor = edgeColor;
            resultPoly.vertices.append(vertex);
            resultPoly.edgeColors.append(edgeColor);
        }

        m_clipPatternResults.append(resultPoly);
    }

    // 触发视图更新
    update();
}

void Sketch2DView::executeSubjectPattern(
    const tailor_visualization::IFillType* fillType,
    const tailor_visualization::IConnectType<ConnectTypeDrafting>* connectType) {
    // 清空当前结果
    m_subjectPatternResults.clear();

    // 创建布尔运算对象
    tailor_visualization::BooleanOperations booleanOp;

    // 转换 Subject 集合的多边形到 BooleanOperations 格式
    std::vector<std::vector<tailor_visualization::Arc>> subjectArcs;
    for (int index : m_subjectPolygons) {
        if (index >= 0 && index < m_polygons.size()) {
            const auto& poly = m_polygons[index];
            std::vector<tailor_visualization::Arc> arcs;
            for (int j = 0; j < poly.vertices.size(); ++j) {
                int next = (j + 1) % poly.vertices.size();
                const auto& v1 = poly.vertices[j];
                const auto& v2 = poly.vertices[next];

                // 使用顶点的edgeColor，如果为默认白色则使用红色
                QRgba64 edgeColor = (v1.edgeColor == QColor(255, 255, 255))
                    ? QRgba64::fromRgba(255, 50, 50, 255)
                    : QColorToQRgba64(v1.edgeColor);
                tailor_visualization::Arc arc(
                    tailor_visualization::ArcPoint(v1.point.x(), v1.point.y()),
                    tailor_visualization::ArcPoint(v2.point.x(), v2.point.y()),
                    v1.bulge,
                    edgeColor
                );
                arcs.push_back(arc);
            }
            subjectArcs.push_back(arcs);
        }
    }
    // 批量添加 Subject 多边形
    booleanOp.AddSubjectPolygons(subjectArcs);

    // 转换 Subject 集合的多段线为封闭形式
    std::vector<std::vector<tailor_visualization::Arc>> subjectPolylineArcs;
    for (int index : m_subjectPolylines) {
        if (index >= 0 && index < m_polylines.size()) {
            const auto& polyline = m_polylines[index];
            std::vector<tailor_visualization::Arc> arcs;
            int n = polyline.vertices.size();
            if (n < 2) continue;
            for (int j = 0; j < n - 1; ++j) {
                const auto& v1 = polyline.vertices[j];
                const auto& v2 = polyline.vertices[j + 1];
                QRgba64 edgeColor = QRgba64::fromRgba(255, 50, 50, 255);
                tailor_visualization::Arc arc(
                    tailor_visualization::ArcPoint(v1.point.x(), v1.point.y()),
                    tailor_visualization::ArcPoint(v2.point.x(), v2.point.y()),
                    v1.bulge, edgeColor);
                arcs.push_back(arc);
            }
            for (int j = n - 1; j > 0; --j) {
                const auto& v1 = polyline.vertices[j];
                const auto& v2 = polyline.vertices[j - 1];
                QRgba64 edgeColor = QRgba64::fromRgba(255, 50, 50, 255);
                tailor_visualization::Arc arc(
                    tailor_visualization::ArcPoint(v1.point.x(), v1.point.y()),
                    tailor_visualization::ArcPoint(v2.point.x(), v2.point.y()),
                    -v2.bulge, edgeColor);  // 反向边凸度取反
                arcs.push_back(arc);
            }
            if (!arcs.empty()) subjectPolylineArcs.push_back(arcs);
        }
    }
    booleanOp.AddSubjectPolygons(subjectPolylineArcs);

    // 生成不同的颜色用于每个结果多边形（红色系）
    std::vector<QColor> colorPalette = {
        QColor(255, 50, 50, 200),     // 红色
        QColor(255, 100, 100, 200),   // 浅红色
        QColor(255, 150, 150, 200),   // 更浅的红色
        QColor(255, 0, 100, 200),    // 品红色
        QColor(200, 0, 50, 200)      // 深红色
    };

    // 执行 OnlySubjectPatternWithHoles (使用指定的 fillType 和 connectType 参数，获取内环信息)
    auto resultPolygonsWithHoles =
        booleanOp.ExecuteOnlySubjectPatternWithHoles(fillType, connectType);

    // 转换结果为 BooleanResultPolygon
    for (size_t i = 0; i < resultPolygonsWithHoles.size(); ++i) {
        const auto& polyWithHole = resultPolygonsWithHoles[i];
        const auto& resultArcs = polyWithHole.vertices;
        BooleanResultPolygon resultPoly;
        resultPoly.color = colorPalette[i % colorPalette.size()];
        resultPoly.isHole = polyWithHole.isHole;  // 设置内环标记

        // 转换每个边到顶点
        for (const auto& edge : resultArcs) {
            const auto& start = edge.Point0();
            double bulge = edge.Bulge();
            QRgba64 colorData = edge.Data();

            PolygonVertex vertex;
            vertex.point = QPointF(start.x, start.y);
            vertex.bulge = bulge;
            // 从Arc的颜色数据中提取颜色
            QColor edgeColor = QColor::fromRgba64(colorData);
            vertex.edgeColor = edgeColor;
            resultPoly.vertices.append(vertex);
            resultPoly.edgeColors.append(edgeColor);
        }

        m_subjectPatternResults.append(resultPoly);
    }

    // 触发视图更新
    update();
}

void Sketch2DView::setReadOnly(bool readOnly) {
    m_readOnly = readOnly;
    if (readOnly) {
        // Disable editing state
        m_dragMode = DragMode::None;
        m_dragButton = Qt::NoButton;
        m_arcThroughPoint = QPointF();
        clearSelection();
    }
    update();
}


void Sketch2DView::setPolygons(const QVector<Polygon>& polygons) {
    m_polygons = polygons;
    update();
}

void Sketch2DView::setPolylines(const QVector<Polyline>& polylines) {
    m_polylines = polylines;
    update();
}

void Sketch2DView::setPolygonEdgeColor(int index, const QColor& color, bool isPolygon) {
    if (isPolygon) {
        if (index >= 0 && index < m_polygons.size()) {
            for (auto& v : m_polygons[index].vertices) {
                v.edgeColor = color;
            }
            emit polygonColorChanged(index, color);
            update();
            repaint();  // 强制立即刷新视图
        }
    } else {
        if (index >= 0 && index < m_polylines.size()) {
            for (auto& v : m_polylines[index].vertices) {
                v.edgeColor = color;
            }
            update();
            repaint();  // 强制立即刷新视图
        }
    }
}

void Sketch2DView::setPolygonEdgeColorBatch(const QSet<int>& indices, const QColor& color, bool isPolygon) {
    // 批量设置颜色，避免多次刷新
    for (int index : indices) {
        if (isPolygon) {
            if (index >= 0 && index < m_polygons.size()) {
                for (auto& v : m_polygons[index].vertices) {
                    v.edgeColor = color;
                }
            }
        } else {
            if (index >= 0 && index < m_polylines.size()) {
                for (auto& v : m_polylines[index].vertices) {
                    v.edgeColor = color;
                }
            }
        }
    }
    // 只刷新一次
    update();
    repaint();  // 强制立即刷新视图
    // 通知其他视图刷新
    emit polygonColorChanged(-1, color);  // 使用 -1 表示批量操作
}


void Sketch2DView::contextMenuEvent(QContextMenuEvent* event) {
    if (m_readOnly) {
        QWidget::contextMenuEvent(event);
        return;
    }

    // Don't show context menu if dragging occurred
    if (m_hasDragged) {
        m_hasDragged = false;
        event->ignore();
        return;
    }

    // Store the position for potential creation
    m_contextMenuPosition = snapToPixelCenter(screenToWorld(event->pos()));

    // 检查是否按住 Ctrl 键
    bool ctrlPressed = (QGuiApplication::keyboardModifiers() & Qt::ControlModifier);

    QMenu menu(this);

    // Create polyline action (only available when Polyline tool is selected)
    if (m_tool == Tool::Polyline) {
        QAction* createAction = menu.addAction("创建多段线");
        connect(createAction, &QAction::triggered, this, [this]() {
            // Create line segment (2 vertices) at the stored position
            QVector<PolygonVertex> line;
            line.append(PolygonVertex{ m_contextMenuPosition, 0.0 });
            line.append(PolygonVertex{ m_contextMenuPosition + QPointF(100, 0), 0.0 });
            m_polylines.append(Polyline{ line });
            int index = m_polylines.size() - 1;
            m_polylineCounter++;
            emit polylineAdded(index, QString("多段线%1").arg(m_polylineCounter));
            setSelectedPolygon(index);
            update();
            });
    }

    // Create polygon action (only available when Polygon tool is selected)
    if (m_tool == Tool::Polygon) {
        QAction* createAction = menu.addAction("创建多边形");
        connect(createAction, &QAction::triggered, this, [this]() {
            // Create triangle at the stored position
            QVector<PolygonVertex> triangle;
            triangle.append(PolygonVertex{ m_contextMenuPosition, 0.0 });
            triangle.append(PolygonVertex{ m_contextMenuPosition + QPointF(100, 0), 0.0 });
            triangle.append(PolygonVertex{ m_contextMenuPosition + QPointF(50, 86.6), 0.0 }); // equilateral triangle
            m_polygons.append(Polygon{ triangle });
            int index = m_polygons.size() - 1;
            m_polygonCounter++;
            emit polygonAdded(index, QString("多边形%1").arg(m_polygonCounter));
            setSelectedPolygon(index);
            update();
            });
    }

    // 批量加入 Clip/Subject 集合（当有选中的对象时）
    bool hasSelection = !m_selectedPolygons.isEmpty() || !m_selectedPolylines.isEmpty();
    if (hasSelection && !m_readOnly) {
        menu.addSeparator();

        QAction* addToClipAction = menu.addAction("加入 Clip 集合");
        connect(addToClipAction, &QAction::triggered, this, [this]() {
            setPolygonRoleBatch(m_selectedPolygons, BooleanRole::Clip, true);
            setPolygonRoleBatch(m_selectedPolylines, BooleanRole::Clip, false);
            // 强制立即刷新所有相关视图
            QCoreApplication::processEvents();
            update();
            });

        QAction* addToSubjectAction = menu.addAction("加入 Subject 集合");
        connect(addToSubjectAction, &QAction::triggered, this, [this]() {
            setPolygonRoleBatch(m_selectedPolygons, BooleanRole::Subject, true);
            setPolygonRoleBatch(m_selectedPolylines, BooleanRole::Subject, false);
            // 强制立即刷新所有相关视图
            QCoreApplication::processEvents();
            update();
            });

        QAction* removeFromSetsAction = menu.addAction("从集合中移除");
        connect(removeFromSetsAction, &QAction::triggered, this, [this]() {
            setPolygonRoleBatch(m_selectedPolygons, BooleanRole::None, true);
            setPolygonRoleBatch(m_selectedPolylines, BooleanRole::None, false);
            // 强制立即刷新所有相关视图
            QCoreApplication::processEvents();
            update();
            });

        // 如果按住 Ctrl 键，显示快捷菜单
        if (ctrlPressed) {
            menu.addSeparator();
            QAction* clipMenu = menu.addAction("加入 Clip");
            connect(clipMenu, &QAction::triggered, this, [this]() {
                setPolygonRoleBatch(m_selectedPolygons, BooleanRole::Clip, true);
                setPolygonRoleBatch(m_selectedPolylines, BooleanRole::Clip, false);
                // 强制立即刷新所有相关视图
                QCoreApplication::processEvents();
                update();
                });

            QAction* subjectMenu = menu.addAction("加入 Subject");
            connect(subjectMenu, &QAction::triggered, this, [this]() {
                setPolygonRoleBatch(m_selectedPolygons, BooleanRole::Subject, true);
                setPolygonRoleBatch(m_selectedPolylines, BooleanRole::Subject, false);
                // 强制立即刷新所有相关视图
                QCoreApplication::processEvents();
                update();
                });

            QAction* removeMenu = menu.addAction("移除");
            connect(removeMenu, &QAction::triggered, this, [this]() {
                setPolygonRoleBatch(m_selectedPolygons, BooleanRole::None, true);
                setPolygonRoleBatch(m_selectedPolylines, BooleanRole::None, false);
                // 强制立即刷新所有相关视图
                QCoreApplication::processEvents();
                update();
                });
        }
    }

    // 修改多边形边界颜色（当有选中的多边形时）
    if (!m_selectedPolygons.isEmpty() && !m_readOnly) {
        menu.addSeparator();
        QAction* colorAction = menu.addAction("更改边界颜色...");
        connect(colorAction, &QAction::triggered, this, [this]() {
            if (!m_selectedPolygons.isEmpty()) {
                // 获取第一个选中多边形的颜色作为默认值
                int firstIndex = *m_selectedPolygons.constBegin();
                const auto& vertices = m_polygons[firstIndex].vertices;
                if (!vertices.isEmpty()) {
                    QColor currentColor = vertices[0].edgeColor;
                    QColor newColor = QColorDialog::getColor(currentColor, this, "选择边界颜色");
                    if (newColor.isValid()) {
                        // 批量设置所有选中多边形的颜色
                        setPolygonEdgeColorBatch(m_selectedPolygons, newColor, true);
                        // 强制立即刷新所有相关视图
                        QCoreApplication::processEvents();
                        update();
                    }
                }
            }
            });
    }

    if (menu.actions().isEmpty()) {
        QWidget::contextMenuEvent(event);
    } else {
        menu.exec(event->globalPos());
    }
}

QPainterPath Sketch2DView::createPolygonPath(const Polygon& poly) const {
    QPainterPath path;
    if (poly.vertices.isEmpty()) {
        return path;
    }

    QPointF startPoint = poly.vertices[0].point;
    path.moveTo(startPoint);

    for (int j = 0; j < poly.vertices.size(); ++j) {
        int next = (j + 1) % poly.vertices.size();
        const auto& v1 = poly.vertices[j];
        const auto& v2 = poly.vertices[next];

        if (qAbs(v1.bulge) < 1e-6) {
            path.lineTo(v2.point);
        } else {
            ArcSegment arc = arcSegmentFromBulge(v1.point, v2.point, v1.bulge);
            const QRectF rect(arc.center.x() - arc.radius,
                arc.center.y() - arc.radius,
                arc.radius * 2.0,
                arc.radius * 2.0);
            path.arcTo(rect, arc.startAngleDeg, arc.spanAngleDeg);
        }
    }

    path.closeSubpath();
    return path;
}

QPainterPath Sketch2DView::createResultPolygonPath(const BooleanResultPolygon& poly) const {
    QPainterPath path;
    if (poly.vertices.isEmpty()) {
        return path;
    }

    QPointF startPoint = poly.vertices[0].point;
    path.moveTo(startPoint);

    for (int j = 0; j < poly.vertices.size(); ++j) {
        int next = (j + 1) % poly.vertices.size();
        const auto& v1 = poly.vertices[j];
        const auto& v2 = poly.vertices[next];

        if (qAbs(v1.bulge) < 1e-6) {
            path.lineTo(v2.point);
        } else {
            ArcSegment arc = arcSegmentFromBulge(v1.point, v2.point, v1.bulge);
            const QRectF rect(arc.center.x() - arc.radius,
                arc.center.y() - arc.radius,
                arc.radius * 2.0,
                arc.radius * 2.0);
            path.arcTo(rect, arc.startAngleDeg, arc.spanAngleDeg);
        }
    }

    path.closeSubpath();
    return path;
}

void Sketch2DView::debugExportPolygons(const QString& filename, const QSet<int>& indices, const QString& setName) const {
    // 使用 D 盘绝对路径
    QString fullPath = "D:/" + filename;

    // 输出调试信息
    qDebug() << "Attempting to export to:" << fullPath;
    qDebug() << "Set:" << setName << "Count:" << indices.size();

    // 确保目录存在（D盘根目录）
    QDir dir("D:/");
    if (!dir.exists()) {
        qWarning() << "D: drive does not exist!";
        return;
    }

    // 收集需要导出的多边形
    std::vector<tailor_visualization::Polygon> polygonsToExport;

    for (int index : indices) {
        if (index < 0 || index >= m_polygons.size()) {
            continue;
        }

        const auto& poly = m_polygons[index];
        tailor_visualization::Polygon exportPoly;

        // 将 Sketch2DView::Polygon 转换为 PolygonIO::Polygon
        for (int j = 0; j < poly.vertices.size(); ++j) {
            const auto& vertex = poly.vertices[j];
            const auto& nextVertex = poly.vertices[(j + 1) % poly.vertices.size()];

            tailor_visualization::PolygonEdge edge;
            edge.startPoint.x = vertex.point.x();
            edge.startPoint.y = vertex.point.y();
            edge.endPoint.x = nextVertex.point.x();
            edge.endPoint.y = nextVertex.point.y();
            edge.bulge = vertex.bulge;
            exportPoly.edges.push_back(edge);
        }

        if (!exportPoly.edges.empty()) {
            polygonsToExport.push_back(exportPoly);
        }
    }

    // 使用 PolygonIO::ExportToFile 导出（覆盖写入）
    if (!polygonsToExport.empty()) {
        bool success = tailor_visualization::PolygonIO::ExportToFile(fullPath.toStdString(), polygonsToExport);
        if (success) {
            qDebug() << "Export completed successfully:" << fullPath;
        } else {
            qWarning() << "Export failed:" << fullPath;
        }
    } else {
        qWarning() << "No polygons to export";
    }
}






