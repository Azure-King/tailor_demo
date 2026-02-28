#include "FourViewContainer.h"
#include "Sketch2DView.h"
#include "BooleanOperations.h"

#include <QFrame>
#include <QVBoxLayout>
#include <QOverload>
#include <QSet>

void FourViewContainer::updateWindingSpinBoxStyle(QSpinBox* spinBox, int value) {
    if (value == 0) {
        spinBox->setStyleSheet("color: red;");
    } else {
        spinBox->setStyleSheet("");
    }
}


FourViewContainer::FourViewContainer(QWidget* parent)
    : QWidget(parent) {
    m_nonZeroFillType = std::make_unique<tailor_visualization::NonZeroFillTypeWrapper>();
    m_evenOddFillType = std::make_unique<tailor_visualization::EvenOddFillTypeWrapper>();
    m_ignoreFillType = std::make_unique<tailor_visualization::IgnoreFillTypeWrapper>();
    m_specificWindingFillTypeTopRight = std::make_unique<tailor_visualization::SpecificWindingFillTypeWrapper>(m_topRightWinding);
    m_specificWindingFillTypeBottomLeft = std::make_unique<tailor_visualization::SpecificWindingFillTypeWrapper>(m_bottomLeftWinding);
    m_connectTypeOuterFirst = std::make_unique<tailor_visualization::ConnectTypeOuterFirstWrapper<ConnectTypeDrafting>>();
    m_connectTypeInnerFirst = std::make_unique<tailor_visualization::ConnectTypeInnerFirstWrapper<ConnectTypeDrafting>>();

    setupViews();
    setupLayout();
    setupBooleanComboBox();
    setupFillTypeComboBoxes();
    setupConnectTypeComboBoxes();
}

FourViewContainer::~FourViewContainer() = default;

void FourViewContainer::setupViews() {
    // Create all four views
    m_mainView = new Sketch2DView(this);
    m_topRightView = new Sketch2DView(this);
    m_bottomLeftView = new Sketch2DView(this);
    m_bottomRightView = new Sketch2DView(this);

    // Set secondary views to read-only mode
    // They can only pan and zoom, not create or modify curves
    m_topRightView->setReadOnly(true);
    m_bottomLeftView->setReadOnly(true);
    m_bottomRightView->setReadOnly(true);

    // Connect main view changes to secondary views for synchronization
    connect(m_mainView, &Sketch2DView::polylineAdded, this, &FourViewContainer::synchronizeViews);
    connect(m_mainView, &Sketch2DView::polygonAdded, this, &FourViewContainer::synchronizeViews);
    connect(m_mainView, &Sketch2DView::polylineRemoved, this, &FourViewContainer::synchronizeViews);
    connect(m_mainView, &Sketch2DView::polygonRemoved, this, &FourViewContainer::synchronizeViews);
    connect(m_mainView, &Sketch2DView::selectionChanged, this, &FourViewContainer::synchronizeViews);
    connect(m_mainView, &Sketch2DView::polylineModified, this, &FourViewContainer::synchronizeViews);
    connect(m_mainView, &Sketch2DView::polygonModified, this, &FourViewContainer::synchronizeViews);
    connect(m_mainView, &Sketch2DView::polygonRoleChanged, this, &FourViewContainer::synchronizeViews);
    connect(m_mainView, &Sketch2DView::polygonColorChanged, this, &FourViewContainer::synchronizeViews);
}

