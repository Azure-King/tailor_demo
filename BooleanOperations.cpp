#include "BooleanOperations.h"
#include <fstream>
#include <sstream>
#include <qDebug>
#include "PolygonIO.h"

namespace tailor_visualization {

    BooleanOperations::BooleanOperations()
        : arcAnalysis_() {
    }

    void BooleanOperations::AddPolygonFromArcs(const std::vector<Arc>& arcs) {
        // 默认将多边形添加到 Clip 集合
        AddClipPolygon(arcs);
    }

    void BooleanOperations::AddClipPolygon(const std::vector<Arc>& arcs) {
        if (arcs.empty()) {
            return;
        }
        clipPolygons_.push_back(arcs);
    }

    void BooleanOperations::AddSubjectPolygon(const std::vector<Arc>& arcs) {
        if (arcs.empty()) {
            return;
        }
        subjectPolygons_.push_back(arcs);
    }

    void BooleanOperations::AddClipPolygons(const std::vector<std::vector<Arc>>& polygons) {
        for (const auto& poly : polygons) {
            AddClipPolygon(poly);
        }
    }

    void BooleanOperations::AddSubjectPolygons(const std::vector<std::vector<Arc>>& polygons) {
        for (const auto& poly : polygons) {
            AddSubjectPolygon(poly);
        }
    }

    std::vector<std::vector<Arc>> BooleanOperations::Execute(BooleanOperation operation, int fillType) {
        // 检查是否有Clip和Subject多边形
        if (clipPolygons_.empty() && subjectPolygons_.empty()) {
            return {};
        }

        // 创建裁剪器
        ArcTailor tailor(arcAnalysis_);

        // 添加所有Clip多边形（裁剪多边形）并分割为单调弧段
        for (const auto& clipPoly : clipPolygons_) {
            auto monotonicClip = SplitToMonotonic(clipPoly);
            tailor.AddClipper(monotonicClip.begin(), monotonicClip.end());
        }

        // 添加所有Subject多边形（被裁剪多边形）并分割为单调弧段
        for (const auto& subjectPoly : subjectPolygons_) {
            auto monotonicSubject = SplitToMonotonic(subjectPoly);
            tailor.AddSubject(monotonicSubject.begin(), monotonicSubject.end());
        }

        // 执行布尔运算
        return ExecuteWithDrafting(tailor.Execute(), operation, fillType);
    }

    std::vector<std::vector<Arc>> BooleanOperations::Execute(
        BooleanOperation operation,
        const IFillType* clipFillType,
        const IFillType* subjectFillType) {

        // 检查是否有Clip和Subject多边形
        if (clipPolygons_.empty() && subjectPolygons_.empty()) {
            return {};
        }

        // 创建裁剪器
        ArcTailor tailor(arcAnalysis_);

        // 添加所有Clip多边形（裁剪多边形）并分割为单调弧段
        for (const auto& clipPoly : clipPolygons_) {
            auto monotonicClip = SplitToMonotonic(clipPoly);
            tailor.AddClipper(monotonicClip.begin(), monotonicClip.end());
        }

        // 添加所有Subject多边形（被裁剪多边形）并分割为单调弧段
        for (const auto& subjectPoly : subjectPolygons_) {
            auto monotonicSubject = SplitToMonotonic(subjectPoly);
            tailor.AddSubject(monotonicSubject.begin(), monotonicSubject.end());
        }

        // 执行裁剪
        auto drafting = tailor.Execute();

        // 调用ExecuteWithFillTypes处理结果
        return ExecuteWithFillTypes(drafting, operation, clipFillType, subjectFillType);
    }

    std::vector<std::vector<Arc>> BooleanOperations::Execute(
        BooleanOperation operation,
        const IFillType* clipFillType,
        const IFillType* subjectFillType,
        const IConnectType<tailor::Tailor<Arc, BooleanOperations::AA>::PatternDrafting>* connectType) {

        // 检查是否有Clip和Subject多边形
        if (clipPolygons_.empty() && subjectPolygons_.empty()) {
            return {};
        }

        // 创建裁剪器
        ArcTailor tailor(arcAnalysis_);

        // 添加所有Clip多边形（裁剪多边形）并分割为单调弧段
        for (const auto& clipPoly : clipPolygons_) {
            auto monotonicClip = SplitToMonotonic(clipPoly);
            tailor.AddClipper(monotonicClip.begin(), monotonicClip.end());
        }

        // 添加所有Subject多边形（被裁剪多边形）并分割为单调弧段
        for (const auto& subjectPoly : subjectPolygons_) {
            auto monotonicSubject = SplitToMonotonic(subjectPoly);
            tailor.AddSubject(monotonicSubject.begin(), monotonicSubject.end());
        }

        // 执行裁剪
        auto drafting = tailor.Execute();

        // 调用ExecuteWithFillTypesAndConnectType方法
        return ExecuteWithFillTypesAndConnectType(drafting, operation, clipFillType, subjectFillType, connectType);
    }

    std::vector<std::vector<Arc>> BooleanOperations::ExecuteWithDrafting(
        const tailor::Tailor<Arc, BooleanOperations::AA>::PatternDrafting& drafting,
        BooleanOperation operation, int fillType) {
        std::vector<std::vector<Arc>> result;
        // 根据布尔运算类型执行相应的操作
        switch (operation) {
        case BooleanOperation::Union:
            result = ExecuteUnion(drafting, fillType);
            break;
        case BooleanOperation::Intersection:
            result = ExecuteIntersection(drafting, fillType);
            break;
        case BooleanOperation::Difference:
            result = ExecuteDifference(drafting, fillType);
            break;
        case BooleanOperation::XOR:
            result = ExecuteXOR(drafting, fillType);
            break;
        default:
            result = {};
        }

        // 导出结果多边形用于调试
        std::vector<tailor_visualization::Polygon> exportPolygons;
        for (const auto& polygon : result) {
            tailor_visualization::Polygon exportPoly;
            for (const auto& arc : polygon) {
                tailor_visualization::PolygonEdge edge;
                edge.startPoint.x = arc.Point0().x;
                edge.startPoint.y = arc.Point0().y;
                edge.endPoint.x = arc.Point1().x;
                edge.endPoint.y = arc.Point1().y;
                edge.bulge = arc.Bulge();
                exportPoly.edges.push_back(edge);
            }
            exportPolygons.push_back(exportPoly);
        }
        tailor_visualization::PolygonIO::ExportToFile(std::string("D:\\result_test.txt"), exportPolygons);

        return result;
    }


    std::vector<std::vector<Arc>> BooleanOperations::ExecuteOnlyClipPattern(const IFillType* fillType) {
        // 默认使用 OuterFirst ConnectType
        return ExecuteOnlyClipPattern(fillType, nullptr);
    }

