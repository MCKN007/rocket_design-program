#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QScatterSeries>
#include <QtCharts/QValueAxis>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QFileDialog>
#include <QRegularExpression>
#include <QDebug>
#include <QTimer>
#include <cmath>
#include <algorithm>

// 常数定义
const double G0 = 9.80665;           // 重力加速度 (m/s²)
const double PI = 3.141592653589793;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , ceaEngine(new CEAEngine(this))
    , chart(nullptr)
    , ispSeries(nullptr)
    , optimalPoint(nullptr)
    , chartView(nullptr)
    , bestOF(0.0)
    , bestIsp(0.0)
    , isCalculating(false)
{
    ui->setupUi(this);

    // ===== 初始化UI控件 =====

    // 氧化剂选择框
    ui->comboBox_oxidizer->clear();
    ui->comboBox_oxidizer->addItem("液氧 (LOX)", "LOX");
    ui->comboBox_oxidizer->addItem("气氧 (O2)", "O2");
    ui->comboBox_oxidizer->addItem("四氧化二氮 (NTO)", "NTO");
    ui->comboBox_oxidizer->addItem("过氧化氢 (H2O2)", "H2O2");

    // 燃料选择框
    ui->comboBox_fuel->clear();
    ui->comboBox_fuel->addItem("煤油 (RP-1)", "RP-1");
    ui->comboBox_fuel->addItem("液氢 (LH2)", "LH2");
    ui->comboBox_fuel->addItem("气氢 (H2)", "H2");
    ui->comboBox_fuel->addItem("偏二甲肼 (UDMH)", "UDMH");
    ui->comboBox_fuel->addItem("乙醇 (C2H5OH)", "C2H5OH");
    ui->comboBox_fuel->addItem("甲烷 (CH4)", "CH4");
    ui->comboBox_fuel->addItem("一甲基肼 (MMH)", "MMH");

    // ===== 设置默认值 =====
    ui->lineEdit_thrust->setText("10000");      // 推力 10000 N
    ui->lineEdit_pressure->setText("50");       // 燃烧室压力 50 bar
    ui->lineEdit_of_ratio->setText("2.5");      // 混合比 2.5
    ui->lineEdit_expansion->setText("40");      // 扩张比 40

    // ===== 连接信号槽 =====

    // 按钮连接
    connect(ui->pushButton_calculate, &QPushButton::clicked,
            this, &MainWindow::onCalculateButtonClicked);
    connect(ui->pushButton_scan, &QPushButton::clicked,
            this, &MainWindow::onScanOFButtonClicked);
    connect(ui->pushButton_export, &QPushButton::clicked,
            this, &MainWindow::onExportButtonClicked);

    // CEA引擎信号连接
    connect(ceaEngine, &CEAEngine::calculationStarted,
            this, &MainWindow::onCEACalculationStarted);
    connect(ceaEngine, &CEAEngine::calculationFinished,
            this, &MainWindow::onCEACalculationFinished);
    connect(ceaEngine, &CEAEngine::batchProgress,
            this, &MainWindow::onCEABatchProgress);

    // 输入框变化连接
    connect(ui->lineEdit_thrust, &QLineEdit::textChanged,
            this, &MainWindow::onInputParameterChanged);
    connect(ui->lineEdit_pressure, &QLineEdit::textChanged,
            this, &MainWindow::onInputParameterChanged);
    connect(ui->lineEdit_of_ratio, &QLineEdit::textChanged,
            this, &MainWindow::onInputParameterChanged);
    connect(ui->lineEdit_expansion, &QLineEdit::textChanged,
            this, &MainWindow::onInputParameterChanged);

    // 下拉框变化连接
    connect(ui->comboBox_oxidizer, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onOxidizerChanged);
    connect(ui->comboBox_fuel, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onFuelChanged);

    // ===== 初始化图表 =====
    initializeChart();

    // 检查CEA状态
    if (!ceaEngine->isCEAAvailable()) {
        updateStatus("警告: CEA软件未找到，请确保cea.exe在正确位置", 5000);
    } else {
        updateStatus("就绪 - CEA软件可用", 3000);
    }

    qDebug() << "MainWindow初始化完成";
}

MainWindow::~MainWindow()
{
    delete ui;
    delete ceaEngine;
}

