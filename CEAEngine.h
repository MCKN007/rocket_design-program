#ifndef CEAENGINE_H
#define CEAENGINE_H

#include <QObject>
#include <QString>
#include <QMap>
#include <QVector>

// CEA计算结果结构体
struct CEAResult {
    double Tc = 0.0;          // 燃烧室温度 (K)
    double gamma = 0.0;       // 比热比
    double molWeight = 0.0;   // 分子量 (kg/kmol)
    double cStar = 0.0;       // 特征速度 (m/s)
    double Isp_vac = 0.0;     // 真空比冲 (s)
    double Isp_sl = 0.0;      // 海平面比冲 (s)
    double cp = 0.0;          // 定压比热 (J/kg·K)
    double Pe = 0.0;          // 出口压力 (Pa)
    double Mach_e = 0.0;      // 出口马赫数
    double rho_c = 0.0;       // 燃烧室密度 (kg/m³)

    // 有效性检查
    bool isValid() const {
        return Tc > 0 && gamma > 0 && cStar > 0 && Isp_vac > 0;
    }

    // 默认构造函数
    CEAResult() = default;
};

class CEAEngine : public QObject
{
    Q_OBJECT

public:
    explicit CEAEngine(QObject *parent = nullptr);
    ~CEAEngine();

    // 运行CEA计算（火箭模式）
    bool runCEA(const QString& oxid, const QString& fuel,
                double Pc_bar, double OF, double epsilon = 40.0);

    // 获取计算结果
    CEAResult getResult() const { return m_result; }

    // 检查CEA是否可用
    bool isCEAAvailable() const;

    // 获取推进剂属性
    static double getDensity(const QString& propellant);
    static double getViscosity(const QString& propellant);
    static double getSurfaceTension(const QString& propellant);

    // 设置/获取CEA路径
    void setCEAPath(const QString& path) { m_ceaExe = path; }
    QString getCEAPath() const { return m_ceaExe; }

    // 批量计算（O/F扫描）
    QVector<CEAResult> batchCalculate(const QString& oxid, const QString& fuel,
                                      double Pc_bar, const QVector<double>& OFs,
                                      double epsilon = 40.0);

signals:
    void calculationStarted();
    void calculationProgress(int current, int total);
    void calculationFinished(bool success, const QString& error = "");
    void batchProgress(int current, int total);

private:
    // 写入输入文件
    bool writeInputFile(const QString& oxid, const QString& fuel,
                        double Pc_bar, double OF, double epsilon);

    // 解析输出文件
    bool parseOutputFile();

    // 清理临时文件
    void cleanupTempFiles();

    // 运行CEA进程
    bool runCEAProcess();

    // 检查文件完整性
    bool checkInputFile() const;
    bool checkOutputFile() const;

    // 解析输出文件（增强版）
    bool parseOutputFileEnhanced();

private:
    QString m_ceaExe = "cea/FCEA2.exe";      // CEA可执行文件路径
    QString m_inputFile = "thermo.inp";    // 输入文件名
    QString m_outputFile = "thermo.out";   // 输出文件名

    CEAResult m_result;                     // 计算结果

    // 状态标志
    bool m_isRunning = false;
    QString m_lastError;

    // 解析模式
    enum class ParseMode {
        Standard,
        Enhanced,
        Fallback
    };

    ParseMode m_parseMode = ParseMode::Standard;
};

#endif // CEAENGINE_H