    std::vector<std::vector<Arc>> BooleanOperations::ExecuteOnlySubjectPattern(const IFillType* fillType) {
        // 默认使用 OuterFirst ConnectType
        return ExecuteOnlySubjectPattern(fillType, nullptr);
    }

    std::vector<std::vector<Arc>> BooleanOperations::ExecuteOnlyClipPattern(
        const IFillType* fillType,
        const IConnectType<tailor::Tailor<Arc, BooleanOperations::AA>::PatternDrafting>* connectType) {
        // 根据 connectType 参数决定使用 InnerFirst 还是 OuterFirst
        bool useInnerFirst = false;
        using Drafting = typename ArcTailor::PatternDrafting;
        if (connectType && dynamic_cast<const ConnectTypeInnerFirstWrapper<Drafting>*>(connectType)) {
            useInnerFirst = true;
        }

        // 只处理 Clip 集合的 Pattern
        if (clipPolygons_.empty()) {
            return {};
        }

        // 创建 tailor
        ArcTailor tailor(arcAnalysis_);

        // 添加 Clip 集合的多边形
        for (const auto& clipPoly : clipPolygons_) {
            auto monotonicClip = SplitToMonotonic(clipPoly);
            tailor.AddClipper(monotonicClip.begin(), monotonicClip.end());
        }

        // 执行
        auto drafting = tailor.Execute();

        std::vector<std::vector<Arc>> resultPolygons;

        std::function<void(const tailor::Polygon<tailor::PolyEdgeInfo>&)> fun =
            [&](const tailor::Polygon<tailor::PolyEdgeInfo>& poly) -> void {
            std::vector<Arc> polygonEdges;
            for (const auto& edge_info : poly.edges) {
                if (edge_info.type == tailor::BoundaryType::UpperBoundary) {
                    // 需要反转边
                    const auto& edge = drafting.edgeEvent[edge_info.id].edge;
                    Arc reversedEdge(edge.Point1(), edge.Point0(), -edge.Bulge(), edge.Data());
                    polygonEdges.push_back(reversedEdge);
                } else {
                    const auto& edge = drafting.edgeEvent[edge_info.id].edge;
                    polygonEdges.push_back(edge);
                }
            }
            if (!polygonEdges.empty()) {
                resultPolygons.push_back(polygonEdges);
            }
        };

        // 将 IFillType 转换为 FillTypeVariant
        auto fillTypeVariant = ToFillTypeVariant(fillType);

        // 使用 std::visit 来选择正确的 Pattern，同时支持 ConnectType
        std::visit([&](auto&& type) {
            using FillType = std::decay_t<decltype(type)>;

            if (useInnerFirst) {
                tailor::OnlyClipPattern<FillType, tailor::ConnectTypeInnerFirst> pattern;
                auto polytree = pattern.Stitch(drafting);
                for (const auto& tree : polytree) {
                    ForEachPolyTree<tailor::PolyEdgeInfo>(tree, fun);
                }
            } else {
                tailor::OnlyClipPattern<FillType, tailor::ConnectTypeOuterFirst> pattern;
                auto polytree = pattern.Stitch(drafting);
                for (const auto& tree : polytree) {
                    ForEachPolyTree<tailor::PolyEdgeInfo>(tree, fun);
                }
            }
        }, fillTypeVariant.variant);

        return resultPolygons;
    }

    std::vector<std::vector<Arc>> BooleanOperations::ExecuteOnlySubjectPattern(
        const IFillType* fillType,
        const IConnectType<tailor::Tailor<Arc, BooleanOperations::AA>::PatternDrafting>* connectType) {
        // 根据 connectType 参数决定使用 InnerFirst 还是 OuterFirst
        bool useInnerFirst = false;
        using Drafting = typename ArcTailor::PatternDrafting;
        if (connectType && dynamic_cast<const ConnectTypeInnerFirstWrapper<Drafting>*>(connectType)) {
            useInnerFirst = true;
        }

        // 只处理 Subject 集合的 Pattern
        if (subjectPolygons_.empty()) {
            return {};
        }

        // 创建 tailor
        ArcTailor tailor(arcAnalysis_);

        // 添加 Subject 集合的多边形
        for (const auto& subjectPoly : subjectPolygons_) {
            auto monotonicSubject = SplitToMonotonic(subjectPoly);
            tailor.AddSubject(monotonicSubject.begin(), monotonicSubject.end());
        }

        // 执行
        auto drafting = tailor.Execute();

        std::vector<std::vector<Arc>> resultPolygons;

        std::function<void(const tailor::Polygon<tailor::PolyEdgeInfo>&)> fun =
            [&](const tailor::Polygon<tailor::PolyEdgeInfo>& poly) -> void {
            std::vector<Arc> polygonEdges;
            for (const auto& edge_info : poly.edges) {
                if (edge_info.type == tailor::BoundaryType::UpperBoundary) { 
                    // 需要反转边
                    const auto& edge = drafting.edgeEvent[edge_info.id].edge;
                    Arc reversedEdge(edge.Point1(), edge.Point0(), -edge.Bulge(), edge.Data());
                    polygonEdges.push_back(reversedEdge);
                } else {
                    const auto& edge = drafting.edgeEvent[edge_info.id].edge;
                    polygonEdges.push_back(edge); 
                }
            }
            if (!polygonEdges.empty()) {
                resultPolygons.push_back(polygonEdges);
            }
        };

        // 将 IFillType 转换为 FillTypeVariant
        auto fillTypeVariant = ToFillTypeVariant(fillType);

        // 使用 std::visit 来选择正确的 Pattern，同时支持 ConnectType
        std::visit([&](auto&& type) {
            using FillType = std::decay_t<decltype(type)>;

            if (useInnerFirst) {
                tailor::OnlySubjectPattern<FillType, tailor::ConnectTypeInnerFirst> pattern;
                auto polytree = pattern.Stitch(drafting);
                for (const auto& tree : polytree) {
                    ForEachPolyTree<tailor::PolyEdgeInfo>(tree, fun);
                }
            } else {
                tailor::OnlySubjectPattern<FillType, tailor::ConnectTypeOuterFirst> pattern;
                auto polytree = pattern.Stitch(drafting);
                for (const auto& tree : polytree) {
                    ForEachPolyTree<tailor::PolyEdgeInfo>(tree, fun);
                }
            }
        }, fillTypeVariant.variant);

        return resultPolygons;
    }