void FourViewContainer::setupLayout() {
    m_layout = new QGridLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(2);

    // Create frames for each view with borders
    auto* frameMain = new QFrame(this);
    frameMain->setFrameShape(QFrame::StyledPanel);
    frameMain->setFrameShadow(QFrame::Sunken);
    auto* layoutMain = new QVBoxLayout(frameMain);
    layoutMain->setContentsMargins(0, 0, 0, 0);
    layoutMain->addWidget(m_mainView);

    m_frameTopRight = new QFrame(this);
    m_frameTopRight->setFrameShape(QFrame::StyledPanel);
    m_frameTopRight->setFrameShadow(QFrame::Sunken);
    auto* layoutTopRight = new QVBoxLayout(m_frameTopRight);
    layoutTopRight->setContentsMargins(0, 0, 0, 0);
    layoutTopRight->addWidget(m_topRightView);

    m_frameBottomLeft = new QFrame(this);
    m_frameBottomLeft->setFrameShape(QFrame::StyledPanel);
    m_frameBottomLeft->setFrameShadow(QFrame::Sunken);
    auto* layoutBottomLeft = new QVBoxLayout(m_frameBottomLeft);
    layoutBottomLeft->setContentsMargins(0, 0, 0, 0);
    layoutBottomLeft->addWidget(m_bottomLeftView);

    m_frameBottomRight = new QFrame(this);
    m_frameBottomRight->setFrameShape(QFrame::StyledPanel);
    m_frameBottomRight->setFrameShadow(QFrame::Sunken);
    auto* layoutBottomRight = new QVBoxLayout(m_frameBottomRight);
    layoutBottomRight->setContentsMargins(0, 0, 0, 0);
    layoutBottomRight->addWidget(m_bottomRightView);

    // Arrange in 2x2 grid
    m_layout->addWidget(frameMain, 0, 0);
    m_layout->addWidget(m_frameTopRight, 0, 1);
    m_layout->addWidget(m_frameBottomLeft, 1, 0);
    m_layout->addWidget(m_frameBottomRight, 1, 1);

    // Equal sizing
    m_layout->setColumnStretch(0, 1);
    m_layout->setColumnStretch(1, 1);
    m_layout->setRowStretch(0, 1);
    m_layout->setRowStretch(1, 1);
}

void FourViewContainer::setupBooleanComboBox() {
    // Create combo box for boolean operations
    m_booleanComboBox = new QComboBox(m_frameBottomRight);
    m_booleanComboBox->addItem("Union", 0); // tailor_visualization::BooleanOperation::Union
    m_booleanComboBox->addItem("Intersection", 1); // tailor_visualization::BooleanOperation::Intersection
    m_booleanComboBox->addItem("Difference", 2); // tailor_visualization::BooleanOperation::Difference
    m_booleanComboBox->addItem("XOR", 3); // tailor_visualization::BooleanOperation::XOR

    // Set default to Union
    m_booleanComboBox->setCurrentIndex(0);

    // Position combo box on top-left of bottom-right view
    m_booleanComboBox->move(5, 5);
    m_booleanComboBox->raise();
    m_booleanComboBox->setMinimumWidth(120);

    // Connect to signal
    connect(m_booleanComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, [this](int index) {
            m_currentBooleanOperation = m_booleanComboBox->itemData(index).toInt();
            emit booleanOperationTriggered(m_booleanComboBox->itemData(index).toInt());
        });

    // Note: 布尔运算视图的 FillType 下拉框已被移除，现在使用 Clip 和 Subject 视图分别选择的 FillType
}

