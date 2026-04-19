#pragma once

#include <vector>
#include <functional>
#include <string>
#include <memory>
#include <variant>
#include <tailor/tailor_arc_or_segment.h>
#include <tailor/tailor.h>
#include <tailor/pattern.h>
#include <qrgba64.h>

namespace tailor_visualization {

    // 类型别名定义 
    using ArcPoint = tailor::Point<double>;
    using Arc = tailor::ArcSegment<ArcPoint, double, QRgba64>;
    using ArcUtils = tailor::ArcSegmentUtils<Arc>;

    using ArcTailor = tailor::Tailor<Arc, tailor::ArcAnalysis<Arc, tailor::ArcSegmentAnalyserCore<Arc, tailor::PrecisionCore<10>>>>;
    // Note: Drafting type is defined in FourViewContainer.h for caching purposes
    // using Drafting = typename ArcTailor::PatternDrafting;

    // 前向声明
    template<typename Drafting>
    class IConnectType;

    /**
     * @brief 带有内环标记的多边形结果
     */
    struct PolygonWithHoleInfo {
        std::vector<Arc> vertices;  // 多边形的边
        bool isHole = false;        // 是否为内环（洞）
    };

    template<typename Drafting>
    class ConnectTypeOuterFirstWrapper;

    template<typename Drafting>
    class ConnectTypeInnerFirstWrapper;

    // 布尔操作类型
    enum class BooleanOperation {
        Union,      // 并集
        Intersection, // 交集
        Difference,  // 差集
        XOR          // 异或
    };

    /**
     * @brief FillType 抽象接口，用于运行时选择不同的填充规则
     */
    class IFillType {
    public:
        virtual ~IFillType() = default;
        virtual tailor::BoundaryType operator()(const tailor::EdgeFillStatus& status) const = 0;
    };

    /**
     * @brief NonZeroFillType 的具体实现
     */
    class NonZeroFillTypeWrapper : public IFillType {
    public:
        ~NonZeroFillTypeWrapper() override = default;
        tailor::BoundaryType operator()(const tailor::EdgeFillStatus& status) const override {
            tailor::NonZeroFillType filler;
            return filler(status);
        }
    };

    /**
     * @brief EvenOddFillType 的具体实现
     */
    class EvenOddFillTypeWrapper : public IFillType {
    public:
        ~EvenOddFillTypeWrapper() override = default;
        tailor::BoundaryType operator()(const tailor::EdgeFillStatus& status) const override {
            tailor::EvenOddFillType filler;
            return filler(status);
        }
    };

    /**
     * @brief IgnoreFillType 的具体实现
     */
    class IgnoreFillTypeWrapper : public IFillType {
    public:
        ~IgnoreFillTypeWrapper() override = default;
        tailor::BoundaryType operator()(const tailor::EdgeFillStatus& status) const override {
            tailor::IgnoreFillType filler;
            return filler(status);
        }
    };

    /**
     * @brief 指定环绕数的条件
     */
    template<int TargetWinding>
    class SpecificWindingCondition {
    public:
        constexpr bool operator()(tailor::Int wind) const {
            return wind == TargetWinding;
        }
    };

    /**
     * @brief 指定环绕数的 FillType (模板版本，用于 Pattern)
     */
    template<int TargetWinding>
    class SpecificWindingFillType {
    public:
        static_assert(TargetWinding != 0, "Target winding cannot be 0 (must start from outside)");

        tailor::BoundaryType operator()(const tailor::EdgeFillStatus& status) const {
            auto x = static_cast<tailor::Int>(status.positive) - static_cast<tailor::Int>(status.negitive);

            bool succ0 = status.wind == TargetWinding;
            bool succ1 = (status.wind + x) == TargetWinding;

            if (succ0 && succ1) {
                return tailor::BoundaryType::Inside;
            } else if (succ0) {
                return tailor::BoundaryType::UpperBoundary;
            } else if (succ1) {
                return tailor::BoundaryType::LowerBoundary;
            } else {
                return tailor::BoundaryType::Outside;
            }
        }
    };

    /**
     * @brief 指定环绕数的 FillType 具体实现 (运行时包装器)
     */
    class SpecificWindingFillTypeWrapper : public IFillType {
    public:
        explicit SpecificWindingFillTypeWrapper(int targetWinding) : targetWinding_(targetWinding) {}

        ~SpecificWindingFillTypeWrapper() override = default;