    typename ArcTailor::PatternDrafting BooleanOperations::CreateDrafting() {
        // 检查是否有任何多边形需要处理
        if (clipPolygons_.empty() && subjectPolygons_.empty()) {
            return {};
        }

        // 创建裁剪器
        ArcTailor tailor(arcAnalysis_);

        // 添加所有Clip多边形（需要分割为单调弧段）
        for (const auto& clipPoly : clipPolygons_) {
            auto monotonicClip = SplitToMonotonic(clipPoly);
            tailor.AddClipper(monotonicClip.begin(), monotonicClip.end());
        }

        // 添加所有Subject多边形（需要分割为单调弧段）
        for (const auto& subjectPoly : subjectPolygons_) {
            auto monotonicSubject = SplitToMonotonic(subjectPoly);
            tailor.AddSubject(monotonicSubject.begin(), monotonicSubject.end());
        }

        // 执行并返回drafting
        return tailor.Execute();
    }

    void BooleanOperations::Clear() {
        clipPolygons_.clear();
        subjectPolygons_.clear();
    }

    std::vector<std::vector<Arc>> BooleanOperations::ExecuteWithFillTypes(
        const tailor::Tailor<Arc, BooleanOperations::AA>::PatternDrafting& drafting,
        BooleanOperation operation,
        const IFillType* clipFillType,
        const IFillType* subjectFillType) {

        qDebug() << "ExecuteWithFillTypes - Operation:" << static_cast<int>(operation);

        std::vector<std::vector<Arc>> resultPolygons;

        std::function<void(const tailor::Polygon<tailor::PolyEdgeInfo>&)> fun =
            [&](const tailor::Polygon<tailor::PolyEdgeInfo>& poly) -> void {
            std::vector<Arc> polygonEdges;
            for (const auto& edge_info : poly.edges) {
                if (edge_info.type == tailor::BoundaryType::UpperBoundary) {
                    // UpperBoundary 需要反转边
                    const auto& edge = drafting.edgeEvent[edge_info.id].edge;
                    Arc reversedEdge(edge.Point1(), edge.Point0(), -edge.Bulge(), edge.Data());
                    polygonEdges.push_back(reversedEdge);
                } else {
                    const auto& edge = drafting.edgeEvent[edge_info.id].edge;
                    polygonEdges.push_back(edge);
                }
            }
            if (!polygonEdges.empty()) {
                resultPolygons.push_back(polygonEdges);
            }
        };

        // 转换FillType到FillTypeVariant
        auto clipVariant = ToFillTypeVariant(clipFillType);
        auto subjectVariant = ToFillTypeVariant(subjectFillType);

        // 使用 std::visit 双重分发到正确的 Pattern
        // Lambda 函数根据 operation 创建相应的 tailor Pattern
        auto executePattern = [&](auto&& clipType, auto&& subjectType) -> std::vector<std::vector<Arc>> {
            using ClipType = std::decay_t<decltype(clipType)>;
            using SubjectType = std::decay_t<decltype(subjectType)>;

            switch (operation) {
                case BooleanOperation::Union: {
                    tailor::UnionPattern<SubjectType, ClipType, tailor::ConnectTypeOuterFirst> pattern;
                    auto polytree = pattern.Stitch(drafting);
                    for (const auto& tree : polytree) {
                        ForEachPolyTree<tailor::PolyEdgeInfo>(tree, fun);
                    }
                    break;
                }
                case BooleanOperation::Intersection: {
                    tailor::IntersectionPattern<SubjectType, ClipType, tailor::ConnectTypeOuterFirst> pattern;
                    auto polytree = pattern.Stitch(drafting);
                    for (const auto& tree : polytree) {
                        ForEachPolyTree<tailor::PolyEdgeInfo>(tree, fun);
                    }
                    break;
                }
                case BooleanOperation::Difference: {
                    tailor::DifferencePattern<SubjectType, ClipType, tailor::ConnectTypeOuterFirst> pattern;
                    auto polytree = pattern.Stitch(drafting);
                    for (const auto& tree : polytree) {
                        ForEachPolyTree<tailor::PolyEdgeInfo>(tree, fun);
                    }
                    break;
                }
                case BooleanOperation::XOR: {
                    tailor::SymmetricDifferencePattern<SubjectType, ClipType, tailor::ConnectTypeOuterFirst> pattern;
                    auto polytree = pattern.Stitch(drafting);
                    for (const auto& tree : polytree) {
                        ForEachPolyTree<tailor::PolyEdgeInfo>(tree, fun);
                    }
                    break;
                }
                default:
                    break;
            }
            return resultPolygons;
        };

        // 使用 std::visit 双重分发
        resultPolygons = std::visit([&](auto&& clipType) {
            return std::visit([&](auto&& subjectType) {
                return executePattern(clipType, subjectType);
            }, subjectVariant.variant);
        }, clipVariant.variant);


        return resultPolygons;
    }