void FourViewContainer::setupFillTypeComboBoxes() {
    // Setup top-right fill type combo box (for Subject)
    m_topRightFillTypeComboBox = new QComboBox(m_frameTopRight);
    m_topRightFillTypeComboBox->addItem("NonZero", static_cast<int>(PatternFillType::NonZero));
    m_topRightFillTypeComboBox->addItem("EvenOdd", static_cast<int>(PatternFillType::EvenOdd));
    m_topRightFillTypeComboBox->addItem("Ignore", static_cast<int>(PatternFillType::Ignore));
    m_topRightFillTypeComboBox->addItem("Specific Winding", static_cast<int>(PatternFillType::SpecificWinding));
    m_topRightFillTypeComboBox->setCurrentIndex(0); // Default: NonZero
    m_topRightFillTypeComboBox->move(5, 5);
    m_topRightFillTypeComboBox->raise();
    m_topRightFillTypeComboBox->setMinimumWidth(130);

    // Setup top-right winding spin box
    m_topRightWindingSpinBox = new QSpinBox(m_frameTopRight);
    m_topRightWindingSpinBox->setRange(-10, 10);
    m_topRightWindingSpinBox->setValue(m_topRightWinding);
    m_topRightWindingSpinBox->move(140, 5);
    m_topRightWindingSpinBox->raise();
    m_topRightWindingSpinBox->setMinimumWidth(50);
    m_topRightWindingSpinBox->setToolTip("Specific winding number");
    m_topRightWindingSpinBox->hide(); // 初始隐藏
    updateWindingSpinBoxStyle(m_topRightWindingSpinBox, m_topRightWinding);

    // Connect to signal
    connect(m_topRightFillTypeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, [this](int index) {
            m_topRightFillType = static_cast<PatternFillType>(m_topRightFillTypeComboBox->itemData(index).toInt());
            // 显示或隐藏环绕数输入框（仅用于布尔运算视图）
            if (m_topRightFillType == PatternFillType::SpecificWinding) {
                m_topRightWindingSpinBox->show();
                // 重新创建 SpecificWindingFillTypeWrapper
                m_specificWindingFillTypeTopRight = std::make_unique<tailor_visualization::SpecificWindingFillTypeWrapper>(m_topRightWinding);
            } else {
                m_topRightWindingSpinBox->hide();
            }
            // Subject视图使用IFillType*接口
            m_topRightView->executeSubjectPattern(getSubjectFillType(), getTopRightConnectType());
            // 同时更新布尔运算视图
            m_bottomRightView->setPolylines(m_mainView->polylines());
            m_bottomRightView->setPolygons(m_mainView->polygons());
            m_bottomRightView->setSelectedPolygon(m_mainView->selectedPolygonIndex());
            m_bottomRightView->setTool(m_mainView->tool());
            m_bottomRightView->setClipPolygons(m_mainView->clipPolygons());
            m_bottomRightView->setClipPolylines(m_mainView->clipPolylines());
            m_bottomRightView->setSubjectPolygons(m_mainView->subjectPolygons());
            m_bottomRightView->setSubjectPolylines(m_mainView->subjectPolylines());
            m_bottomRightView->executeBooleanOperation(
                static_cast<tailor_visualization::BooleanOperation>(m_currentBooleanOperation),
                getClipFillType(),
                getSubjectFillType(),
                getBottomRightConnectType()
            );
            m_bottomRightView->update();
        });

    // Connect winding spin box to signal
    connect(m_topRightWindingSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
        this, [this](int value) {
            m_topRightWinding = value;
            updateWindingSpinBoxStyle(m_topRightWindingSpinBox, value);
            // 重新创建 SpecificWindingFillTypeWrapper
            m_specificWindingFillTypeTopRight = std::make_unique<tailor_visualization::SpecificWindingFillTypeWrapper>(m_topRightWinding);
            // 更新 Subject 视图
            m_topRightView->executeSubjectPattern(getSubjectFillType(), getTopRightConnectType());
            m_topRightView->update();
            // 更新布尔运算视图
            m_bottomRightView->setPolylines(m_mainView->polylines());
            m_bottomRightView->setPolygons(m_mainView->polygons());
            m_bottomRightView->setSelectedPolygon(m_mainView->selectedPolygonIndex());
            m_bottomRightView->setTool(m_mainView->tool());
            m_bottomRightView->setClipPolygons(m_mainView->clipPolygons());
            m_bottomRightView->setClipPolylines(m_mainView->clipPolylines());
            m_bottomRightView->setSubjectPolygons(m_mainView->subjectPolygons());
            m_bottomRightView->setSubjectPolylines(m_mainView->subjectPolylines());
            m_bottomRightView->executeBooleanOperation(
                static_cast<tailor_visualization::BooleanOperation>(m_currentBooleanOperation),
                getClipFillType(),
                getSubjectFillType(),
                getBottomRightConnectType()
            );
            m_bottomRightView->update();
        });

    // Setup bottom-left fill type combo box (for Clip)
    m_bottomLeftFillTypeComboBox = new QComboBox(m_frameBottomLeft);
    m_bottomLeftFillTypeComboBox->addItem("NonZero", static_cast<int>(PatternFillType::NonZero));
    m_bottomLeftFillTypeComboBox->addItem("EvenOdd", static_cast<int>(PatternFillType::EvenOdd));
    m_bottomLeftFillTypeComboBox->addItem("Ignore", static_cast<int>(PatternFillType::Ignore));
    m_bottomLeftFillTypeComboBox->addItem("Specific Winding", static_cast<int>(PatternFillType::SpecificWinding));
    m_bottomLeftFillTypeComboBox->setCurrentIndex(0); // Default: NonZero
    m_bottomLeftFillTypeComboBox->move(5, 5);
    m_bottomLeftFillTypeComboBox->raise();
    m_bottomLeftFillTypeComboBox->setMinimumWidth(130);

    // Setup bottom-left winding spin box
    m_bottomLeftWindingSpinBox = new QSpinBox(m_frameBottomLeft);
    m_bottomLeftWindingSpinBox->setRange(-10, 10);
    m_bottomLeftWindingSpinBox->setValue(m_bottomLeftWinding);
    m_bottomLeftWindingSpinBox->move(140, 5);
    m_bottomLeftWindingSpinBox->raise();
    m_bottomLeftWindingSpinBox->setMinimumWidth(50);
    m_bottomLeftWindingSpinBox->setToolTip("Specific winding number");
    m_bottomLeftWindingSpinBox->hide(); // 初始隐藏
    updateWindingSpinBoxStyle(m_bottomLeftWindingSpinBox, m_bottomLeftWinding);

    // Connect to signal
    connect(m_bottomLeftFillTypeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, [this](int index) {
            m_bottomLeftFillType = static_cast<PatternFillType>(m_bottomLeftFillTypeComboBox->itemData(index).toInt());
            // 显示或隐藏环绕数输入框（仅用于布尔运算视图）
            if (m_bottomLeftFillType == PatternFillType::SpecificWinding) {
                m_bottomLeftWindingSpinBox->show();
                // 重新创建 SpecificWindingFillTypeWrapper
                m_specificWindingFillTypeBottomLeft = std::make_unique<tailor_visualization::SpecificWindingFillTypeWrapper>(m_bottomLeftWinding);
            } else {
                m_bottomLeftWindingSpinBox->hide();
            }
            // Clip视图使用IFillType*接口
            m_bottomLeftView->executeClipPattern(getClipFillType(), getBottomLeftConnectType());
            // 同时更新布尔运算视图
            m_bottomRightView->setPolylines(m_mainView->polylines());
            m_bottomRightView->setPolygons(m_mainView->polygons());
            m_bottomRightView->setSelectedPolygon(m_mainView->selectedPolygonIndex());
            m_bottomRightView->setTool(m_mainView->tool());
            m_bottomRightView->setClipPolygons(m_mainView->clipPolygons());
            m_bottomRightView->setClipPolylines(m_mainView->clipPolylines());
            m_bottomRightView->setSubjectPolygons(m_mainView->subjectPolygons());
            m_bottomRightView->setSubjectPolylines(m_mainView->subjectPolylines());
            m_bottomRightView->executeBooleanOperation(
                static_cast<tailor_visualization::BooleanOperation>(m_currentBooleanOperation),
                getClipFillType(),
                getSubjectFillType(),
                getBottomRightConnectType()
            );
            m_bottomRightView->update();
        });

    // Connect winding spin box to signal
    connect(m_bottomLeftWindingSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
        this, [this](int value) {
            m_bottomLeftWinding = value;
            updateWindingSpinBoxStyle(m_bottomLeftWindingSpinBox, value);
            // 重新创建 SpecificWindingFillTypeWrapper
            m_specificWindingFillTypeBottomLeft = std::make_unique<tailor_visualization::SpecificWindingFillTypeWrapper>(m_bottomLeftWinding);
            // 更新 Clip 视图
            m_bottomLeftView->executeClipPattern(getClipFillType(), getBottomLeftConnectType());
            m_bottomLeftView->update();
            // 更新布尔运算视图
            m_bottomRightView->setPolylines(m_mainView->polylines());
            m_bottomRightView->setPolygons(m_mainView->polygons());
            m_bottomRightView->setSelectedPolygon(m_mainView->selectedPolygonIndex());
            m_bottomRightView->setTool(m_mainView->tool());
            m_bottomRightView->setClipPolygons(m_mainView->clipPolygons());
            m_bottomRightView->setClipPolylines(m_mainView->clipPolylines());
            m_bottomRightView->setSubjectPolygons(m_mainView->subjectPolygons());
            m_bottomRightView->setSubjectPolylines(m_mainView->subjectPolylines());
            m_bottomRightView->executeBooleanOperation(
                static_cast<tailor_visualization::BooleanOperation>(m_currentBooleanOperation),
                getClipFillType(),
                getSubjectFillType(),
                getBottomRightConnectType()
            );
            m_bottomRightView->update();
        });
}