        tailor::BoundaryType operator()(const tailor::EdgeFillStatus& status) const override {
            auto x = static_cast<tailor::Int>(status.positive) - static_cast<tailor::Int>(status.negitive);

            bool succ0 = status.wind == targetWinding_;
            bool succ1 = (status.wind + x) == targetWinding_;

            if (succ0 && succ1) {
                return tailor::BoundaryType::Inside;
            } else if (succ0) {
                return tailor::BoundaryType::UpperBoundary;
            } else if (succ1) {
                return tailor::BoundaryType::LowerBoundary;
            } else {
                return tailor::BoundaryType::Outside;
            }
        }

        int getTargetWinding() const { return targetWinding_; }

    private:
        int targetWinding_;
    };

    /**
     * @brief FillType variant，支持所有可能的FillType组合
     * 用于编译时类型分发到tailor的Pattern模板
     */
    struct FillTypeVariant {
        template<typename T>
        FillTypeVariant(T&& value) : variant(std::forward<T>(value)) {}

        std::variant <
            tailor::NonZeroFillType,
            tailor::EvenOddFillType,
            tailor::IgnoreFillType,
            SpecificWindingFillType<1>,
            SpecificWindingFillType<2>,
            SpecificWindingFillType<3>,
            SpecificWindingFillType<4>,
            SpecificWindingFillType<5>,
            SpecificWindingFillType<6>,
            SpecificWindingFillType<7>,
            SpecificWindingFillType<8>,
            SpecificWindingFillType<9>,
            SpecificWindingFillType<10>,
            SpecificWindingFillType<-1>,
            SpecificWindingFillType<-2>,
            SpecificWindingFillType<-3>,
            SpecificWindingFillType<-4>,
            SpecificWindingFillType<-5>,
            SpecificWindingFillType<-6>,
            SpecificWindingFillType<-7>,
            SpecificWindingFillType<-8>,
            SpecificWindingFillType<-9>,
            SpecificWindingFillType<-10>
        > variant;
    };

    /**
     * @brief 将IFillType指针转换为FillTypeVariant
     * 通过运行时分发将虚接口转换为编译时类型
     */
    inline FillTypeVariant ToFillTypeVariant(const IFillType* fillType) {
        if (dynamic_cast<const NonZeroFillTypeWrapper*>(fillType)) {
            return FillTypeVariant(tailor::NonZeroFillType{});
        } else if (dynamic_cast<const EvenOddFillTypeWrapper*>(fillType)) {
            return FillTypeVariant(tailor::EvenOddFillType{});
        } else if (dynamic_cast<const IgnoreFillTypeWrapper*>(fillType)) {
            return FillTypeVariant(tailor::IgnoreFillType{});
        } else if (const auto* specific = dynamic_cast<const SpecificWindingFillTypeWrapper*>(fillType)) {
            int winding = specific->getTargetWinding();
            switch (winding) {
            case 1: return FillTypeVariant(SpecificWindingFillType<1>{});
            case 2: return FillTypeVariant(SpecificWindingFillType<2>{});
            case 3: return FillTypeVariant(SpecificWindingFillType<3>{});
            case 4: return FillTypeVariant(SpecificWindingFillType<4>{});
            case 5: return FillTypeVariant(SpecificWindingFillType<5>{});
            case 6: return FillTypeVariant(SpecificWindingFillType<6>{});
            case 7: return FillTypeVariant(SpecificWindingFillType<7>{});
            case 8: return FillTypeVariant(SpecificWindingFillType<8>{});
            case 9: return FillTypeVariant(SpecificWindingFillType<9>{});
            case 10: return FillTypeVariant(SpecificWindingFillType<10>{});
            case -1: return FillTypeVariant(SpecificWindingFillType<-1>{});
            case -2: return FillTypeVariant(SpecificWindingFillType<-2>{});
            case -3: return FillTypeVariant(SpecificWindingFillType<-3>{});
            case -4: return FillTypeVariant(SpecificWindingFillType<-4>{});
            case -5: return FillTypeVariant(SpecificWindingFillType<-5>{});
            case -6: return FillTypeVariant(SpecificWindingFillType<-6>{});
            case -7: return FillTypeVariant(SpecificWindingFillType<-7>{});
            case -8: return FillTypeVariant(SpecificWindingFillType<-8>{});
            case -9: return FillTypeVariant(SpecificWindingFillType<-9>{});
            case -10: return FillTypeVariant(SpecificWindingFillType<-10>{});
            default: return FillTypeVariant(tailor::NonZeroFillType{}); // 默认回退
            }
        }
        return FillTypeVariant(tailor::NonZeroFillType{});
    }

