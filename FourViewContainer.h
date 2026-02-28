#pragma once

#include <QWidget>
#include <QGridLayout>
#include <QFrame>
#include <QComboBox>
#include <QSpinBox>
#include <memory>

#include "BooleanOperations.h"

// Forward declarations
namespace tailor_visualization {
    class IFillType;
    class NonZeroFillTypeWrapper;
    class EvenOddFillTypeWrapper;
    class IgnoreFillTypeWrapper;
    class SpecificWindingFillTypeWrapper;
    class BooleanOperations;

    // Forward declaration of ConnectType wrappers
    template<typename Drafting>
    class IConnectType;

    template<typename Drafting>
    class ConnectTypeOuterFirstWrapper;

    template<typename Drafting>
    class ConnectTypeInnerFirstWrapper;
}

class Sketch2DView;

// Type alias for IConnectType Drafting parameter
using AnalysisCore = tailor::ArcSegmentAnalyserCore<tailor_visualization::Arc, tailor::PrecisionCore<10>>;
using ConnectTypeDrafting = tailor::Tailor<tailor_visualization::Arc, tailor::ArcAnalysis<tailor_visualization::Arc ,AnalysisCore>>::PatternDrafting;

// Fill type enumeration for pattern operations
enum class PatternFillType {
    NonZero,            // Non-zero fill rule
    EvenOdd,            // Even-odd fill rule
    Ignore,             // Ignore fill rule
    SpecificWinding     // Specific winding number
};

// Connect type enumeration for pattern operations
enum class PatternConnectType {
    OuterFirst,         // Connect outer first
    InnerFirst          // Connect inner first
};

class FourViewContainer : public QWidget {
    Q_OBJECT

public:
    explicit FourViewContainer(QWidget* parent = nullptr);
    ~FourViewContainer();

    // Main view (top-left) - full functionality
    Sketch2DView* mainView() const { return m_mainView; }

    // Secondary views - view only (pan/zoom only)
    Sketch2DView* topRightView() const { return m_topRightView; }
    Sketch2DView* bottomLeftView() const { return m_bottomLeftView; }
    Sketch2DView* bottomRightView() const { return m_bottomRightView; }

    // Synchronize view state from main view to all views
    void synchronizeViews();

    // 获取当前FillType的IFillType指针
    const tailor_visualization::IFillType* getClipFillType() const;
    const tailor_visualization::IFillType* getSubjectFillType() const;

    // 获取当前ConnectType的IConnectType指针
    const tailor_visualization::IConnectType<ConnectTypeDrafting>* getTopRightConnectType() const;
    const tailor_visualization::IConnectType<ConnectTypeDrafting>* getBottomLeftConnectType() const;
    const tailor_visualization::IConnectType<ConnectTypeDrafting>* getBottomRightConnectType() const;

signals:
    void booleanOperationTriggered(int operationIndex);

private:
    void setupViews();
    void setupLayout();
    void setupBooleanComboBox();
    void setupFillTypeComboBoxes();
    void setupConnectTypeComboBoxes();
    void updateWindingSpinBoxStyle(QSpinBox* spinBox, int value);

    Sketch2DView* m_mainView = nullptr;
    Sketch2DView* m_topRightView = nullptr;
    Sketch2DView* m_bottomLeftView = nullptr;
    Sketch2DView* m_bottomRightView = nullptr;

    QFrame* m_frameBottomRight = nullptr;
    QFrame* m_frameTopRight = nullptr;
    QFrame* m_frameBottomLeft = nullptr;

    QGridLayout* m_layout = nullptr;
    QComboBox* m_booleanComboBox = nullptr;
    // QComboBox* m_booleanFillTypeComboBox = nullptr; // 已移除，使用Clip和Subject视图的FillType
    QComboBox* m_topRightFillTypeComboBox = nullptr;
    QComboBox* m_bottomLeftFillTypeComboBox = nullptr;
    QSpinBox* m_topRightWindingSpinBox = nullptr;
    QSpinBox* m_bottomLeftWindingSpinBox = nullptr;
    QComboBox* m_topRightConnectTypeComboBox = nullptr;
    QComboBox* m_bottomLeftConnectTypeComboBox = nullptr;
    QComboBox* m_bottomRightConnectTypeComboBox = nullptr;

    PatternFillType m_topRightFillType = PatternFillType::NonZero;
    PatternFillType m_bottomLeftFillType = PatternFillType::NonZero;
    int m_topRightWinding = 1;  // 默认环绕数为1
    int m_bottomLeftWinding = 1; // 默认环绕数为1
    PatternConnectType m_topRightConnectType = PatternConnectType::OuterFirst;
    PatternConnectType m_bottomLeftConnectType = PatternConnectType::OuterFirst;
    PatternConnectType m_bottomRightConnectType = PatternConnectType::OuterFirst;
    // PatternFillType m_booleanFillType = PatternFillType::EvenOdd; // 已移除，使用Clip和Subject视图的FillType

    // Current boolean operation type (default: Union = 0)
    int m_currentBooleanOperation = 0;

    // FillType 实例（用于布尔运算视图）
    std::unique_ptr<tailor_visualization::NonZeroFillTypeWrapper> m_nonZeroFillType;
    std::unique_ptr<tailor_visualization::EvenOddFillTypeWrapper> m_evenOddFillType;
    std::unique_ptr<tailor_visualization::IgnoreFillTypeWrapper> m_ignoreFillType;
    std::unique_ptr<tailor_visualization::SpecificWindingFillTypeWrapper> m_specificWindingFillTypeTopRight;
    std::unique_ptr<tailor_visualization::SpecificWindingFillTypeWrapper> m_specificWindingFillTypeBottomLeft;

    // ConnectType 实例（用于布尔运算视图）
    std::unique_ptr<tailor_visualization::ConnectTypeOuterFirstWrapper<ConnectTypeDrafting>> m_connectTypeOuterFirst;
    std::unique_ptr<tailor_visualization::ConnectTypeInnerFirstWrapper<ConnectTypeDrafting>> m_connectTypeInnerFirst;
};