void FourViewContainer::setupConnectTypeComboBoxes() {
    // Setup top-right connect type combo box (for Subject)
    m_topRightConnectTypeComboBox = new QComboBox(m_frameTopRight);
    m_topRightConnectTypeComboBox->addItem("Outer First", static_cast<int>(PatternConnectType::OuterFirst));
    m_topRightConnectTypeComboBox->addItem("Inner First", static_cast<int>(PatternConnectType::InnerFirst));
    m_topRightConnectTypeComboBox->setCurrentIndex(0); // Default: Outer First
    m_topRightConnectTypeComboBox->move(5, 35);
    m_topRightConnectTypeComboBox->raise();
    m_topRightConnectTypeComboBox->setMinimumWidth(130);

    // Connect to signal
    connect(m_topRightConnectTypeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, [this](int index) {
            m_topRightConnectType = static_cast<PatternConnectType>(m_topRightConnectTypeComboBox->itemData(index).toInt());
            // Subject视图使用IFillType*接口
            m_topRightView->executeSubjectPattern(getSubjectFillType(), getTopRightConnectType());
            // 同时更新布尔运算视图
            m_bottomRightView->setPolylines(m_mainView->polylines());
            m_bottomRightView->setPolygons(m_mainView->polygons());
            m_bottomRightView->setSelectedPolygon(m_mainView->selectedPolygonIndex());
            m_bottomRightView->setTool(m_mainView->tool());
            m_bottomRightView->setClipPolygons(m_mainView->clipPolygons());
            m_bottomRightView->setClipPolylines(m_mainView->clipPolylines());
            m_bottomRightView->setSubjectPolygons(m_mainView->subjectPolygons());
            m_bottomRightView->setSubjectPolylines(m_mainView->subjectPolylines());
            m_bottomRightView->executeBooleanOperation(
                static_cast<tailor_visualization::BooleanOperation>(m_currentBooleanOperation),
                getClipFillType(),
                getSubjectFillType(),
                getBottomRightConnectType()
            );
            m_bottomRightView->update();
        });

    // Setup bottom-left connect type combo box (for Clip)
    m_bottomLeftConnectTypeComboBox = new QComboBox(m_frameBottomLeft);
    m_bottomLeftConnectTypeComboBox->addItem("Outer First", static_cast<int>(PatternConnectType::OuterFirst));
    m_bottomLeftConnectTypeComboBox->addItem("Inner First", static_cast<int>(PatternConnectType::InnerFirst));
    m_bottomLeftConnectTypeComboBox->setCurrentIndex(0); // Default: Outer First
    m_bottomLeftConnectTypeComboBox->move(5, 35);
    m_bottomLeftConnectTypeComboBox->raise();
    m_bottomLeftConnectTypeComboBox->setMinimumWidth(130);

    // Connect to signal
    connect(m_bottomLeftConnectTypeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, [this](int index) {
            m_bottomLeftConnectType = static_cast<PatternConnectType>(m_bottomLeftConnectTypeComboBox->itemData(index).toInt());
            // Clip视图使用IFillType*接口
            m_bottomLeftView->executeClipPattern(getClipFillType(), getBottomLeftConnectType());
            // 同时更新布尔运算视图
            m_bottomRightView->setPolylines(m_mainView->polylines());
            m_bottomRightView->setPolygons(m_mainView->polygons());
            m_bottomRightView->setSelectedPolygon(m_mainView->selectedPolygonIndex());
            m_bottomRightView->setTool(m_mainView->tool());
            m_bottomRightView->setClipPolygons(m_mainView->clipPolygons());
            m_bottomRightView->setClipPolylines(m_mainView->clipPolylines());
            m_bottomRightView->setSubjectPolygons(m_mainView->subjectPolygons());
            m_bottomRightView->setSubjectPolylines(m_mainView->subjectPolylines());
            m_bottomRightView->executeBooleanOperation(
                static_cast<tailor_visualization::BooleanOperation>(m_currentBooleanOperation),
                getClipFillType(),
                getSubjectFillType(),
                getBottomRightConnectType()
            );
            m_bottomRightView->update();
        });

    // Setup bottom-right connect type combo box (for Boolean operations)
    m_bottomRightConnectTypeComboBox = new QComboBox(m_frameBottomRight);
    m_bottomRightConnectTypeComboBox->addItem("Outer First", static_cast<int>(PatternConnectType::OuterFirst));
    m_bottomRightConnectTypeComboBox->addItem("Inner First", static_cast<int>(PatternConnectType::InnerFirst));
    m_bottomRightConnectTypeComboBox->setCurrentIndex(0); // Default: Outer First
    m_bottomRightConnectTypeComboBox->move(5, 35);
    m_bottomRightConnectTypeComboBox->raise();
    m_bottomRightConnectTypeComboBox->setMinimumWidth(130);

    // Connect to signal
    connect(m_bottomRightConnectTypeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, [this](int index) {
            m_bottomRightConnectType = static_cast<PatternConnectType>(m_bottomRightConnectTypeComboBox->itemData(index).toInt());
            // 更新布尔运算视图
            m_bottomRightView->setPolylines(m_mainView->polylines());
            m_bottomRightView->setPolygons(m_mainView->polygons());
            m_bottomRightView->setSelectedPolygon(m_mainView->selectedPolygonIndex());
            m_bottomRightView->setTool(m_mainView->tool());
            m_bottomRightView->setClipPolygons(m_mainView->clipPolygons());
            m_bottomRightView->setClipPolylines(m_mainView->clipPolylines());
            m_bottomRightView->setSubjectPolygons(m_mainView->subjectPolygons());
            m_bottomRightView->setSubjectPolylines(m_mainView->subjectPolylines());
            m_bottomRightView->executeBooleanOperation(
                static_cast<tailor_visualization::BooleanOperation>(m_currentBooleanOperation),
                getClipFillType(),
                getSubjectFillType(),
                getBottomRightConnectType()
            );
            m_bottomRightView->update();
        });
}

