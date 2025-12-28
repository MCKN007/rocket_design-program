#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtCharts>
#include <QVector>
#include "CEAEngine.h"

// O/F扫描范围结构体
struct OFScanRange {
    double min;
    double max;
    double step;
    double bestOF;
    double bestIsp;
};

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // 按钮点击事件
    void onCalculateButtonClicked();      // 计算按钮
    void onScanOFButtonClicked();         // O/F扫描按钮
    void onExportButtonClicked();         // 导出结果按钮
    void onCheckCEAButtonClicked();       // 检查CEA按钮

    // 输入变化事件
    void onInputParameterChanged();       // 输入参数变化时

    // 下拉框变化事件
    void onOxidizerChanged();             // 氧化剂变化
    void onFuelChanged();                 // 燃料变化

    // CEA引擎信号
    void onCEACalculationStarted();
    void onCEACalculationFinished(bool success, const QString& error);
    void onCEABatchProgress(int current, int total);

private:
    // 核心计算函数
    void runCEACalculation();             // 运行CEA计算
    void calculateEngineGeometry();       // 计算发动机几何参数
    void calculateInjectorParameters();   // 计算喷注器参数
    void calculateCoolingSystem();        // 计算冷却系统

    // O/F扫描功能
    void performOFScan();                 // 执行O/F扫描
    OFScanRange getOFScanRange(const QString& oxid, const QString& fuel); // 获取扫描范围
    void findOptimalOF();                 // 寻找最佳O/F
    void plotOFScanCurve();               // 绘制O/F扫描曲线

    // 辅助函数
    void updateUIWithResults();           // 更新UI显示结果
    void clearAllResults();               // 清空所有结果
    bool validateInputParameters();       // 验证输入参数
    void exportResultsToFile(const QString& filename); // 导出结果到文件
    void updateStatus(const QString& message, int timeout = 0); // 更新状态栏

    // 单位转换函数
    double mmToM(double mm) { return mm / 1000.0; }
    double mToMm(double m) { return m * 1000.0; }
    double barToPa(double bar) { return bar * 1e5; }
    double paToBar(double pa) { return pa / 1e5; }

    // 获取推荐O/F范围
    QPair<double, double> getRecommendedOFRange(const QString& oxid, const QString& fuel);

private:
    Ui::MainWindow *ui;
    CEAEngine* ceaEngine;

    // 图表相关
    QChart *chart;
    QLineSeries *ispSeries;
    QScatterSeries *optimalPoint;
    QChartView *chartView;

    // 数据存储
    QVector<double> ofValues;
    QVector<double> ispValues;

    // 当前计算结果
    CEAResult ceaResult;
    double bestOF;
    double bestIsp;

    // 发动机设计参数
    struct EngineGeometry {
        double chamberDiameter;      // 燃烧室直径 (mm)
        double chamberLength;        // 燃烧室长度 (mm)
        double throatDiameter;       // 喉部直径 (mm)
        double exitDiameter;         // 出口直径 (mm)
        double expansionRatio;       // 扩张比
        double halfAngle;            // 扩张半角 (度)
    } geometry;

    struct InjectorParams {
        double oxOrificeDia;         // 氧化剂孔直径 (mm)
        double fuelOrificeDia;       // 燃料孔直径 (mm)
        int oxOrificeCount;          // 氧化剂孔数
        int fuelOrificeCount;        // 燃料孔数
        double oxPressureDrop;       // 氧化剂压降 (bar)
        double fuelPressureDrop;     // 燃料压降 (bar)
        double smd;                  // 雾化SMD (μm)
    } injector;

    struct CoolingSystem {
        double finHeight;            // 肋高 (mm)
        double finWidth;             // 肋宽 (mm)
        double finSpacing;           // 肋间距 (mm)
        int finCount;                // 肋片数量
    } cooling;

    // 计算状态
    bool isCalculating;
};

#endif // MAINWINDOW_H