    /**
     * @brief ConnectType 抽象接口，用于运行时选择不同的连接方式
     */
    template<typename Drafting>
    class IConnectType {
    public:
        virtual ~IConnectType() = default;
        virtual std::vector<tailor::Polygon<tailor::PolyEdgeInfo>> Connect(
            const Drafting& drafting,
            std::vector<tailor::BoundaryType> types) const = 0;
    };

    /**
     * @brief ConnectTypeOuterFirst 的具体实现
     */
    template<typename Drafting>
    class ConnectTypeOuterFirstWrapper : public IConnectType<Drafting> {
    public:
        std::vector<tailor::Polygon<tailor::PolyEdgeInfo>> Connect(
            const Drafting& drafting,
            std::vector<tailor::BoundaryType> types) const override {
            tailor::ConnectTypeOuterFirst connector;
            return connector.Connect(drafting, std::move(types));
        }
    };

    /**
     * @brief ConnectTypeInnerFirst 的具体实现
     */
    template<typename Drafting>
    class ConnectTypeInnerFirstWrapper : public IConnectType<Drafting> {
    public:
        std::vector<tailor::Polygon<tailor::PolyEdgeInfo>> Connect(
            const Drafting& drafting,
            std::vector<tailor::BoundaryType> types) const override {
            tailor::ConnectTypeInnerFirst connector;
            return connector.Connect(drafting, std::move(types));
        }
    };

    /**
     * @brief 布尔运算封装类
     * 提供路径多边形的布尔运算功能，支持并集、交集、差集和异或操作
     */
    class BooleanOperations {
        using AA = tailor::ArcAnalysis<Arc, tailor::ArcSegmentAnalyserCore<Arc, tailor::PrecisionCore<10>>>;
    public:
        /**
         * @brief 构造函数
         */
        BooleanOperations();

        /**
         * @brief 从弧段数组添加多边形
         * @param arcs 弧段数组
         */
        void AddPolygonFromArcs(const std::vector<Arc>& arcs);

        /**
         * @brief 添加 Clip 多边形（裁剪多边形）
         * @param arcs 弧段数组
         */
        void AddClipPolygon(const std::vector<Arc>& arcs);

        /**
         * @brief 添加 Subject 多边形（被裁剪多边形）
         * @param arcs 弧段数组
         */
        void AddSubjectPolygon(const std::vector<Arc>& arcs);

        /**
         * @brief 批量添加多个 Clip 多边形
         * @param polygons 多边形集合
         */
        void AddClipPolygons(const std::vector<std::vector<Arc>>& polygons);

        /**
         * @brief 批量添加多个 Subject 多边形
         * @param polygons 多边形集合
         */
        void AddSubjectPolygons(const std::vector<std::vector<Arc>>& polygons);

        /**
         * @brief 获取 Clip 多边形数量
         * @return Clip 多边形数量
         */
        size_t GetClipPolygonCount() const { return clipPolygons_.size(); }

        /**
         * @brief 获取 Subject 多边形数量
         * @return Subject 多边形数量
         */
        size_t GetSubjectPolygonCount() const { return subjectPolygons_.size(); }

        /**
         * @brief 获取添加的多边形数量（向后兼容）
         * @return 多边形数量
         */
        size_t GetPolygonCount() const { return GetClipPolygonCount() + GetSubjectPolygonCount(); }

        /**
         * @brief 执行布尔运算
         * @param operation 布尔操作类型
         * @param clipFillType Clip集合的填充规则指针
         * @param subjectFillType Subject集合的填充规则指针
         * @return 结果多边形集合
         */
        std::vector<std::vector<Arc>> Execute(
            BooleanOperation operation,
            const IFillType* clipFillType,
            const IFillType* subjectFillType);

        /**
         * @brief 执行布尔运算（向后兼容，使用单一fillType）
         * @param operation 布尔操作类型
         * @param fillType 填充类型：0=NonZero, 1=EvenOdd, 2=Ignore（默认 EvenOdd）
         * @return 结果多边形集合
         */
        std::vector<std::vector<Arc>> Execute(BooleanOperation operation, int fillType = 1);

        /**
         * @brief 执行布尔运算，支持 ConnectType
         * @param operation 布尔操作类型
         * @param clipFillType Clip集合的填充规则指针
         * @param subjectFillType Subject集合的填充规则指针
         * @param connectType 连接类型指针（可选）
         * @return 结果多边形集合
         */
        std::vector<std::vector<Arc>> Execute(
            BooleanOperation operation,
            const IFillType* clipFillType,
            const IFillType* subjectFillType,
            const IConnectType<tailor::Tailor<Arc, AA>::PatternDrafting>* connectType);