// ===== 图表初始化函数 =====
void MainWindow::initializeChart()
{
    if (!ui->widget_chart) {
        qWarning() << "widget_chart 指针为空!";
        return;
    }

    // 清除widget_chart上可能已有的内容
    QLayout* existingLayout = ui->widget_chart->layout();
    if (existingLayout) {
        QLayoutItem* item;
        while ((item = existingLayout->takeAt(0)) != nullptr) {
            if (item->widget()) {
                item->widget()->setParent(nullptr);
            }
            delete item;
        }
        delete existingLayout;
    }

    // 创建图表对象
    chart = new QChart();
    chart->setTitle("比冲 vs 混合比 (Isp vs O/F)");
    chart->setAnimationOptions(QChart::SeriesAnimations);
    chart->setTheme(QChart::ChartThemeLight);

    // 创建曲线系列
    ispSeries = new QLineSeries();
    ispSeries->setName("比冲曲线");
    ispSeries->setColor(QColor(65, 105, 225));
    ispSeries->setPointsVisible(true);
    ispSeries->setPen(QPen(QBrush(QColor(65, 105, 225)), 2));

    // 创建最佳点系列
    optimalPoint = new QScatterSeries();
    optimalPoint->setName("最佳点");
    optimalPoint->setMarkerSize(15.0);
    optimalPoint->setColor(Qt::red);
    optimalPoint->setBorderColor(Qt::darkRed);
    optimalPoint->setMarkerShape(QScatterSeries::MarkerShapeCircle);

    // 添加到图表
    chart->addSeries(ispSeries);
    chart->addSeries(optimalPoint);

    // 创建坐标轴
    QValueAxis *axisX = new QValueAxis();
    axisX->setTitleText("混合比 (O/F)");
    axisX->setTitleBrush(QBrush(Qt::black));
    axisX->setLabelFormat("%.2f");
    axisX->setRange(1.0, 8.0);
    axisX->setTickCount(8);
    axisX->setGridLineVisible(true);
    axisX->setMinorTickCount(1);

    QValueAxis *axisY = new QValueAxis();
    axisY->setTitleText("比冲 Isp (秒)");
    axisY->setTitleBrush(QBrush(Qt::black));
    axisY->setLabelFormat("%.0f");
    axisY->setRange(200, 500);
    axisY->setTickCount(7);
    axisY->setGridLineVisible(true);
    axisY->setMinorTickCount(1);

    // 连接系列和坐标轴
    chart->addAxis(axisX, Qt::AlignBottom);
    chart->addAxis(axisY, Qt::AlignLeft);

    ispSeries->attachAxis(axisX);
    ispSeries->attachAxis(axisY);
    optimalPoint->attachAxis(axisX);
    optimalPoint->attachAxis(axisY);

    // 创建图表视图
    chartView = new QChartView(chart);
    chartView->setRenderHint(QPainter::Antialiasing);
    chartView->setBackgroundBrush(QBrush(QColor(245, 245, 245)));
    chartView->setRubberBand(QChartView::RectangleRubberBand);
    chartView->setInteractive(true);

    // 添加到布局
    QVBoxLayout *layout = new QVBoxLayout(ui->widget_chart);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(chartView);
    ui->widget_chart->setLayout(layout);

    // 确保widget_chart可见
    ui->widget_chart->setVisible(true);
    chartView->setVisible(true);

    // 添加假数据用于测试图表显示
    addTestDataToChart();

    qDebug() << "图表初始化完成";
}

// ===== 添加测试数据函数 =====
void MainWindow::addTestDataToChart()
{
    if (!chart || !ispSeries) {
        qWarning() << "图表未初始化，无法添加测试数据";
        return;
    }

    // 清空现有数据
    ispSeries->clear();
    if (optimalPoint) {
        optimalPoint->clear();
    }

    // 生成假数据用于测试显示
    // 这是一个典型的LOX/RP-1比冲曲线
    QVector<QPointF> testData;
    testData << QPointF(1.5, 280.0)
             << QPointF(2.0, 310.0)
             << QPointF(2.5, 330.0)
             << QPointF(3.0, 340.0)
             << QPointF(3.5, 335.0)
             << QPointF(4.0, 325.0)
             << QPointF(4.5, 315.0)
             << QPointF(5.0, 305.0);

    // 添加测试数据
    for (const QPointF& point : testData) {
        ispSeries->append(point);
    }

    // 添加一个测试的最佳点
    if (optimalPoint) {
        optimalPoint->append(3.0, 340.0);
        bestOF = 3.0;
        bestIsp = 340.0;
    }

    // 更新图表
    chart->setTitle("测试图表 - LOX/RP-1 比冲曲线");

    // 确保图表视图更新
    if (chartView) {
        chartView->update();
        chartView->repaint();
    }

    qDebug() << "测试图表数据已添加，应有" << testData.size() << "个数据点";
    qDebug() << "测试最佳点: O/F =" << bestOF << ", Isp =" << bestIsp;
}

// ===== 槽函数实现 =====

void MainWindow::onCalculateButtonClicked()
{
    if (isCalculating) {
        QMessageBox::information(this, "计算中", "当前正在计算，请稍候...");
        return;
    }

    if (!validateInputParameters()) {
        QMessageBox::warning(this, "输入错误", "请检查输入参数是否有效");
        return;
    }

    // 清空之前的计算结果
    clearAllResults();

    // 运行CEA计算
    runCEACalculation();
}

void MainWindow::onScanOFButtonClicked()
{
    if (isCalculating) {
        QMessageBox::information(this, "计算中", "当前正在计算，请稍候...");
        return;
    }

    if (!validateInputParameters()) {
        QMessageBox::warning(this, "输入错误", "请检查输入参数是否有效");
        return;
    }

    // 执行O/F扫描
    performOFScan();
}

void MainWindow::onExportButtonClicked()
{
    if (ofValues.isEmpty()) {
        QMessageBox::warning(this, "警告", "没有可导出的数据，请先进行计算或扫描");
        return;
    }

    QString defaultFileName = "火箭发动机设计结果_" +
                              QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ".csv";

    QString fileName = QFileDialog::getSaveFileName(
        this, "导出结果", defaultFileName,
        "CSV文件 (*.csv);;文本文件 (*.txt);;Excel文件 (*.xlsx)"
        );

    if (!fileName.isEmpty()) {
        exportResultsToFile(fileName);
    }
}