    std::vector<std::vector<Arc>> BooleanOperations::ExecuteWithFillTypesAndConnectType(
        const tailor::Tailor<Arc, BooleanOperations::AA>::PatternDrafting& drafting,
        BooleanOperation operation,
        const IFillType* clipFillType,
        const IFillType* subjectFillType,
        const IConnectType<tailor::Tailor<Arc, BooleanOperations::AA>::PatternDrafting>* connectType) {

        qDebug() << "ExecuteWithFillTypesAndConnectType - Operation:" << static_cast<int>(operation);

        std::vector<std::vector<Arc>> resultPolygons;

        std::function<void(const tailor::Polygon<tailor::PolyEdgeInfo>&)> fun =
            [&](const tailor::Polygon<tailor::PolyEdgeInfo>& poly) -> void {
            std::vector<Arc> polygonEdges;
            for (const auto& edge_info : poly.edges) {
                if (edge_info.type == tailor::BoundaryType::UpperBoundary) {
                    // UpperBoundary需要反转边
                    const auto& edge = drafting.edgeEvent[edge_info.id].edge;
                    Arc reversedEdge(edge.Point1(), edge.Point0(), -edge.Bulge(), edge.Data());
                    polygonEdges.push_back(reversedEdge);
                } else {
                    const auto& edge = drafting.edgeEvent[edge_info.id].edge;
                    polygonEdges.push_back(edge);
                }
            }
            if (!polygonEdges.empty()) {
                resultPolygons.push_back(polygonEdges);
            }
        };

        // 转换FillType到FillTypeVariant
        auto clipVariant = ToFillTypeVariant(clipFillType);
        auto subjectVariant = ToFillTypeVariant(subjectFillType);

        // 根据 connectType 参数决定使用 InnerFirst 还是 OuterFirst
        bool useInnerFirst = false;
        using Drafting = typename ArcTailor::PatternDrafting;
        if (connectType && dynamic_cast<const ConnectTypeInnerFirstWrapper<Drafting>*>(connectType)) {
            useInnerFirst = true;
        }

        // 使用 std::visit 双重分发到正确的 Pattern
        // Lambda 函数根据 operation 创建相应的 tailor Pattern
        auto executePattern = [&](auto&& clipType, auto&& subjectType) -> std::vector<std::vector<Arc>> {
            using ClipType = std::decay_t<decltype(clipType)>;
            using SubjectType = std::decay_t<decltype(subjectType)>;

            switch (operation) {
                case BooleanOperation::Union: {
                    if (useInnerFirst) {
                        tailor::UnionPattern<SubjectType, ClipType, tailor::ConnectTypeInnerFirst> pattern;
                        auto polytree = pattern.Stitch(drafting);
                        for (const auto& tree : polytree) {
                            ForEachPolyTree<tailor::PolyEdgeInfo>(tree, fun);
                        }
                    } else {
                        tailor::UnionPattern<SubjectType, ClipType, tailor::ConnectTypeOuterFirst> pattern;
                        auto polytree = pattern.Stitch(drafting);
                        for (const auto& tree : polytree) {
                            ForEachPolyTree<tailor::PolyEdgeInfo>(tree, fun);
                        }
                    }
                    break;
                }
                case BooleanOperation::Intersection: {
                    if (useInnerFirst) {
                        tailor::IntersectionPattern<SubjectType, ClipType, tailor::ConnectTypeInnerFirst> pattern;
                        auto polytree = pattern.Stitch(drafting);
                        for (const auto& tree : polytree) {
                            ForEachPolyTree<tailor::PolyEdgeInfo>(tree, fun);
                        }
                    } else {
                        tailor::IntersectionPattern<SubjectType, ClipType, tailor::ConnectTypeOuterFirst> pattern;
                        auto polytree = pattern.Stitch(drafting);
                        for (const auto& tree : polytree) {
                            ForEachPolyTree<tailor::PolyEdgeInfo>(tree, fun);
                        }
                    }
                    break;
                }
                case BooleanOperation::Difference: {
                    if (useInnerFirst) {
                        tailor::DifferencePattern<SubjectType, ClipType, tailor::ConnectTypeInnerFirst> pattern;
                        auto polytree = pattern.Stitch(drafting);
                        for (const auto& tree : polytree) {
                            ForEachPolyTree<tailor::PolyEdgeInfo>(tree, fun);
                        }
                    } else {
                        tailor::DifferencePattern<SubjectType, ClipType, tailor::ConnectTypeOuterFirst> pattern;
                        auto polytree = pattern.Stitch(drafting);
                        for (const auto& tree : polytree) {
                            ForEachPolyTree<tailor::PolyEdgeInfo>(tree, fun);
                        }
                    }
                    break;
                }
                case BooleanOperation::XOR: {
                    if (useInnerFirst) {
                        tailor::SymmetricDifferencePattern<SubjectType, ClipType, tailor::ConnectTypeInnerFirst> pattern;
                        auto polytree = pattern.Stitch(drafting);
                        for (const auto& tree : polytree) {
                            ForEachPolyTree<tailor::PolyEdgeInfo>(tree, fun);
                        }
                    } else {
                        tailor::SymmetricDifferencePattern<SubjectType, ClipType, tailor::ConnectTypeOuterFirst> pattern;
                        auto polytree = pattern.Stitch(drafting);
                        for (const auto& tree : polytree) {
                            ForEachPolyTree<tailor::PolyEdgeInfo>(tree, fun);
                        }
                    }
                    break;
                }
                default:
                    break;
            }
            return resultPolygons;
        };

        // 使用 std::visit 双重分发
        resultPolygons = std::visit([&](auto&& clipType) {
            return std::visit([&](auto&& subjectType) {
                return executePattern(clipType, subjectType);
            }, subjectVariant.variant);
        }, clipVariant.variant);


        return resultPolygons;
    }

    std::vector<Arc> BooleanOperations::SplitToMonotonic(const std::vector<Arc>& arcs) {
        std::vector<Arc> result;
        for (const auto& arc : arcs) {
            auto splitArcs = arcAnalysis_.SplitToMonotonic2(arc);
            result.insert(result.end(), splitArcs.begin(), splitArcs.end());
        }
        return result;
    }

    template<typename T>
    void BooleanOperations::ForEachPolyTree(const tailor::PolyTree<T>& tree,
        const std::function<void(const typename tailor::PolyTree<T>::PolygonType&)>& callback) const {
        if (callback) {
            callback(tree.polygon);
        }
        for (const auto& child : tree.children) {
            ForEachPolyTree(child, callback);
        }
    }

    std::vector<std::vector<Arc>> BooleanOperations::ExecuteUnion(
        const tailor::Tailor<Arc, BooleanOperations::AA>::PatternDrafting& drafting, int fillType) {
        std::vector<std::vector<Arc>> resultPolygons;

        std::function<void(const tailor::Polygon<tailor::PolyEdgeInfo>&)> fun =
            [&](const tailor::Polygon<tailor::PolyEdgeInfo>& poly) -> void {
            std::vector<Arc> polygonEdges;
            for (const auto& edge_info : poly.edges) {
                if (edge_info.type == tailor::BoundaryType::UpperBoundary) {
                    // UpperBoundary 需要反转边
                    const auto& edge = drafting.edgeEvent[edge_info.id].edge;
                    Arc reversedEdge(edge.Point1(), edge.Point0(), -edge.Bulge(), edge.Data());
                    polygonEdges.push_back(reversedEdge);
                } else {
                    const auto& edge = drafting.edgeEvent[edge_info.id].edge;
                    polygonEdges.push_back(edge);
                }
            }
            if (!polygonEdges.empty()) {
                resultPolygons.push_back(polygonEdges);
            }
        };

        // 根据 fillType 选择正确的 Pattern
        switch (fillType) {
            case 0: // NonZeroFillType
                {
                    tailor::UnionPattern<tailor::NonZeroFillType, tailor::NonZeroFillType, tailor::ConnectTypeOuterFirst> pattern;
                    auto polytree = pattern.Stitch(drafting);
                    for (const auto& tree : polytree) {
                        ForEachPolyTree<tailor::PolyEdgeInfo>(tree, fun);
                    }
                }
                break;
            case 1: // EvenOddFillType
            default:
                {
                    tailor::UnionPattern<tailor::EvenOddFillType, tailor::EvenOddFillType, tailor::ConnectTypeOuterFirst> pattern;
                    auto polytree = pattern.Stitch(drafting);
                    for (const auto& tree : polytree) {
                        ForEachPolyTree<tailor::PolyEdgeInfo>(tree, fun);
                    }
                }
                break;
            case 2: // IgnoreFillType
                {
                    tailor::UnionPattern<tailor::IgnoreFillType, tailor::IgnoreFillType, tailor::ConnectTypeOuterFirst> pattern;
                    auto polytree = pattern.Stitch(drafting);
                    for (const auto& tree : polytree) {
                        ForEachPolyTree<tailor::PolyEdgeInfo>(tree, fun);
                    }
                }
                break;
        }

        return resultPolygons;
    }