void FourViewContainer::synchronizeViews() {
    // Copy all data from main view to secondary views
    m_topRightView->setPolylines(m_mainView->polylines());
    m_topRightView->setPolygons(m_mainView->polygons());
    m_topRightView->setSelectedPolygon(m_mainView->selectedPolygonIndex());
    m_topRightView->setTool(m_mainView->tool());
    m_topRightView->setClipPolygons(m_mainView->clipPolygons());
    m_topRightView->setClipPolylines(m_mainView->clipPolylines());
    m_topRightView->setSubjectPolygons(m_mainView->subjectPolygons());
    m_topRightView->setSubjectPolylines(m_mainView->subjectPolylines());
    m_topRightView->setViewMode(Sketch2DView::ViewMode::SubjectFocus); // 右上侧重 Subject
    // 执行 Subject Pattern 计算（使用当前选择的 FillType 和 ConnectType）
    m_topRightView->executeSubjectPattern(getSubjectFillType(), getTopRightConnectType());

    m_bottomLeftView->setPolylines(m_mainView->polylines());
    m_bottomLeftView->setPolygons(m_mainView->polygons());
    m_bottomLeftView->setSelectedPolygon(m_mainView->selectedPolygonIndex());
    m_bottomLeftView->setTool(m_mainView->tool());
    m_bottomLeftView->setClipPolygons(m_mainView->clipPolygons());
    m_bottomLeftView->setClipPolylines(m_mainView->clipPolylines());
    m_bottomLeftView->setSubjectPolygons(m_mainView->subjectPolygons());
    m_bottomLeftView->setSubjectPolylines(m_mainView->subjectPolylines());
    m_bottomLeftView->setViewMode(Sketch2DView::ViewMode::ClipFocus); // 左下侧重 Clip
    // 执行 Clip Pattern 计算（使用当前选择的 FillType 和 ConnectType）
    m_bottomLeftView->executeClipPattern(getClipFillType(), getBottomLeftConnectType());

    m_bottomRightView->setPolylines(m_mainView->polylines());
    m_bottomRightView->setPolygons(m_mainView->polygons());
    m_bottomRightView->setSelectedPolygon(m_mainView->selectedPolygonIndex());
    m_bottomRightView->setTool(m_mainView->tool());
    m_bottomRightView->setClipPolygons(m_mainView->clipPolygons());
    m_bottomRightView->setClipPolylines(m_mainView->clipPolylines());
    m_bottomRightView->setSubjectPolygons(m_mainView->subjectPolygons());
    m_bottomRightView->setSubjectPolylines(m_mainView->subjectPolylines());
    m_bottomRightView->setViewMode(Sketch2DView::ViewMode::BooleanResult); // 右下显示布尔运算结果

    // 调试：在用户改动输入后，导出输入多边形（三个视图使用相同的输入，只导出一次）
    // QSet<int> allInputPolygons = m_mainView->clipPolygons() + m_mainView->subjectPolygons();
    // m_mainView->debugExportPolygons("debug_input_polygons.txt", allInputPolygons, "Input");

    // 实时执行布尔运算计算（使用Clip和Subject视图分别选择的 FillType 和 ConnectType）
    m_bottomRightView->executeBooleanOperation(
        static_cast<tailor_visualization::BooleanOperation>(m_currentBooleanOperation),
        getClipFillType(),
        getSubjectFillType(),
        getBottomRightConnectType()
    );

    // Update secondary views
    m_topRightView->update();
    m_bottomLeftView->update();
    m_bottomRightView->update();
}

