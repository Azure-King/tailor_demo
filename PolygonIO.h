#pragma once

#include <vector>
#include <string>
#include <QString>
#include <QTextStream>
#include <QFile>

namespace tailor_visualization {

/**
 * @brief 二维点数据结构
 */
struct Point2D {
    double x;
    double y;

    Point2D() : x(0), y(0) {}
    Point2D(double x_, double y_) : x(x_), y(y_) {}
};

/**
 * @brief 多边形边数据结构
 * 以边为基础单位，包含起点、终点和凸度
 */
struct PolygonEdge {
    Point2D startPoint;      // 起点
    Point2D endPoint;        // 终点
    double bulge;            // 凸度：0表示直线，非0表示圆弧

    /**
     * @brief 判断是否为圆弧
     * @return true 表示圆弧，false 表示直线
     */
    bool IsArc() const { return std::abs(bulge) > 1e-10; }
};

/**
 * @brief 多边形数据结构
 * 一个多边形由多条边组成
 */
struct Polygon {
    std::vector<PolygonEdge> edges;  // 边集合
};

/**
 * @brief 多边形输入输出工具类
 * 用于从文件导入多边形或将多边形导出到文件
 * 支持一个文件包含多个多边形
 */
class PolygonIO {
public:
    /**
     * @brief 从文件导入多边形
     * @param path 文件路径
     * @param outPolygons 输出的多边形数组
     * @return 是否成功导入
     */
    static bool ImportFromFile(const std::string& path, std::vector<Polygon>& outPolygons);

    /**
     * @brief 从文件导入多边形（QString版本）
     * @param path 文件路径
     * @param outPolygons 输出的多边形数组
     * @return 是否成功导入
     */
    static bool ImportFromFile(const QString& path, std::vector<Polygon>& outPolygons);

    /**
     * @brief 将多边形导出到文件
     * @param path 文件路径
     * @param polygons 多边形数组
     * @return 是否成功导出
     */
    static bool ExportToFile(const std::string& path, const std::vector<Polygon>& polygons);

    /**
     * @brief 将多边形导出到文件（QString版本）
     * @param path 文件路径
     * @param polygons 多边形数组
     * @return 是否成功导出
     */
    static bool ExportToFile(const QString& path, const std::vector<Polygon>& polygons);

private:
    /**
     * @brief 解析边数据行
     * @param line 文本行
     * @param outEdge 输出的边
     * @return 是否解析成功
     */
    static bool ParseEdgeLine(const QString& line, PolygonEdge& outEdge);

    /**
     * @brief 格式化边数据为字符串
     * @param edge 边
     * @return 格式化字符串
     */
    static QString FormatEdge(const PolygonEdge& edge);
};

} // namespace tailor_visualization
