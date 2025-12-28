#ifndef THERMOENGINE_H
#define THERMOENGINE_H

#include <QObject>
#include <QMap>
#include <QString>
#include <cmath>

// 推进剂数据
struct PropellantData {
    QString name;           // 推进剂名称
    QString formula;        // 化学式
    double hf;              // 生成焓 (kJ/mol)
    double density;         // 密度 (kg/m³)
    double molWeight;       // 分子量 (g/mol)
    double gamma;           // 比热比
    double cp;              // 定压比热 (J/kg·K)
    double viscosity;       // 粘度 (Pa·s)
    double surfaceTension;  // 表面张力 (N/m)
};

// 燃烧计算结果
struct CombustionResult {
    double Tc;              // 燃烧室温度 (K)
    double gamma;           // 比热比
    double cStar;           // 特征速度 (m/s)
    double Isp_vac;         // 真空比冲 (s)
    double Isp_sl;          // 海平面比冲 (s)
    double molWeight;       // 分子量 (kg/kmol)
    double cp;              // 定压比热 (J/kg·K)
    double Pe;              // 出口压力 (Pa)
    double Mach_e;          // 出口马赫数
    double rho_c;           // 燃烧室密度 (kg/m³)
};

// 反应物
struct Reactant {
    QString name;
    double moles;           // 摩尔数
    double enthalpy;        // 焓值 (kJ/mol)
};

class ThermoEngine : public QObject
{
    Q_OBJECT

public:
    explicit ThermoEngine(QObject *parent = nullptr);
    ~ThermoEngine();

    // 热力学计算
    CombustionResult calculate(const QString& oxid, const QString& fuel,
                               double Pc_bar, double OF, double epsilon = 40.0);

    // 推进剂属性
    PropellantData getPropellant(const QString& name);

    // 温度相关计算
    double calculateAdiabaticFlameTemp(const QString& oxid, const QString& fuel, double OF);
    double calculateGamma(double Tc, const QString& mixture);
    double calculateCstar(double Tc, double gamma, double molWeight);
    double calculateIspVac(double cStar, double gamma, double epsilon, double Pc, double Pe);
    double calculateIspSl(double cStar, double gamma, double epsilon, double Pc, double Pa = 101325.0);

    // 喷管计算
    double calculateExitPressure(double Pc, double gamma, double epsilon);
    double calculateExitMach(double epsilon, double gamma);

    // O/F扫描
    QVector<double> scanOF(const QString& oxid, const QString& fuel,
                           double Pc_bar, double minOF, double maxOF, double step);

    // 添加自定义推进剂
    void addCustomPropellant(const PropellantData& prop);

    // 数据验证
    bool validateOF(double OF, const QString& oxid, const QString& fuel);

private:
    // 推进剂数据库
    QMap<QString, PropellantData> propellantDB;

    // 反应数据库
    QMap<QString, QMap<QString, double>> reactionEnthalpyDB;

    // 物理常数
    const double R = 8314.4621;     // 通用气体常数 (J/kmol·K)
    const double g0 = 9.80665;      // 重力加速度 (m/s²)
    const double Pa_sea = 101325.0; // 海平面大气压 (Pa)

    // 初始化数据库
    void initializeDatabase();

    // 辅助函数
    double calculateMixtureEnthalpy(const QString& oxid, const QString& fuel, double OF);
    double calculateEquilibriumTemp(double initialGuess, double OF, const QString& oxid, const QString& fuel);
    double newtonRaphsonSolver(std::function<double(double)> f, std::function<double(double)> df, double initial);

    // 缓存结果
    QMap<QString, CombustionResult> resultCache;
};

#endif // THERMOENGINE_H