const tailor_visualization::IFillType* FourViewContainer::getClipFillType() const {
    switch (m_bottomLeftFillType) {
    case PatternFillType::NonZero:
        return m_nonZeroFillType.get();
    case PatternFillType::EvenOdd:
        return m_evenOddFillType.get();
    case PatternFillType::Ignore:
        return m_ignoreFillType.get();
    case PatternFillType::SpecificWinding:
        return m_specificWindingFillTypeBottomLeft.get();
    default:
        return m_nonZeroFillType.get();
    }
}

const tailor_visualization::IFillType* FourViewContainer::getSubjectFillType() const {
    switch (m_topRightFillType) {
    case PatternFillType::NonZero:
        return m_nonZeroFillType.get();
    case PatternFillType::EvenOdd:
        return m_evenOddFillType.get();
    case PatternFillType::Ignore:
        return m_ignoreFillType.get();
    case PatternFillType::SpecificWinding:
        return m_specificWindingFillTypeTopRight.get();
    default:
        return m_nonZeroFillType.get();
    }
}

const tailor_visualization::IConnectType<ConnectTypeDrafting>* FourViewContainer::getTopRightConnectType() const {
    switch (m_topRightConnectType) {
    case PatternConnectType::OuterFirst:
        return m_connectTypeOuterFirst.get();
    case PatternConnectType::InnerFirst:
        return m_connectTypeInnerFirst.get();
    default:
        return m_connectTypeOuterFirst.get();
    }
}

const tailor_visualization::IConnectType<ConnectTypeDrafting>* FourViewContainer::getBottomLeftConnectType() const {
    switch (m_bottomLeftConnectType) {
    case PatternConnectType::OuterFirst:
        return m_connectTypeOuterFirst.get();
    case PatternConnectType::InnerFirst:
        return m_connectTypeInnerFirst.get();
    default:
        return m_connectTypeOuterFirst.get();
    }
}

const tailor_visualization::IConnectType<ConnectTypeDrafting>* FourViewContainer::getBottomRightConnectType() const {
    switch (m_bottomRightConnectType) {
    case PatternConnectType::OuterFirst:
        return m_connectTypeOuterFirst.get();
    case PatternConnectType::InnerFirst:
        return m_connectTypeInnerFirst.get();
    default:
        return m_connectTypeOuterFirst.get();
    }
}