void MainWindow::onCheckCEAButtonClicked()
{
    if (ceaEngine->isCEAAvailable()) {
        QMessageBox::information(this, "CEA状态",
                                 QString("CEA软件可用\n路径: %1").arg(ceaEngine->getCEAPath()));
    } else {
        QMessageBox::warning(this, "CEA状态",
                             "CEA软件未找到\n请确保cea.exe位于:\n"
                             "1. 应用程序目录下的cea/子目录\n"
                             "2. 与应用程序相同的目录\n"
                             "3. 或手动设置CEA路径");
    }
}

void MainWindow::onInputParameterChanged()
{
    // 实时验证输入参数
    validateInputParameters();
}

void MainWindow::onOxidizerChanged()
{
    // 根据选择的氧化剂调整推荐参数
    QString oxid = ui->comboBox_oxidizer->currentData().toString();
    QString fuel = ui->comboBox_fuel->currentData().toString();

    // 设置典型O/F值
    if (oxid == "LOX" && fuel == "RP-1") {
        ui->lineEdit_of_ratio->setText("2.5");
    } else if (oxid == "LOX" && fuel == "LH2") {
        ui->lineEdit_of_ratio->setText("6.0");
    } else if (oxid == "NTO" && fuel == "UDMH") {
        ui->lineEdit_of_ratio->setText("2.0");
    } else if (oxid == "O2" && fuel == "C2H5OH") {
        ui->lineEdit_of_ratio->setText("1.4");
    } else if (oxid == "H2O2" && fuel == "RP-1") {
        ui->lineEdit_of_ratio->setText("6.5");
    } else if (oxid == "LOX" && fuel == "CH4") {
        ui->lineEdit_of_ratio->setText("3.0");
    }

    // 在状态栏显示建议范围
    QPair<double, double> range = getRecommendedOFRange(oxid, fuel);
    QString tip = QString("建议O/F范围: %1 - %2")
                      .arg(range.first, 0, 'f', 1)
                      .arg(range.second, 0, 'f', 1);
    updateStatus(tip, 3000);

    onInputParameterChanged();
}

void MainWindow::onFuelChanged()
{
    // 与onOxidizerChanged类似
    onOxidizerChanged();
}

void MainWindow::onCEACalculationStarted()
{
    isCalculating = true;
    ui->pushButton_calculate->setEnabled(false);
    ui->pushButton_scan->setEnabled(false);
    updateStatus("CEA计算中...");
    qDebug() << "CEA计算开始";
}

void MainWindow::onCEACalculationFinished(bool success, const QString& error)
{
    isCalculating = false;
    ui->pushButton_calculate->setEnabled(true);
    ui->pushButton_scan->setEnabled(true);

    if (success) {
        updateStatus("CEA计算完成", 3000);
        ceaResult = ceaEngine->getResult();

        // 计算发动机几何参数
        calculateEngineGeometry();

        // 计算喷注器参数
        calculateInjectorParameters();

        // 计算冷却系统
        calculateCoolingSystem();

        // 更新UI显示
        updateUIWithResults();

        qDebug() << "CEA计算成功完成";
    } else {
        updateStatus("CEA计算失败", 3000);
        QMessageBox::critical(this, "CEA计算失败",
                              QString("CEA计算失败:\n%1\n\n"
                                      "可能的原因:\n"
                                      "1. CEA软件路径不正确\n"
                                      "2. 输入参数超出范围\n"
                                      "3. CEA软件内部错误").arg(error));
        qWarning() << "CEA计算失败:" << error;
    }
}

void MainWindow::onCEABatchProgress(int current, int total)
{
    updateStatus(QString("O/F扫描进度: %1/%2").arg(current).arg(total));
}

// ===== 核心计算函数 =====

void MainWindow::runCEACalculation()
{
    // 获取输入参数
    double thrust = ui->lineEdit_thrust->text().toDouble();
    double pressure = ui->lineEdit_pressure->text().toDouble();
    double ofRatio = ui->lineEdit_of_ratio->text().toDouble();
    double expansion = ui->lineEdit_expansion->text().toDouble();
    QString oxid = ui->comboBox_oxidizer->currentData().toString();
    QString fuel = ui->comboBox_fuel->currentData().toString();

    qDebug() << "开始CEA计算:"
             << "氧化剂:" << oxid
             << "燃料:" << fuel
             << "压力:" << pressure << "bar"
             << "O/F:" << ofRatio
             << "扩张比:" << expansion;

    // 运行CEA计算（异步）
    ceaEngine->runCEA(oxid, fuel, pressure, ofRatio, expansion);
}

