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

    /**
     * @brief 动态精度核心类
     * 
     * 与 tailor::PrecisionCore 有相同的接口，但精度值可以在运行时动态修改。
     * 用于替代 tailor::PrecisionCore<10> 以支持界面调整精度。
     * 
     * 精度值通过 SetPrecision() 函数设置，默认精度为 10（对应 1e-10）
     */
    struct DynamicPrecisionCore {
        // 计算 epsilon 值的辅助函数
        static double Epsilon(size_t preci) {
            double result = 1.0;
            while (preci) {
                result /= 10.0;
                preci--;
            }
            return result;
        }

        // 可动态修改的静态成员变量（在 cpp 文件中初始化）
        static double VALUE_EPSILON;
        static double POINT_EPSILON;
        static double ANGLE_EPSILON;

        // 默认精度值
        static constexpr size_t DEFAULT_PRECISION = 10;

        /**
         * @brief 设置精度
         * @param precision 精度位数（例如 10 表示 1e-10）
         */
        static void SetPrecision(size_t precision) {
            VALUE_EPSILON = Epsilon(precision);
            POINT_EPSILON = Epsilon(precision);
            ANGLE_EPSILON = Epsilon(precision);
        }

        /**
         * @brief 获取当前精度
         * @return 精度位数
         */
        static size_t GetPrecision() {
            // 通过反算 VALUE_EPSILON 来获取精度
            double eps = VALUE_EPSILON;
            size_t preci = 0;
            while (preci < 20 && eps < 0.1) { // 最多支持 20 位精度
                eps *= 10.0;
                if (eps >= 0.0999 && eps <= 0.1001) break;
                preci++;
            }
            return preci;
        }
    };

    // 类型别名定义 
    using ArcPoint = tailor::Point<double>;
    using Arc = tailor::ArcSegment<ArcPoint, double, QRgba64>;
    using ArcUtils = tailor::ArcSegmentUtils<Arc>;

    // 使用动态精度核心替代静态的 tailor::PrecisionCore<10>
    // 拆分为多步以避免模板嵌套过深导致的语法问题
    using ArcAnalyserCore = tailor::ArcSegmentAnalyserCore<Arc, DynamicPrecisionCore>;
    using ArcAnalysisWithCore = tailor::ArcAnalysis<Arc, ArcAnalyserCore>;
    using ArcTailor = tailor::Tailor<Arc, ArcAnalysisWithCore>;
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
        // 使用动态精度核心
        using AA = tailor::ArcAnalysis<Arc, tailor::ArcSegmentAnalyserCore<Arc, DynamicPrecisionCore>>;
    public:
        /**
         * @brief 构造函数
         */
        BooleanOperations();

        /**
         * @brief 设置计算精度
         * @param precision 精度位数（例如 10 表示 1e-10，数值越大精度越高）
         * @note 精度会影响算法的容差值。较低的精度（如 5）会使算法更"宽松"，
         *       较高的精度（如 15）会使算法更"严格"。默认为 10。
         */
        static void SetPrecision(size_t precision) {
            DynamicPrecisionCore::SetPrecision(precision);
        }

        /**
         * @brief 获取当前计算精度
         * @return 当前精度位数
         */
        static size_t GetPrecision() {
            return DynamicPrecisionCore::GetPrecision();
        }

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
         * @param clipFillType polygonSetB 集合的填充规则指针
         * @param subjectFillType polygonSetA 集合的填充规则指针
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
         * @param clipFillType polygonSetB 集合的填充规则指针
         * @param subjectFillType polygonSetA 集合的填充规则指针
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
         * @param clipFillType polygonSetB 集合的填充规则指针
         * @param subjectFillType polygonSetA 集合的填充规则指针
         * @param connectType 连接类型指针（可选）
         * @return 带内环标记的结果多边形集合
         */
        std::vector<PolygonWithHoleInfo> ExecuteWithHoles(
            BooleanOperation operation,
            const IFillType* clipFillType,
            const IFillType* subjectFillType,
            const IConnectType<tailor::Tailor<Arc, AA>::PatternDrafting>* connectType = nullptr);

        /**
         * @brief 执行 PolygonSetBPattern，获取 polygonSetB 多边形（非自交）
         * @param fillType 填充类型指针，支持指定环绕数
         * @return polygonSetB 多边形集合
         */
        std::vector<std::vector<Arc>> ExecuteOnlyClipPattern(const IFillType* fillType = nullptr);

        /**
         * @brief 执行 PolygonSetBPattern，获取 polygonSetB 多边形（非自交），支持 ConnectType
         * @param fillType 填充类型指针，支持指定环绕数
         * @param connectType 连接类型指针
         * @return polygonSetB 多边形集合
         */
        std::vector<std::vector<Arc>> ExecuteOnlyClipPattern(
            const IFillType* fillType,
            const IConnectType<tailor::Tailor<Arc, AA>::PatternDrafting>* connectType);

        /**
         * @brief 执行 PolygonSetBPattern，获取带内环信息的 polygonSetB 多边形
         * @param fillType 填充类型指针，支持指定环绕数
         * @param connectType 连接类型指针
         * @return 带内环标记的 polygonSetB 多边形集合
         */
        std::vector<PolygonWithHoleInfo> ExecuteOnlyClipPatternWithHoles(
            const IFillType* fillType = nullptr,
            const IConnectType<tailor::Tailor<Arc, AA>::PatternDrafting>* connectType = nullptr);

        /**
         * @brief 执行 PolygonSetAPattern，获取 polygonSetA 多边形（非自交）
         * @param fillType 填充类型指针，支持指定环绕数
         * @return polygonSetA 多边形集合
         */
        std::vector<std::vector<Arc>> ExecuteOnlySubjectPattern(const IFillType* fillType = nullptr);

        /**
         * @brief 执行 PolygonSetAPattern，获取 polygonSetA 多边形（非自交），支持 ConnectType
         * @param fillType 填充类型指针，支持指定环绕数
         * @param connectType 连接类型指针
         * @return polygonSetA 多边形集合
         */
        std::vector<std::vector<Arc>> ExecuteOnlySubjectPattern(
            const IFillType* fillType,
            const IConnectType<tailor::Tailor<Arc, AA>::PatternDrafting>* connectType);

        /**
         * @brief 执行 PolygonSetAPattern，获取带内环信息的 polygonSetA 多边形
         * @param fillType 填充类型指针，支持指定环绕数
         * @param connectType 连接类型指针
         * @return 带内环标记的 polygonSetA 多边形集合
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
         * @param clipFillType polygonSetB 集合的填充规则指针
         * @param subjectFillType polygonSetA 集合的填充规则指针
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
         * @param clipFillType polygonSetB 集合的填充规则指针
         * @param subjectFillType polygonSetA 集合的填充规则指针
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