    std::vector<std::vector<Arc>> BooleanOperations::ExecuteIntersection(
        const tailor::Tailor<Arc, BooleanOperations::AA>::PatternDrafting& drafting, int fillType) {
        std::vector<std::vector<Arc>> resultPolygons;

        std::function<void(const tailor::Polygon<tailor::PolyEdgeInfo>&)> fun =
            [&](const tailor::Polygon<tailor::PolyEdgeInfo>& poly) -> void {
            std::vector<Arc> polygonEdges;
            for (const auto& edge_info : poly.edges) {
                if (edge_info.type == tailor::BoundaryType::UpperBoundary) {
                    // UpperBoundary 需要反转边
                    const auto& edge = drafting.edgeEvent[edge_info.id].edge;
                    Arc reversedEdge(edge.Point1(), edge.Point0(), -edge.Bulge(), edge.Data());
                    polygonEdges.push_back(reversedEdge);
                } else {
                    const auto& edge = drafting.edgeEvent[edge_info.id].edge;
                    polygonEdges.push_back(edge);
                }
            }
            if (!polygonEdges.empty()) {
                resultPolygons.push_back(polygonEdges);
            }
        };

        // 根据 fillType 选择正确的 Pattern
        switch (fillType) {
            case 0: // NonZeroFillType
                {
                    tailor::IntersectionPattern<tailor::NonZeroFillType, tailor::NonZeroFillType, tailor::ConnectTypeOuterFirst> pattern;
                    auto polytree = pattern.Stitch(drafting);
                    for (const auto& tree : polytree) {
                        ForEachPolyTree<tailor::PolyEdgeInfo>(tree, fun);
                    }
                }
                break;
            case 1: // EvenOddFillType
            default:
                {
                    tailor::IntersectionPattern<tailor::EvenOddFillType, tailor::EvenOddFillType, tailor::ConnectTypeOuterFirst> pattern;
                    auto polytree = pattern.Stitch(drafting);
                    for (const auto& tree : polytree) {
                        ForEachPolyTree<tailor::PolyEdgeInfo>(tree, fun);
                    }
                }
                break;
            case 2: // IgnoreFillType
                {
                    tailor::IntersectionPattern<tailor::IgnoreFillType, tailor::IgnoreFillType, tailor::ConnectTypeOuterFirst> pattern;
                    auto polytree = pattern.Stitch(drafting);
                    for (const auto& tree : polytree) {
                        ForEachPolyTree<tailor::PolyEdgeInfo>(tree, fun);
                    }
                }
                break;
        }

        return resultPolygons;
    }

    std::vector<std::vector<Arc>> BooleanOperations::ExecuteDifference(
        const tailor::Tailor<Arc, BooleanOperations::AA>::PatternDrafting& drafting, int fillType) {
        std::vector<std::vector<Arc>> resultPolygons;

        std::function<void(const tailor::Polygon<tailor::PolyEdgeInfo>&)> fun =
            [&](const tailor::Polygon<tailor::PolyEdgeInfo>& poly) -> void {
            std::vector<Arc> polygonEdges;
            for (const auto& edge_info : poly.edges) {
                if (edge_info.type == tailor::BoundaryType::UpperBoundary) {
                    // UpperBoundary 需要反转边
                    const auto& edge = drafting.edgeEvent[edge_info.id].edge;
                    Arc reversedEdge(edge.Point1(), edge.Point0(), -edge.Bulge(), edge.Data());
                    polygonEdges.push_back(reversedEdge);
                } else {
                    const auto& edge = drafting.edgeEvent[edge_info.id].edge;
                    polygonEdges.push_back(edge);
                }
            }
            if (!polygonEdges.empty()) {
                resultPolygons.push_back(polygonEdges);
            }
        };

        // 根据 fillType 选择正确的 Pattern
        switch (fillType) {
            case 0: // NonZeroFillType
                {
                    tailor::DifferencePattern<tailor::NonZeroFillType, tailor::NonZeroFillType, tailor::ConnectTypeOuterFirst> pattern;
                    auto polytree = pattern.Stitch(drafting);
                    for (const auto& tree : polytree) {
                        ForEachPolyTree<tailor::PolyEdgeInfo>(tree, fun);
                    }
                }
                break;
            case 1: // EvenOddFillType
            default:
                {
                    tailor::DifferencePattern<tailor::EvenOddFillType, tailor::EvenOddFillType, tailor::ConnectTypeOuterFirst> pattern;
                    auto polytree = pattern.Stitch(drafting);
                    for (const auto& tree : polytree) {
                        ForEachPolyTree<tailor::PolyEdgeInfo>(tree, fun);
                    }
                }
                break;
            case 2: // IgnoreFillType
                {
                    tailor::DifferencePattern<tailor::IgnoreFillType, tailor::IgnoreFillType, tailor::ConnectTypeOuterFirst> pattern;
                    auto polytree = pattern.Stitch(drafting);
                    for (const auto& tree : polytree) {
                        ForEachPolyTree<tailor::PolyEdgeInfo>(tree, fun);
                    }
                }
                break;
        }

        return resultPolygons;
    }

    std::vector<std::vector<Arc>> BooleanOperations::ExecuteXOR(
        const tailor::Tailor<Arc, BooleanOperations::AA>::PatternDrafting& drafting, int fillType) {
        std::vector<std::vector<Arc>> resultPolygons;

        std::function<void(const tailor::Polygon<tailor::PolyEdgeInfo>&)> fun =
            [&](const tailor::Polygon<tailor::PolyEdgeInfo>& poly) -> void {
            std::vector<Arc> polygonEdges;
            for (const auto& edge_info : poly.edges) {
                if (edge_info.type == tailor::BoundaryType::UpperBoundary) {
                    // UpperBoundary 需要反转边
                    const auto& edge = drafting.edgeEvent[edge_info.id].edge;
                    Arc reversedEdge(edge.Point1(), edge.Point0(), -edge.Bulge(), edge.Data());
                    polygonEdges.push_back(reversedEdge);
                } else {
                    const auto& edge = drafting.edgeEvent[edge_info.id].edge;
                    polygonEdges.push_back(edge);
                }
            }
            if (!polygonEdges.empty()) {
                resultPolygons.push_back(polygonEdges);
            }
        };

        // 根据 fillType 选择正确的 Pattern
        switch (fillType) {
            case 0: // NonZeroFillType
                {
                    tailor::SymmetricDifferencePattern<tailor::NonZeroFillType, tailor::NonZeroFillType, tailor::ConnectTypeOuterFirst> pattern;
                    auto polytree = pattern.Stitch(drafting);
                    for (const auto& tree : polytree) {
                        ForEachPolyTree<tailor::PolyEdgeInfo>(tree, fun);
                    }
                }
                break;
            case 1: // EvenOddFillType
            default:
                {
                    tailor::SymmetricDifferencePattern<tailor::EvenOddFillType, tailor::EvenOddFillType, tailor::ConnectTypeOuterFirst> pattern;
                    auto polytree = pattern.Stitch(drafting);
                    for (const auto& tree : polytree) {
                        ForEachPolyTree<tailor::PolyEdgeInfo>(tree, fun);
                    }
                }
                break;
            case 2: // IgnoreFillType
                {
                    tailor::SymmetricDifferencePattern<tailor::IgnoreFillType, tailor::IgnoreFillType, tailor::ConnectTypeOuterFirst> pattern;
                    auto polytree = pattern.Stitch(drafting);
                    for (const auto& tree : polytree) {
                        ForEachPolyTree<tailor::PolyEdgeInfo>(tree, fun);
                    }
                }
                break;
        }

        return resultPolygons;
    }