        /**
         * @brief 执行布尔运算，获取带内环信息的结果
         * @param operation 布尔操作类型
         * @param clipFillType Clip集合的填充规则指针
         * @param subjectFillType Subject集合的填充规则指针
         * @param connectType 连接类型指针（可选）
         * @return 带内环标记的结果多边形集合
         */
        std::vector<PolygonWithHoleInfo> ExecuteWithHoles(
            BooleanOperation operation,
            const IFillType* clipFillType,
            const IFillType* subjectFillType,
            const IConnectType<tailor::Tailor<Arc, AA>::PatternDrafting>* connectType = nullptr);

        /**
         * @brief 执行 OnlyClipPattern，获取 Clip 多边形（非自交）
         * @param fillType 填充类型指针，支持指定环绕数
         * @return Clip 多边形集合
         */
        std::vector<std::vector<Arc>> ExecuteOnlyClipPattern(const IFillType* fillType = nullptr);

        /**
         * @brief 执行 OnlyClipPattern，获取 Clip 多边形（非自交），支持 ConnectType
         * @param fillType 填充类型指针，支持指定环绕数
         * @param connectType 连接类型指针
         * @return Clip 多边形集合
         */
        std::vector<std::vector<Arc>> ExecuteOnlyClipPattern(
            const IFillType* fillType,
            const IConnectType<tailor::Tailor<Arc, AA>::PatternDrafting>* connectType);

        /**
         * @brief 执行 OnlyClipPattern，获取带内环信息的 Clip 多边形
         * @param fillType 填充类型指针，支持指定环绕数
         * @param connectType 连接类型指针
         * @return 带内环标记的 Clip 多边形集合
         */
        std::vector<PolygonWithHoleInfo> ExecuteOnlyClipPatternWithHoles(
            const IFillType* fillType = nullptr,
            const IConnectType<tailor::Tailor<Arc, AA>::PatternDrafting>* connectType = nullptr);

        /**
         * @brief 执行 OnlySubjectPattern，获取 Subject 多边形（非自交）
         * @param fillType 填充类型指针，支持指定环绕数
         * @return Subject 多边形集合
         */
        std::vector<std::vector<Arc>> ExecuteOnlySubjectPattern(const IFillType* fillType = nullptr);

        /**
         * @brief 执行 OnlySubjectPattern，获取 Subject 多边形（非自交），支持 ConnectType
         * @param fillType 填充类型指针，支持指定环绕数
         * @param connectType 连接类型指针
         * @return Subject 多边形集合
         */
        std::vector<std::vector<Arc>> ExecuteOnlySubjectPattern(
            const IFillType* fillType,
            const IConnectType<tailor::Tailor<Arc, AA>::PatternDrafting>* connectType);

        /**
         * @brief 执行 OnlySubjectPattern，获取带内环信息的 Subject 多边形
         * @param fillType 填充类型指针，支持指定环绕数
         * @param connectType 连接类型指针
         * @return 带内环标记的 Subject 多边形集合
         */
        std::vector<PolygonWithHoleInfo> ExecuteOnlySubjectPatternWithHoles(
            const IFillType* fillType = nullptr,
            const IConnectType<tailor::Tailor<Arc, AA>::PatternDrafting>* connectType = nullptr);

        /**
         * @brief 创建并返回 Drafting（用于缓存）
         * @return Drafting 对象
         */
        typename ArcTailor::PatternDrafting CreateDrafting();

        /**
         * @brief 清空所有多边形
         */
        void Clear();

        /**
         * @brief 将弧段分割为单调弧段
         * @param arcs 输入弧段
         * @return 分割后的弧段
         */
        std::vector<Arc> SplitToMonotonic(const std::vector<Arc>& arcs);

        /**
         * @brief 递归遍历多边形树
         * @param tree 多边形树
         * @param callback 处理多边形的回调
         */
        template<typename T>
        void ForEachPolyTree(const tailor::PolyTree<T>& tree,
            const std::function<void(const typename tailor::PolyTree<T>::PolygonType&)>& callback) const;

        /**
         * @brief 递归遍历多边形树，带层级信息
         * @param tree 多边形树
         * @param depth 当前层级
         * @param callback 处理多边形的回调，传递多边形和层级
         */
        template<typename T>
        void ForEachPolyTreeWithDepth(const tailor::PolyTree<T>& tree,
            int depth,
            const std::function<void(const typename tailor::PolyTree<T>::PolygonType&, int)>& callback) const;