void MainWindow::calculateEngineGeometry()
{
    // 基于推力、比冲和压力计算发动机几何参数

    // 1. 计算总质量流量
    double thrust = ui->lineEdit_thrust->text().toDouble();
    double mdot_total = thrust / (ceaResult.Isp_vac * G0);  // kg/s

    // 2. 计算喉部面积 (使用特征速度公式)
    double Pc = barToPa(ui->lineEdit_pressure->text().toDouble());
    double At = (mdot_total * ceaResult.cStar) / Pc;  // m²

    // 3. 计算喉部直径
    double Dt = sqrt(4 * At / PI);  // m
    geometry.throatDiameter = mToMm(Dt);

    // 4. 计算出口面积和直径 (使用扩张比)
    double expansion = ui->lineEdit_expansion->text().toDouble();
    double Ae = At * expansion;     // m²
    double De = sqrt(4 * Ae / PI);  // m
    geometry.exitDiameter = mToMm(De);

    // 5. 计算燃烧室尺寸 (经验公式)
    // 燃烧室直径通常是喉部直径的2.5-4倍
    double Dc = Dt * 3.5;  // 取中间值
    geometry.chamberDiameter = mToMm(Dc);

    // 6. 计算燃烧室长度 (L* = Vc/At，典型L* = 0.8-1.2m)
    double Lstar = 1.0;  // 典型特征长度 1.0m
    double Vc = Lstar * At;
    double Lc = Vc / (PI * Dc * Dc / 4);
    geometry.chamberLength = mToMm(Lc);

    // 7. 计算特征长度 (实际值)
    // 对于火箭发动机，特征长度L*通常在0.8-1.5m之间
    // 我们可以基于推进剂组合和燃烧室压力给出一个合理值
    QString oxid = ui->comboBox_oxidizer->currentData().toString();
    QString fuel = ui->comboBox_fuel->currentData().toString();

    // 根据不同推进剂组合设置特征长度
    if ((oxid == "LOX" || oxid == "O2") && (fuel == "RP-1" || fuel == "C2H5OH" || fuel == "CH4")) {
        Lstar = 1.0;  // 液氧/烃类燃料
    } else if ((oxid == "LOX" || oxid == "O2") && fuel == "LH2") {
        Lstar = 1.2;  // 液氧/液氢
    } else if (oxid == "NTO" && (fuel == "UDMH" || fuel == "MMH")) {
        Lstar = 0.9;  // NTO/肼类燃料
    } else {
        Lstar = 1.0;  // 默认值
    }

    // 8. 计算收敛段长度 (经验公式)
    // 收敛段长度通常为喉部直径的0.5-1.5倍
    double L_conv = Dt * 1.0;  // 取中间值

    // 9. 基于新的Lstar重新计算燃烧室体积和长度
    Vc = Lstar * At;
    Lc = Vc / (PI * Dc * Dc / 4);
    geometry.chamberLength = mToMm(Lc);

    // 10. 计算扩张半角 (经验值 12-18度)
    geometry.halfAngle = 15.0;
    geometry.expansionRatio = expansion;

    // 11. 更新UI显示
    ui->lineEdit_chamber_dia->setText(QString::number(geometry.chamberDiameter, 'f', 2));
    ui->lineEdit_chamber_len->setText(QString::number(geometry.chamberLength, 'f', 2));
    ui->lineEdit_throat_dia->setText(QString::number(geometry.throatDiameter, 'f', 2));
    ui->lineEdit_exit_dia->setText(QString::number(geometry.exitDiameter, 'f', 2));
    ui->lineEdit_expansion_ratio->setText(QString::number(geometry.expansionRatio, 'f', 1));
    ui->lineEdit_half_angle->setText(QString::number(geometry.halfAngle, 'f', 1));

    // 12. 显示特征长度和收敛段长度
    ui->lineEdit_tezheng->setText(QString::number(Lstar, 'f', 3));  // 特征长度 (m)
    ui->lineEdit_shoulian_long->setText(QString::number(mToMm(L_conv), 'f', 2));  // 收敛段长度 (mm)

    qDebug() << "几何参数计算完成:"
             << "喉径:" << geometry.throatDiameter << "mm"
             << "出口直径:" << geometry.exitDiameter << "mm"
             << "燃烧室直径:" << geometry.chamberDiameter << "mm"
             << "燃烧室长度:" << geometry.chamberLength << "mm"
             << "特征长度L*:" << Lstar << "m"
             << "收敛段长度:" << mToMm(L_conv) << "mm";
}