    std::vector<std::vector<Arc>> BooleanOperations::ExecuteWithSpecificWinding(
        const tailor::Tailor<Arc, BooleanOperations::AA>::PatternDrafting& drafting,
        const IFillType* clipFillType,
        const IFillType* subjectFillType,
        BooleanOperation operation,
        const std::function<void(const tailor::Polygon<tailor::PolyEdgeInfo>&)>& fun) {

        qDebug() << "ExecuteWithSpecificWinding - Operation:" << static_cast<int>(operation);

        std::vector<std::vector<Arc>> resultPolygons;

        // 1. 遍历所有边，根据填充类型确定每条边的边界类型 FillType
        auto& edges = drafting.edgeEvent;
        size_t size = edges.size();
        std::vector<tailor::BoundaryType> types(size);

        // 2. 定义布尔运算函数
        auto boolOperation = [operation](tailor::BoundaryType subject, tailor::BoundaryType clipper) -> tailor::BoundaryType {
            switch (operation) {
            case BooleanOperation::Union:
                // Union: Inside if either is Inside
                if (subject == tailor::BoundaryType::Inside || clipper == tailor::BoundaryType::Inside) {
                    return tailor::BoundaryType::Inside;
                } else if (subject == tailor::BoundaryType::Outside && clipper == tailor::BoundaryType::Outside) {
                    return tailor::BoundaryType::Outside;
                } else {
                    return tailor::BoundaryType::LowerBoundary; // 默认为下边界
                }
            case BooleanOperation::Intersection:
                // Intersection: Inside only if both are Inside
                if (subject == tailor::BoundaryType::Inside && clipper == tailor::BoundaryType::Inside) {
                    return tailor::BoundaryType::Inside;
                } else if (subject == tailor::BoundaryType::Outside || clipper == tailor::BoundaryType::Outside) {
                    return tailor::BoundaryType::Outside;
                } else {
                    return tailor::BoundaryType::LowerBoundary;
                }
            case BooleanOperation::Difference:
                // Difference (Subject - Clipper): Inside if Subject is Inside and Clipper is Outside
                if (subject == tailor::BoundaryType::Inside && clipper == tailor::BoundaryType::Outside) {
                    return tailor::BoundaryType::Inside;
                } else if (subject == tailor::BoundaryType::Outside) {
                    return tailor::BoundaryType::Outside;
                } else {
                    return tailor::BoundaryType::LowerBoundary;
                }
            case BooleanOperation::XOR: {
                // XOR: Inside if exactly one is Inside
                bool subjectInside = (subject == tailor::BoundaryType::Inside);
                bool clipperInside = (clipper == tailor::BoundaryType::Inside);
                if (subjectInside != clipperInside) {
                    return tailor::BoundaryType::Inside;
                } else if (subject == tailor::BoundaryType::Outside && clipper == tailor::BoundaryType::Outside) {
                    return tailor::BoundaryType::Outside;
                } else {
                    return tailor::BoundaryType::LowerBoundary;
                }
            }
            default:
                return tailor::BoundaryType::Outside;
            }
        };

        // 3. 计算每条边的边界类型
        for (const auto& edge : edges) {
            if (!edge.end) continue;

            tailor::EdgeGroupFillStatus status;
            status.clipper.wind = edge.clipperWind;
            status.clipper.positive = 0;
            status.clipper.negitive = 0;
            status.subject.wind = edge.subjectWind;
            status.subject.positive = 0;
            status.subject.negitive = 0;

            // 统计正向和负向边
            if (edge.aggregatedEdges) {
                for (auto id : edge.aggregatedEdges->sourceEdges) {
                    const auto& srcEdge = drafting.edgeEvent[id];
                    if (srcEdge.isClipper) {
                        srcEdge.reversed ? (++status.clipper.negitive) : (++status.clipper.positive);
                    } else {
                        srcEdge.reversed ? (++status.subject.negitive) : (++status.subject.positive);
                    }
                }
            }

            // 计算边界类型
            auto subjectBoundary = (*subjectFillType)(status.subject);
            auto clipperBoundary = (*clipFillType)(status.clipper);
            auto resultBoundary = boolOperation(subjectBoundary, clipperBoundary);

            types[edge.id] = resultBoundary;
        }

        // 4. 使用 ConnectType 连接
        tailor::ConnectTypeOuterFirst connector;
        auto polys = connector.Connect(drafting, types);

        // 5. 处理结果多边形
        for (const auto& poly : polys) {
            fun(poly);
        }

        // 导出结果多边形用于调试
        std::vector<tailor_visualization::Polygon> exportPolygons;
        for (const auto& polygon : resultPolygons) {
            tailor_visualization::Polygon exportPoly;
            for (const auto& arc : polygon) {
                tailor_visualization::PolygonEdge edge;
                edge.startPoint.x = arc.Point0().x;
                edge.startPoint.y = arc.Point0().y;
                edge.endPoint.x = arc.Point1().x;
                edge.endPoint.y = arc.Point1().y;
                edge.bulge = arc.Bulge();
                exportPoly.edges.push_back(edge);
            }
            exportPolygons.push_back(exportPoly);
        }
        tailor_visualization::PolygonIO::ExportToFile(std::string("D:\\result_test.txt"), exportPolygons);

        return resultPolygons;
    }

    template<typename T>
    void BooleanOperations::ForEachPolyTreeWithDepth(const tailor::PolyTree<T>& tree,
                                 int depth,
                                 const std::function<void(const typename tailor::PolyTree<T>::PolygonType&, int)>& callback) const {
        if (callback) {
            callback(tree.polygon, depth);
        }
        for (const auto& child : tree.children) {
            ForEachPolyTreeWithDepth(child, depth + 1, callback);
        }
    }