        /**
         * @brief 计算多边形的带符号面积（用于判断方向）
         * @param arcs 多边形的弧段
         * @return 带符号面积，负值表示顺时针（内环）
         */
        double CalculateSignedArea(const std::vector<Arc>& arcs) const;

    private:
        /**
         * @brief 使用 drafting 执行指定的布尔运算
         * @param drafting 裁剪结果
         * @param operation 布尔操作类型
         * @param fillType 填充类型：0=NonZero, 1=EvenOdd, 2=Ignore
         * @return 结果多边形集合
         */
        std::vector<std::vector<Arc>> ExecuteWithDrafting(
            const typename ArcTailor::PatternDrafting& drafting,
            BooleanOperation operation, int fillType = 1);

        /**
         * @brief 使用 drafting 和指定的填充规则执行布尔运算
         * @param drafting 裁剪结果
         * @param operation 布尔操作类型
         * @param clipFillType Clip集合的填充规则指针
         * @param subjectFillType Subject集合的填充规则指针
         * @return 结果多边形集合
         */
        std::vector<std::vector<Arc>> ExecuteWithFillTypes(
            const typename ArcTailor::PatternDrafting& drafting,
            BooleanOperation operation,
            const IFillType* clipFillType,
            const IFillType* subjectFillType);

        /**
         * @brief 使用 drafting 和指定的填充规则、连接方式执行布尔运算
         * @param drafting 裁剪结果
         * @param operation 布尔操作类型
         * @param clipFillType Clip集合的填充规则指针
         * @param subjectFillType Subject集合的填充规则指针
         * @param connectType 连接方式指针
         * @return 结果多边形集合
         */
        std::vector<std::vector<Arc>> ExecuteWithFillTypesAndConnectType(
            const typename ArcTailor::PatternDrafting& drafting,
            BooleanOperation operation,
            const IFillType* clipFillType,
            const IFillType* subjectFillType,
            const IConnectType<tailor::Tailor<Arc, AA>::PatternDrafting>* connectType);

    private:
        /**
         * @brief 处理指定环绕数的布尔运算（运行时实现）
         */
        std::vector<std::vector<Arc>> ExecuteWithSpecificWinding(
            const typename ArcTailor::PatternDrafting& drafting,
            const IFillType* clipFillType,
            const IFillType* subjectFillType,
            BooleanOperation operation,
            const std::function<void(const tailor::Polygon<tailor::PolyEdgeInfo>&)>& fun);

        /**
         * @brief 执行并集操作
         * @param drafting 裁剪结果
         * @param fillType 填充类型：0=NonZero, 1=EvenOdd, 2=Ignore
         * @return 结果多边形集合
         */
        std::vector<std::vector<Arc>> ExecuteUnion(const typename ArcTailor::PatternDrafting& drafting, int fillType = 1);

        /**
         * @brief 执行交集操作
         * @param drafting 裁剪结果
         * @param fillType 填充类型：0=NonZero, 1=EvenOdd, 2=Ignore
         * @return 结果多边形集合
         */
        std::vector<std::vector<Arc>> ExecuteIntersection(const typename ArcTailor::PatternDrafting& drafting, int fillType = 1);

        /**
         * @brief 执行差集操作
         * @param drafting 裁剪结果
         * @param fillType 填充类型：0=NonZero, 1=EvenOdd, 2=Ignore
         * @return 结果多边形集合
         */
        std::vector<std::vector<Arc>> ExecuteDifference(const typename ArcTailor::PatternDrafting& drafting, int fillType = 1);

        /**
         * @brief 执行异或操作
         * @param drafting 裁剪结果
         * @param fillType 填充类型：0=NonZero, 1=EvenOdd, 2=Ignore
         * @return 结果多边形集合
         */
        std::vector<std::vector<Arc>> ExecuteXOR(const typename ArcTailor::PatternDrafting& drafting, int fillType = 1);

        // 数据成员
        AA arcAnalysis_;
        std::vector<std::vector<Arc>> clipPolygons_;     // Clip 集合（裁剪多边形）
        std::vector<std::vector<Arc>> subjectPolygons_;   // Subject 集合（被裁剪多边形）

        // 向后兼容：polygons_ 指向 clipPolygons_
        std::vector<std::vector<Arc>>& polygons_ = clipPolygons_;
    };

} // namespace tailor_visualization