void MainWindow::calculateInjectorParameters()
{
    // 计算喷注器参数

    double thrust = ui->lineEdit_thrust->text().toDouble();
    double ofRatio = ui->lineEdit_of_ratio->text().toDouble();
    double Pc = ui->lineEdit_pressure->text().toDouble();

    // 1. 计算总质量流量和分流量
    double mdot_total = thrust / (ceaResult.Isp_vac * G0);  // kg/s
    double mdot_ox = mdot_total * ofRatio / (1 + ofRatio);  // 氧化剂流量
    double mdot_fuel = mdot_total / (1 + ofRatio);          // 燃料流量

    // 2. 获取推进剂属性
    QString oxid = ui->comboBox_oxidizer->currentData().toString();
    QString fuel = ui->comboBox_fuel->currentData().toString();
    double rho_ox = CEAEngine::getDensity(oxid);     // kg/m³
    double rho_fuel = CEAEngine::getDensity(fuel);   // kg/m³

    // 3. 计算喷射速度 (典型值 20-40 m/s)
    double V_ox = 30.0;     // m/s
    double V_fuel = 25.0;   // m/s

    // 4. 计算压降 (ΔP = 0.5 * ρ * V²)
    injector.oxPressureDrop = 0.5 * rho_ox * V_ox * V_ox / 1e5;  // bar
    injector.fuelPressureDrop = 0.5 * rho_fuel * V_fuel * V_fuel / 1e5;  // bar

    // 5. 计算单孔流量和孔数
    // 假设目标孔数
    int target_holes = 100;
    double mdot_per_hole_ox = mdot_ox / target_holes;
    double mdot_per_hole_fuel = mdot_fuel / target_holes;

    // 6. 计算孔直径 (使用流量公式: mdot = Cd * A * sqrt(2ρΔP))
    double Cd = 0.7;  // 流量系数
    double A_ox = mdot_per_hole_ox / (Cd * sqrt(2 * rho_ox * injector.oxPressureDrop * 1e5));
    double A_fuel = mdot_per_hole_fuel / (Cd * sqrt(2 * rho_fuel * injector.fuelPressureDrop * 1e5));

    injector.oxOrificeDia = sqrt(4 * A_ox / PI) * 1000;  // mm
    injector.fuelOrificeDia = sqrt(4 * A_fuel / PI) * 1000;  // mm

    // 确保孔直径在合理范围内 (0.5-3.0 mm)
    if (injector.oxOrificeDia < 0.5) injector.oxOrificeDia = 0.5;
    if (injector.oxOrificeDia > 3.0) injector.oxOrificeDia = 3.0;
    if (injector.fuelOrificeDia < 0.5) injector.fuelOrificeDia = 0.5;
    if (injector.fuelOrificeDia > 3.0) injector.fuelOrificeDia = 3.0;

    // 7. 根据调整后的孔直径重新计算孔数
    injector.oxOrificeCount = static_cast<int>(ceil(mdot_ox / (Cd * PI/4 * pow(injector.oxOrificeDia/1000, 2) * sqrt(2 * rho_ox * injector.oxPressureDrop * 1e5))));
    injector.fuelOrificeCount = static_cast<int>(ceil(mdot_fuel / (Cd * PI/4 * pow(injector.fuelOrificeDia/1000, 2) * sqrt(2 * rho_fuel * injector.fuelPressureDrop * 1e5))));

    // 8. 计算雾化SMD (索特尔平均直径)
    double sigma_ox = CEAEngine::getSurfaceTension(oxid);   // 表面张力 N/m
    double sigma_fuel = CEAEngine::getSurfaceTension(fuel); // 表面张力 N/m

    double We_ox = rho_ox * V_ox * V_ox * injector.oxOrificeDia/1000 / sigma_ox;

    injector.smd = 3.08 * injector.oxOrificeDia / sqrt(We_ox) * 1000;  // μm

    qDebug() << "喷注器参数计算完成:"
             << "氧化剂孔直径:" << injector.oxOrificeDia << "mm"
             << "燃料孔直径:" << injector.fuelOrificeDia << "mm"
             << "氧化剂孔数:" << injector.oxOrificeCount
             << "燃料孔数:" << injector.fuelOrificeCount
             << "雾化SMD:" << injector.smd << "μm";
}

void MainWindow::calculateCoolingSystem()
{
    // 简化冷却系统计算

    // 1. 基于燃烧室尺寸计算冷却肋参数
    double chamberDia = geometry.chamberDiameter;  // mm
    double chamberLen = geometry.chamberLength;    // mm

    // 2. 经验公式: 肋高约为燃烧室直径的1/20
    cooling.finHeight = chamberDia / 20.0;
    if (cooling.finHeight < 5.0) cooling.finHeight = 5.0;
    if (cooling.finHeight > 20.0) cooling.finHeight = 20.0;

    // 3. 肋宽约为肋高的1/3
    cooling.finWidth = cooling.finHeight / 3.0;
    if (cooling.finWidth < 1.5) cooling.finWidth = 1.5;
    if (cooling.finWidth > 6.0) cooling.finWidth = 6.0;

    // 4. 肋间距约为肋高的2倍
    cooling.finSpacing = cooling.finHeight * 2.0;

    // 5. 计算肋片数量
    double circumference = PI * chamberDia;
    cooling.finCount = static_cast<int>(circumference / (cooling.finWidth + cooling.finSpacing));

    // 确保肋片数量合理
    if (cooling.finCount < 20) cooling.finCount = 20;
    if (cooling.finCount > 200) cooling.finCount = 200;

    qDebug() << "冷却系统计算完成:"
             << "肋高:" << cooling.finHeight << "mm"
             << "肋宽:" << cooling.finWidth << "mm"
             << "肋间距:" << cooling.finSpacing << "mm"
             << "肋片数量:" << cooling.finCount;
}

// ===== O/F扫描函数 =====

void MainWindow::performOFScan()
{
    // 清空之前的数据
    ofValues.clear();
    ispValues.clear();
    if (ispSeries) {
        ispSeries->clear();
    }
    if (optimalPoint) {
        optimalPoint->clear();
    }

    // 获取扫描范围
    QString oxid = ui->comboBox_oxidizer->currentData().toString();
    QString fuel = ui->comboBox_fuel->currentData().toString();

    QPair<double, double> rangePair = getRecommendedOFRange(oxid, fuel);
    double minOF = rangePair.first;
    double maxOF = rangePair.second;
    double step = (maxOF - minOF) / 30.0; // 30个数据点（避免太多）

    double pressure = ui->lineEdit_pressure->text().toDouble();
    double expansion = ui->lineEdit_expansion->text().toDouble();

    qDebug() << "开始O/F扫描: 范围" << minOF << "到" << maxOF << "步长" << step;

    // 准备O/F值列表
    QVector<double> ofList;
    for (double of = minOF; of <= maxOF; of += step) {
        ofList.append(of);
    }

    // 显示进度
    updateStatus("开始O/F扫描...");

    // 执行批量计算
    QVector<CEAResult> results = ceaEngine->batchCalculate(oxid, fuel, pressure, ofList, expansion);

    // 处理结果
    for (int i = 0; i < results.size(); i++) {
        const CEAResult& result = results[i];
        if (result.isValid()) {
            double of = ofList[i];
            ofValues.append(of);
            ispValues.append(result.Isp_vac);

            qDebug() << "扫描点: O/F =" << of << ", Isp =" << result.Isp_vac;
        }
    }

    if (ofValues.isEmpty()) {
        QMessageBox::warning(this, "警告", "O/F扫描未获得有效数据");
        // 如果没有真实数据，显示测试数据
        addTestDataToChart();
        return;
    }

    updateStatus("O/F扫描完成，正在绘制曲线...", 3000);

    // 找到最佳O/F
    findOptimalOF();

    // 绘制曲线
    plotOFScanCurve();

    // 更新最佳O/F到输入框
    if (bestOF > 0) {
        ui->lineEdit_of_ratio->setText(QString::number(bestOF, 'f', 2));
        updateStatus(QString("找到最佳O/F: %1, 对应比冲: %2秒").arg(bestOF, 0, 'f', 2).arg(bestIsp, 0, 'f', 1), 5000);
    }
}