    double BooleanOperations::CalculateSignedArea(const std::vector<Arc>& arcs) const {
        double area = 0.0;
        int n = arcs.size();

        for (int i = 0; i < n; ++i) {
            const auto& arc = arcs[i];
            const auto& p0 = arc.Point0();
            const auto& p1 = arc.Point1();

            // Shoelace formula for signed area
            area += (p0.x + p1.x) * (p1.y - p0.y);
        }

        return area * 0.5;
    }

    std::vector<PolygonWithHoleInfo> BooleanOperations::ExecuteOnlyClipPatternWithHoles(
        const IFillType* fillType,
        const IConnectType<tailor::Tailor<Arc, BooleanOperations::AA>::PatternDrafting>* connectType) {
        // 根据 connectType 参数决定使用 InnerFirst 还是 OuterFirst
        bool useInnerFirst = false;
        using Drafting = typename ArcTailor::PatternDrafting;
        if (connectType && dynamic_cast<const ConnectTypeInnerFirstWrapper<Drafting>*>(connectType)) {
            useInnerFirst = true;
        }

        // 只处理 Clip 集合的 Pattern
        if (clipPolygons_.empty()) {
            return {};
        }

        // 创建裁剪器
        ArcTailor tailor(arcAnalysis_);

        // 添加 Clip 集合的多边形
        for (const auto& clipPoly : clipPolygons_) {
            auto monotonicClip = SplitToMonotonic(clipPoly);
            tailor.AddClipper(monotonicClip.begin(), monotonicClip.end());
        }

        // 执行
        auto drafting = tailor.Execute();

        std::vector<PolygonWithHoleInfo> resultPolygons;

        std::function<void(const tailor::Polygon<tailor::PolyEdgeInfo>&, int)> fun =
            [&](const tailor::Polygon<tailor::PolyEdgeInfo>& poly, int depth) -> void {
            std::vector<Arc> polygonEdges;
            for (const auto& edge_info : poly.edges) {
                if (edge_info.type == tailor::BoundaryType::UpperBoundary) {
                    // 需要反转边
                    const auto& edge = drafting.edgeEvent[edge_info.id].edge;
                    Arc reversedEdge(edge.Point1(), edge.Point0(), -edge.Bulge(), edge.Data());
                    polygonEdges.push_back(reversedEdge);
                } else {
                    const auto& edge = drafting.edgeEvent[edge_info.id].edge;
                    polygonEdges.push_back(edge);
                }
            }
            if (!polygonEdges.empty()) {
                // 判断是否为内环：偶数层为内环
                bool isHole = (depth % 2 == 1);
                resultPolygons.push_back({polygonEdges, isHole});
            }
        };

        // 将 IFillType 转换为 FillTypeVariant
        auto fillTypeVariant = ToFillTypeVariant(fillType);

        // 使用 std::visit 来选择正确的 Pattern，同时支持 ConnectType
        std::visit([&](auto&& type) {
            using FillType = std::decay_t<decltype(type)>;

            if (useInnerFirst) {
                tailor::OnlyClipPattern<FillType, tailor::ConnectTypeInnerFirst> pattern;
                auto polytree = pattern.Stitch(drafting);
                for (const auto& tree : polytree) {
                    ForEachPolyTreeWithDepth<tailor::PolyEdgeInfo>(tree, 0, fun);
                }
            } else {
                tailor::OnlyClipPattern<FillType, tailor::ConnectTypeOuterFirst> pattern;
                auto polytree = pattern.Stitch(drafting);
                for (const auto& tree : polytree) {
                    ForEachPolyTreeWithDepth<tailor::PolyEdgeInfo>(tree, 0, fun);
                }
            }
        }, fillTypeVariant.variant);

        return resultPolygons;
    }

    std::vector<PolygonWithHoleInfo> BooleanOperations::ExecuteOnlySubjectPatternWithHoles(
        const IFillType* fillType,
        const IConnectType<tailor::Tailor<Arc, BooleanOperations::AA>::PatternDrafting>* connectType) {
        // 根据 connectType 参数决定使用 InnerFirst 还是 OuterFirst
        bool useInnerFirst = false;
        using Drafting = typename ArcTailor::PatternDrafting;
        if (connectType && dynamic_cast<const ConnectTypeInnerFirstWrapper<Drafting>*>(connectType)) {
            useInnerFirst = true;
        }

        // 只处理 Subject 集合的 Pattern
        if (subjectPolygons_.empty()) {
            return {};
        }

        // 创建裁剪器
        ArcTailor tailor(arcAnalysis_);

        // 添加 Subject 集合的多边形
        for (const auto& subjectPoly : subjectPolygons_) {
            auto monotonicSubject = SplitToMonotonic(subjectPoly);
            tailor.AddSubject(monotonicSubject.begin(), monotonicSubject.end());
        }

        // 执行
        auto drafting = tailor.Execute();

        std::vector<PolygonWithHoleInfo> resultPolygons;

        std::function<void(const tailor::Polygon<tailor::PolyEdgeInfo>&, int)> fun =
            [&](const tailor::Polygon<tailor::PolyEdgeInfo>& poly, int depth) -> void {
            std::vector<Arc> polygonEdges;
            for (const auto& edge_info : poly.edges) {
                if (edge_info.type == tailor::BoundaryType::UpperBoundary) {
                    // 需要反转边
                    const auto& edge = drafting.edgeEvent[edge_info.id].edge;
                    Arc reversedEdge(edge.Point1(), edge.Point0(), -edge.Bulge(), edge.Data());
                    polygonEdges.push_back(reversedEdge);
                } else {
                    const auto& edge = drafting.edgeEvent[edge_info.id].edge;
                    polygonEdges.push_back(edge);
                }
            }
            if (!polygonEdges.empty()) {
                // 判断是否为内环：偶数层为内环
                bool isHole = (depth % 2 == 1);
                resultPolygons.push_back({polygonEdges, isHole});
            }
        };

        // 将 IFillType 转换为 FillTypeVariant
        auto fillTypeVariant = ToFillTypeVariant(fillType);

        // 使用 std::visit 来选择正确的 Pattern，同时支持 ConnectType
        std::visit([&](auto&& type) {
            using FillType = std::decay_t<decltype(type)>;

            if (useInnerFirst) {
                tailor::OnlySubjectPattern<FillType, tailor::ConnectTypeInnerFirst> pattern;
                auto polytree = pattern.Stitch(drafting);
                for (const auto& tree : polytree) {
                    ForEachPolyTreeWithDepth<tailor::PolyEdgeInfo>(tree, 0, fun);
                }
            } else {
                tailor::OnlySubjectPattern<FillType, tailor::ConnectTypeOuterFirst> pattern;
                auto polytree = pattern.Stitch(drafting);
                for (const auto& tree : polytree) {
                    ForEachPolyTreeWithDepth<tailor::PolyEdgeInfo>(tree, 0, fun);
                }
            }
        }, fillTypeVariant.variant);

        return resultPolygons;
    }

