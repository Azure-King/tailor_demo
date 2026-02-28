#include "PolygonIO.h"
#include <QTextStream>
#include <QFile>
#include <QStringList>
#include <QRegularExpression>
#include <cmath>

namespace tailor_visualization {

bool PolygonIO::ImportFromFile(const std::string& path, std::vector<Polygon>& outPolygons) {
    return ImportFromFile(QString::fromStdString(path), outPolygons);
}

bool PolygonIO::ImportFromFile(const QString& path, std::vector<Polygon>& outPolygons) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    outPolygons.clear();
    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);

    std::vector<PolygonEdge> currentEdges;
    bool inPolygon = false;

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();

        // 跳过空行和注释
        if (line.isEmpty() || line.startsWith("#")) {
            continue;
        }

        // 多边形开始标记
        if (line.startsWith("BEGIN_POLYGON") || line.startsWith("POLYGON")) {
            currentEdges.clear();
            inPolygon = true;
            continue;
        }

        // 多边形结束标记
        if (line.startsWith("END_POLYGON") || line.startsWith("END")) {
            if (inPolygon && !currentEdges.empty()) {
                Polygon polygon;
                polygon.edges = currentEdges;
                outPolygons.push_back(polygon);
            }
            inPolygon = false;
            continue;
        }

        // 解析边数据
        if (inPolygon) {
            PolygonEdge edge;
            if (ParseEdgeLine(line, edge)) {
                currentEdges.push_back(edge);
            }
        }
    }

    // 处理没有 END_POLYGON 的情况
    if (inPolygon && !currentEdges.empty()) {
        Polygon polygon;
        polygon.edges = currentEdges;
        outPolygons.push_back(polygon);
    }

    file.close();
    return !outPolygons.empty();
}

bool PolygonIO::ExportToFile(const std::string& path, const std::vector<Polygon>& polygons) {
    return ExportToFile(QString::fromStdString(path), polygons);
}

bool PolygonIO::ExportToFile(const QString& path, const std::vector<Polygon>& polygons) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return false;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    // 写入文件头注释
    out << "# Polygon Data File\n";
    out << "# Format: startX startY endX endY bulge\n";
    out << "# bulge: 0 = line, non-zero = arc\n";
    out << "\n";

    // 写入每个多边形
    for (size_t polyIndex = 0; polyIndex < polygons.size(); ++polyIndex) {
        const Polygon& polygon = polygons[polyIndex];

        out << "BEGIN_POLYGON " << polyIndex + 1 << "\n";

        for (const auto& edge : polygon.edges) {
            out << FormatEdge(edge) << "\n";
        }

        out << "END_POLYGON\n";
        out << "\n";
    }

    file.close();
    return true;
}

bool PolygonIO::ParseEdgeLine(const QString& line, PolygonEdge& outEdge) {
    QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);

    // 格式: startX startY endX endY bulge
    if (parts.size() != 5) {
        return false;
    }

    bool ok;
    outEdge.startPoint.x = parts[0].toDouble(&ok);
    if (!ok) return false;

    outEdge.startPoint.y = parts[1].toDouble(&ok);
    if (!ok) return false;

    outEdge.endPoint.x = parts[2].toDouble(&ok);
    if (!ok) return false;

    outEdge.endPoint.y = parts[3].toDouble(&ok);
    if (!ok) return false;

    outEdge.bulge = parts[4].toDouble(&ok);
    if (!ok) return false;

    return true;
}

QString PolygonIO::FormatEdge(const PolygonEdge& edge) {
    return QString("%1 %2 %3 %4 %5")
        .arg(edge.startPoint.x, 0, 'f', 6)
        .arg(edge.startPoint.y, 0, 'f', 6)
        .arg(edge.endPoint.x, 0, 'f', 6)
        .arg(edge.endPoint.y, 0, 'f', 6)
        .arg(edge.bulge, 0, 'f', 10);
}

} // namespace tailor_visualization