QPair<double, double> MainWindow::getRecommendedOFRange(const QString& oxid, const QString& fuel)
{
    if (oxid == "LOX" && fuel == "RP-1") {
        return qMakePair(1.8, 3.2);
    } else if (oxid == "LOX" && fuel == "LH2") {
        return qMakePair(4.0, 8.0);
    } else if (oxid == "LOX" && fuel == "H2") {
        return qMakePair(4.0, 8.0);
    } else if (oxid == "NTO" && fuel == "UDMH") {
        return qMakePair(1.6, 3.0);
    } else if (oxid == "O2" && fuel == "C2H5OH") {
        return qMakePair(0.9, 2.0);
    } else if (oxid == "H2O2" && fuel == "RP-1") {
        return qMakePair(5.0, 8.0);
    } else if (oxid == "LOX" && fuel == "CH4") {
        return qMakePair(2.5, 4.0);
    } else if (oxid == "NTO" && fuel == "MMH") {
        return qMakePair(1.8, 2.4);
    }

    // 默认范围
    return qMakePair(1.0, 5.0);
}

void MainWindow::findOptimalOF()
{
    if (ofValues.isEmpty() || ispValues.isEmpty()) {
        bestOF = 0;
        bestIsp = 0;
        return;
    }

    // 寻找最大比冲对应的O/F
    double maxIsp = ispValues[0];
    int maxIndex = 0;

    for (int i = 1; i < ispValues.size(); ++i) {
        if (ispValues[i] > maxIsp) {
            maxIsp = ispValues[i];
            maxIndex = i;
        }
    }

    bestOF = ofValues[maxIndex];
    bestIsp = maxIsp;

    // 更新最佳点显示
    if (optimalPoint) {
        optimalPoint->clear();
        optimalPoint->append(bestOF, bestIsp);
    }

    qDebug() << "最佳O/F:" << bestOF << "最佳比冲:" << bestIsp << "s";
}

void MainWindow::plotOFScanCurve()
{
    if (ofValues.isEmpty() || ispValues.isEmpty()) {
        qWarning() << "没有数据可用于绘制曲线";
        return;
    }

    if (!chart || !ispSeries || !optimalPoint || !chartView) {
        qWarning() << "图表组件未初始化";
        return;
    }

    qDebug() << "开始绘制真实曲线，数据点数:" << ofValues.size();

    // 清空之前的曲线数据
    ispSeries->clear();
    optimalPoint->clear();

    // 添加真实数据点
    for (int i = 0; i < ofValues.size(); ++i) {
        ispSeries->append(ofValues[i], ispValues[i]);
    }

    // 添加最佳点
    if (bestOF > 0 && bestIsp > 0) {
        optimalPoint->append(bestOF, bestIsp);
        qDebug() << "真实最佳点: O/F =" << bestOF << ", Isp =" << bestIsp;
    }

    // 更新图表标题
    QString oxid = ui->comboBox_oxidizer->currentText();
    QString fuel = ui->comboBox_fuel->currentText();
    chart->setTitle(QString("%1 / %2 - 比冲 vs 混合比").arg(oxid).arg(fuel));

    // 更新坐标轴
    QValueAxis *axisX = qobject_cast<QValueAxis*>(chart->axes(Qt::Horizontal).first());
    QValueAxis *axisY = qobject_cast<QValueAxis*>(chart->axes(Qt::Vertical).first());

    if (axisX && axisY) {
        // 计算数据范围
        double minOF = *std::min_element(ofValues.begin(), ofValues.end());
        double maxOF = *std::max_element(ofValues.begin(), ofValues.end());
        double minIsp = *std::min_element(ispValues.begin(), ispValues.end());
        double maxIsp = *std::max_element(ispValues.begin(), ispValues.end());

        // 确保有合理的范围
        if (maxOF - minOF < 0.1) {
            minOF = bestOF - 0.5;
            maxOF = bestOF + 0.5;
        }
        if (maxIsp - minIsp < 10) {
            minIsp = bestIsp - 20;
            maxIsp = bestIsp + 20;
        }

        // 添加边距
        double xMargin = (maxOF - minOF) * 0.1;
        double yMargin = (maxIsp - minIsp) * 0.1;

        // 确保边距不为零
        if (xMargin < 0.01) xMargin = 0.1;
        if (yMargin < 1.0) yMargin = 10.0;

        // 设置坐标轴范围
        axisX->setRange(minOF - xMargin, maxOF + xMargin);
        axisY->setRange(minIsp - yMargin, maxIsp + yMargin);

        qDebug() << "真实数据坐标轴范围: X[" << minOF - xMargin << "," << maxOF + xMargin
                 << "], Y[" << minIsp - yMargin << "," << maxIsp + yMargin << "]";
    }

    // 强制更新图表
    chartView->update();
    chartView->repaint();

    qDebug() << "真实曲线绘制完成";
}