    std::vector<PolygonWithHoleInfo> BooleanOperations::ExecuteWithHoles(
        BooleanOperation operation,
        const IFillType* clipFillType,
        const IFillType* subjectFillType,
        const IConnectType<tailor::Tailor<Arc, BooleanOperations::AA>::PatternDrafting>* connectType) {

        qDebug() << "ExecuteWithHoles - Operation:" << static_cast<int>(operation);

        // 检查是否有Clip和Subject多边形
        if (clipPolygons_.empty() && subjectPolygons_.empty()) {
            return {};
        }

        // 创建裁剪器
        ArcTailor tailor(arcAnalysis_);

        // 添加所有Clip多边形（裁剪多边形）并分割为单调弧段
        for (const auto& clipPoly : clipPolygons_) {
            auto monotonicClip = SplitToMonotonic(clipPoly);
            tailor.AddClipper(monotonicClip.begin(), monotonicClip.end());
        }

        // 添加所有Subject多边形（被裁剪多边形）并分割为单调弧段
        for (const auto& subjectPoly : subjectPolygons_) {
            auto monotonicSubject = SplitToMonotonic(subjectPoly);
            tailor.AddSubject(monotonicSubject.begin(), monotonicSubject.end());
        }

        // 执行裁剪
        auto drafting = tailor.Execute();

        std::vector<PolygonWithHoleInfo> resultPolygons;

        std::function<void(const tailor::Polygon<tailor::PolyEdgeInfo>&, int)> fun =
            [&](const tailor::Polygon<tailor::PolyEdgeInfo>& poly, int depth) -> void {
            std::vector<Arc> polygonEdges;
            for (const auto& edge_info : poly.edges) {
                if (edge_info.type == tailor::BoundaryType::UpperBoundary) {
                    // UpperBoundary需要反转边
                    const auto& edge = drafting.edgeEvent[edge_info.id].edge;
                    Arc reversedEdge(edge.Point1(), edge.Point0(), -edge.Bulge(), edge.Data());
                    polygonEdges.push_back(reversedEdge);
                } else {
                    const auto& edge = drafting.edgeEvent[edge_info.id].edge;
                    polygonEdges.push_back(edge);
                }
            }
            if (!polygonEdges.empty()) {
                // 判断是否为内环：偶数层为内环
                bool isHole = (depth % 2 == 1);
                resultPolygons.push_back({polygonEdges, isHole});
            }
        };

        // 转换FillType到FillTypeVariant
        auto clipVariant = ToFillTypeVariant(clipFillType);
        auto subjectVariant = ToFillTypeVariant(subjectFillType);

        // 根据 connectType 参数决定使用 InnerFirst 还是 OuterFirst
        bool useInnerFirst = false;
        using Drafting = typename ArcTailor::PatternDrafting;
        if (connectType && dynamic_cast<const ConnectTypeInnerFirstWrapper<Drafting>*>(connectType)) {
            useInnerFirst = true;
        }

        // 使用 std::visit 双重分发到正确的 Pattern
        auto executePattern = [&](auto&& clipType, auto&& subjectType) {
            using ClipType = std::decay_t<decltype(clipType)>;
            using SubjectType = std::decay_t<decltype(subjectType)>;

            switch (operation) {
                case BooleanOperation::Union: {
                    if (useInnerFirst) {
                        tailor::UnionPattern<SubjectType, ClipType, tailor::ConnectTypeInnerFirst> pattern;
                        auto polytree = pattern.Stitch(drafting);
                        for (const auto& tree : polytree) {
                            ForEachPolyTreeWithDepth<tailor::PolyEdgeInfo>(tree, 0, fun);
                        }
                    } else {
                        tailor::UnionPattern<SubjectType, ClipType, tailor::ConnectTypeOuterFirst> pattern;
                        auto polytree = pattern.Stitch(drafting);
                        for (const auto& tree : polytree) {
                            ForEachPolyTreeWithDepth<tailor::PolyEdgeInfo>(tree, 0, fun);
                        }
                    }
                    break;
                }
                case BooleanOperation::Intersection: {
                    if (useInnerFirst) {
                        tailor::IntersectionPattern<SubjectType, ClipType, tailor::ConnectTypeInnerFirst> pattern;
                        auto polytree = pattern.Stitch(drafting);
                        for (const auto& tree : polytree) {
                            ForEachPolyTreeWithDepth<tailor::PolyEdgeInfo>(tree, 0, fun);
                        }
                    } else {
                        tailor::IntersectionPattern<SubjectType, ClipType, tailor::ConnectTypeOuterFirst> pattern;
                        auto polytree = pattern.Stitch(drafting);
                        for (const auto& tree : polytree) {
                            ForEachPolyTreeWithDepth<tailor::PolyEdgeInfo>(tree, 0, fun);
                        }
                    }
                    break;
                }
                case BooleanOperation::Difference: {
                    if (useInnerFirst) {
                        tailor::DifferencePattern<SubjectType, ClipType, tailor::ConnectTypeInnerFirst> pattern;
                        auto polytree = pattern.Stitch(drafting);
                        for (const auto& tree : polytree) {
                            ForEachPolyTreeWithDepth<tailor::PolyEdgeInfo>(tree, 0, fun);
                        }
                    } else {
                        tailor::DifferencePattern<SubjectType, ClipType, tailor::ConnectTypeOuterFirst> pattern;
                        auto polytree = pattern.Stitch(drafting);
                        for (const auto& tree : polytree) {
                            ForEachPolyTreeWithDepth<tailor::PolyEdgeInfo>(tree, 0, fun);
                        }
                    }
                    break;
                }
                case BooleanOperation::XOR: {
                    if (useInnerFirst) {
                        tailor::SymmetricDifferencePattern<SubjectType, ClipType, tailor::ConnectTypeInnerFirst> pattern;
                        auto polytree = pattern.Stitch(drafting);
                        for (const auto& tree : polytree) {
                            ForEachPolyTreeWithDepth<tailor::PolyEdgeInfo>(tree, 0, fun);
                        }
                    } else {
                        tailor::SymmetricDifferencePattern<SubjectType, ClipType, tailor::ConnectTypeOuterFirst> pattern;
                        auto polytree = pattern.Stitch(drafting);
                        for (const auto& tree : polytree) {
                            ForEachPolyTreeWithDepth<tailor::PolyEdgeInfo>(tree, 0, fun);
                        }
                    }
                    break;
                }
                default:
                    break;
            }
        };

        // 使用 std::visit 双重分发
        std::visit([&](auto&& clipType) {
            return std::visit([&](auto&& subjectType) {
                executePattern(clipType, subjectType);
            }, subjectVariant.variant);
        }, clipVariant.variant);

        return resultPolygons;
    }


} // namespace tailor_visualization