// ===== 辅助函数 =====

void MainWindow::updateUIWithResults()
{
    // 更新热力学计算结果
    ui->lineEdit_temperature->setText(QString::number(ceaResult.Tc, 'f', 1));
    ui->lineEdit_gamma->setText(QString::number(ceaResult.gamma, 'f', 3));
    ui->lineEdit_cstar->setText(QString::number(ceaResult.cStar, 'f', 0));
    ui->lineEdit_isp->setText(QString::number(ceaResult.Isp_vac, 'f', 1));

    // 更新几何参数
    ui->lineEdit_chamber_dia->setText(QString::number(geometry.chamberDiameter, 'f', 2));
    ui->lineEdit_chamber_len->setText(QString::number(geometry.chamberLength, 'f', 2));
    ui->lineEdit_throat_dia->setText(QString::number(geometry.throatDiameter, 'f', 2));
    ui->lineEdit_exit_dia->setText(QString::number(geometry.exitDiameter, 'f', 2));
    ui->lineEdit_expansion_ratio->setText(QString::number(geometry.expansionRatio, 'f', 1));
    ui->lineEdit_half_angle->setText(QString::number(geometry.halfAngle, 'f', 1));

    // 更新喷注器参数
    ui->lineEdit_ox_orifice->setText(QString::number(injector.oxOrificeDia, 'f', 3));
    ui->lineEdit_fuel_orifice->setText(QString::number(injector.fuelOrificeDia, 'f', 3));
    ui->lineEdit_ox_count->setText(QString::number(injector.oxOrificeCount));
    ui->lineEdit_fuel_count->setText(QString::number(injector.fuelOrificeCount));
    ui->lineEdit_ox_pressure->setText(QString::number(injector.oxPressureDrop, 'f', 2));
    ui->lineEdit_fuel_pressure->setText(QString::number(injector.fuelPressureDrop, 'f', 2));
    ui->lineEdit_smd->setText(QString::number(injector.smd, 'f', 2));

    // 更新冷却系统参数
    ui->lineEdit_fin_height->setText(QString::number(cooling.finHeight, 'f', 2));
    ui->lineEdit_fin_width->setText(QString::number(cooling.finWidth, 'f', 2));
    ui->lineEdit_fin_spacing->setText(QString::number(cooling.finSpacing, 'f', 2));
    ui->lineEdit_fin_count->setText(QString::number(cooling.finCount));
}

void MainWindow::clearAllResults()
{
    // 清空所有结果显示
    ui->lineEdit_temperature->clear();
    ui->lineEdit_gamma->clear();
    ui->lineEdit_cstar->clear();
    ui->lineEdit_isp->clear();
    ui->lineEdit_chamber_dia->clear();
    ui->lineEdit_chamber_len->clear();
    ui->lineEdit_throat_dia->clear();
    ui->lineEdit_exit_dia->clear();
    ui->lineEdit_expansion_ratio->clear();
    ui->lineEdit_half_angle->clear();
    ui->lineEdit_ox_orifice->clear();
    ui->lineEdit_fuel_orifice->clear();
    ui->lineEdit_ox_count->clear();
    ui->lineEdit_fuel_count->clear();
    ui->lineEdit_ox_pressure->clear();
    ui->lineEdit_fuel_pressure->clear();
    ui->lineEdit_smd->clear();
    ui->lineEdit_fin_height->clear();
    ui->lineEdit_fin_width->clear();
    ui->lineEdit_fin_spacing->clear();
    ui->lineEdit_fin_count->clear();

    // 清空特征长度和收敛段长度
    ui->lineEdit_tezheng->clear();
    ui->lineEdit_shoulian_long->clear();
}

bool MainWindow::validateInputParameters()
{
    bool isValid = true;

    // 检查推力 (1-1000000 N)
    double thrust = ui->lineEdit_thrust->text().toDouble();
    if (thrust < 1 || thrust > 1000000) {
        ui->lineEdit_thrust->setStyleSheet("background-color: #ffcccc;");
        isValid = false;
    } else {
        ui->lineEdit_thrust->setStyleSheet("");
    }

    // 检查燃烧室压力 (1-300 bar)
    double pressure = ui->lineEdit_pressure->text().toDouble();
    if (pressure < 1 || pressure > 300) {
        ui->lineEdit_pressure->setStyleSheet("background-color: #ffcccc;");
        isValid = false;
    } else {
        ui->lineEdit_pressure->setStyleSheet("");
    }

    // 检查混合比 (0.1-10)
    double ofRatio = ui->lineEdit_of_ratio->text().toDouble();
    if (ofRatio < 0.1 || ofRatio > 10) {
        ui->lineEdit_of_ratio->setStyleSheet("background-color: #ffcccc;");
        isValid = false;
    } else {
        ui->lineEdit_of_ratio->setStyleSheet("");
    }

    // 检查扩张比 (1-200)
    double expansion = ui->lineEdit_expansion->text().toDouble();
    if (expansion < 1 || expansion > 200) {
        ui->lineEdit_expansion->setStyleSheet("background-color: #ffcccc;");
        isValid = false;
    } else {
        ui->lineEdit_expansion->setStyleSheet("");
    }

    return isValid;
}

void MainWindow::exportResultsToFile(const QString& filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "错误", "无法创建文件");
        return;
    }

    QTextStream out(&file);

#if QT_VERSION_MAJOR >= 6
    out.setEncoding(QStringConverter::Utf8);
#else
    out.setCodec("UTF-8");
#endif

    // 写入文件头
    out << "火箭发动机设计结果（基于NASA CEA计算）\n";
    out << "生成时间: " << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "\n";
    out << "CEA软件版本: NASA Chemical Equilibrium with Applications\n";
    out << "========================================\n\n";

    // 输入参数
    out << "【输入参数】\n";
    out << "推力: " << ui->lineEdit_thrust->text() << " N\n";
    out << "燃烧室压力: " << ui->lineEdit_pressure->text() << " bar\n";
    out << "混合比(O/F): " << ui->lineEdit_of_ratio->text() << "\n";
    out << "扩张比(ε): " << ui->lineEdit_expansion->text() << "\n";
    out << "氧化剂: " << ui->comboBox_oxidizer->currentText() << "\n";
    out << "燃料: " << ui->comboBox_fuel->currentText() << "\n\n";

    // CEA计算结果
    out << "【CEA计算结果】\n";
    out << "燃烧室温度: " << ui->lineEdit_temperature->text() << " K\n";
    out << "比热比(γ): " << ui->lineEdit_gamma->text() << "\n";
    out << "特征速度(c*): " << ui->lineEdit_cstar->text() << " m/s\n";
    out << "真空比冲: " << ui->lineEdit_isp->text() << " s\n";
    out << "海平面比冲: " << QString::number(ceaResult.Isp_sl, 'f', 1) << " s\n";
    out << "出口压力: " << QString::number(ceaResult.Pe / 1e5, 'f', 3) << " bar\n";
    out << "出口马赫数: " << QString::number(ceaResult.Mach_e, 'f', 2) << "\n\n";

    // 几何参数
    out << "【几何参数】\n";
    out << "燃烧室直径: " << ui->lineEdit_chamber_dia->text() << " mm\n";
    out << "燃烧室长度: " << ui->lineEdit_chamber_len->text() << " mm\n";
    out << "喉部直径: " << ui->lineEdit_throat_dia->text() << " mm\n";
    out << "出口直径: " << ui->lineEdit_exit_dia->text() << " mm\n";
    out << "扩张比: " << ui->lineEdit_expansion_ratio->text() << "\n";
    out << "扩张半角: " << ui->lineEdit_half_angle->text() << " °\n";
    out << "特征长度(L*): " << ui->lineEdit_tezheng->text() << " m\n";
    out << "收敛段长度: " << ui->lineEdit_shoulian_long->text() << " mm\n\n";

    // 喷注器参数
    out << "【喷注器参数】\n";
    out << "氧化剂孔直径: " << ui->lineEdit_ox_orifice->text() << " mm\n";
    out << "燃料孔直径: " << ui->lineEdit_fuel_orifice->text() << " mm\n";
    out << "氧化剂孔数: " << ui->lineEdit_ox_count->text() << "\n";
    out << "燃料孔数: " << ui->lineEdit_fuel_count->text() << "\n";
    out << "氧化剂压降: " << ui->lineEdit_ox_pressure->text() << " bar\n";
    out << "燃料压降: " << ui->lineEdit_fuel_pressure->text() << " bar\n";
    out << "雾化SMD: " << ui->lineEdit_smd->text() << " μm\n\n";

    // 冷却系统参数
    out << "【冷却系统参数】\n";
    out << "肋高: " << ui->lineEdit_fin_height->text() << " mm\n";
    out << "肋宽: " << ui->lineEdit_fin_width->text() << " mm\n";
    out << "肋间距: " << ui->lineEdit_fin_spacing->text() << " mm\n";
    out << "肋片数量: " << ui->lineEdit_fin_count->text() << "\n\n";

    // O/F扫描结果
    if (!ofValues.isEmpty()) {
        out << "【O/F扫描结果】\n";
        out << "最佳O/F: " << QString::number(bestOF, 'f', 3) << "\n";
        out << "最佳比冲: " << QString::number(bestIsp, 'f', 1) << " s\n";
        out << "扫描点数: " << ofValues.size() << "\n";
        out << "O/F\t比冲(s)\t燃烧室温度(K)\t比热比\t特征速度(m/s)\n";
        out << "----\t----\t----\t----\t----\n";

        for (int i = 0; i < ofValues.size(); ++i) {
            out << QString::number(ofValues[i], 'f', 3) << "\t"
                << QString::number(ispValues[i], 'f', 1) << "\t";

            // 重新获取完整结果（如果需要）
            if (i < ofValues.size()) {
                // 这里可以添加额外的数据
                out << "-\t-\t-";
            }
            out << "\n";
        }
    }

    file.close();

    QMessageBox::information(this, "导出成功",
                             QString("结果已成功导出到:\n%1").arg(filename));
}

void MainWindow::updateStatus(const QString& message, int timeout)
{
    if (timeout > 0) {
        statusBar()->showMessage(message, timeout);
    } else {
        statusBar()->showMessage(message);
    }

    // 更新日志
    qDebug() << "状态:" << message;
}
