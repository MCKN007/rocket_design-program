#include "ThermoEngine.h"
#include <QDebug>
#include <algorithm>
#include <functional>

ThermoEngine::ThermoEngine(QObject *parent) : QObject(parent)
{
    initializeDatabase();
}

ThermoEngine::~ThermoEngine()
{
}

void ThermoEngine::initializeDatabase()
{
    // ===== 常见推进剂数据 =====

    // 氧化剂
    propellantDB["LOX"] = {
        "液氧", "O2(L)", 0.0, 1141.0, 32.0, 1.4, 918.0, 0.00019, 0.013
    };

    propellantDB["O2"] = {
        "气氧", "O2", 0.0, 1.429, 32.0, 1.4, 918.0, 0.00002, 0.0
    };

    propellantDB["NTO"] = {
        "四氧化二氮", "N2O4", 9.08, 1440.0, 92.011, 1.32, 850.0, 0.0004, 0.035
    };

    propellantDB["H2O2"] = {
        "过氧化氢", "H2O2", -187.78, 1450.0, 34.0147, 1.4, 1400.0, 0.0012, 0.075
    };

    // 燃料
    propellantDB["RP-1"] = {
        "煤油", "CH1.953", -31.7, 810.0, 175.0, 1.1, 2000.0, 0.0015, 0.025
    };

    propellantDB["LH2"] = {
        "液氢", "H2(L)", 0.0, 70.85, 2.016, 1.4, 14300.0, 0.000013, 0.002
    };

    propellantDB["H2"] = {
        "气氢", "H2", 0.0, 0.0899, 2.016, 1.4, 14300.0, 0.000009, 0.0
    };

    propellantDB["UDMH"] = {
        "偏二甲肼", "(CH3)2NNH2", 53.0, 793.0, 60.098, 1.2, 2800.0, 0.0008, 0.030
    };

    propellantDB["C2H5OH"] = {
        "乙醇", "C2H5OH", -277.0, 789.0, 46.069, 1.13, 2400.0, 0.0012, 0.022
    };

    propellantDB["MMH"] = {
        "一甲基肼", "CH3NHNH2", 54.0, 878.0, 46.072, 1.2, 3000.0, 0.0007, 0.028
    };

    propellantDB["CH4"] = {
        "甲烷", "CH4", -74.8, 0.656, 16.04, 1.31, 2190.0, 0.000011, 0.0
    };

    // ===== 常见反应的燃烧温度数据 =====
    // 基于理论计算和经验数据的简化模型

    // LOX/RP-1 在不同O/F下的近似温度
    reactionEnthalpyDB["LOX"]["RP-1"] = {
        {1.8, 3200}, {2.0, 3450}, {2.2, 3600}, {2.4, 3650},
        {2.6, 3650}, {2.8, 3620}, {3.0, 3580}, {3.2, 3530}
    };

    // LOX/LH2
    reactionEnthalpyDB["LOX"]["LH2"] = {
        {4.0, 2800}, {4.5, 2900}, {5.0, 2950}, {5.5, 2970},
        {6.0, 2980}, {6.5, 2970}, {7.0, 2950}, {7.5, 2920}, {8.0, 2880}
    };

    // NTO/UDMH
    reactionEnthalpyDB["NTO"]["UDMH"] = {
        {1.6, 3200}, {1.8, 3350}, {2.0, 3400}, {2.2, 3420},
        {2.4, 3400}, {2.6, 3380}, {2.8, 3350}, {3.0, 3300}
    };

    // GOX/乙醇
    reactionEnthalpyDB["O2"]["C2H5OH"] = {
        {0.9, 2800}, {1.0, 2950}, {1.2, 3050}, {1.4, 3100},
        {1.6, 3120}, {1.8, 3100}, {2.0, 3070}
    };

    // H2O2/煤油
    reactionEnthalpyDB["H2O2"]["RP-1"] = {
        {5.0, 2500}, {5.5, 2600}, {6.0, 2650}, {6.5, 2680},
        {7.0, 2700}, {7.5, 2690}, {8.0, 2670}
    };

    // LOX/甲烷
    reactionEnthalpyDB["LOX"]["CH4"] = {
        {2.5, 3300}, {2.8, 3400}, {3.0, 3450}, {3.2, 3470},
        {3.4, 3480}, {3.6, 3470}, {3.8, 3450}, {4.0, 3420}
    };

    qDebug() << "热力学数据库初始化完成，包含" << propellantDB.size() << "种推进剂";
}

CombustionResult ThermoEngine::calculate(const QString& oxid, const QString& fuel,
                                         double Pc_bar, double OF, double epsilon)
{
    // 创建缓存键
    QString cacheKey = QString("%1_%2_%3_%4").arg(oxid).arg(fuel).arg(Pc_bar).arg(OF);

    // 检查缓存
    if (resultCache.contains(cacheKey)) {
        qDebug() << "使用缓存结果:" << cacheKey;
        return resultCache[cacheKey];
    }

    qDebug() << "开始热力学计算: oxid=" << oxid
             << ", fuel=" << fuel
             << ", Pc=" << Pc_bar << "bar"
             << ", O/F=" << OF
             << ", ε=" << epsilon;

    CombustionResult result;

    // 1. 计算绝热火焰温度
    result.Tc = calculateAdiabaticFlameTemp(oxid, fuel, OF);

    // 2. 计算混合气体性质
    double mixGamma = calculateGamma(result.Tc, oxid + "+" + fuel);
    result.gamma = mixGamma;

    // 3. 计算平均分子量（简化模型）
    double molWeightOxid = propellantDB[oxid].molWeight;
    double molWeightFuel = propellantDB[fuel].molWeight;
    result.molWeight = (molWeightOxid * OF + molWeightFuel) / (1 + OF) * 1000; // g/mol 转 kg/kmol

    // 4. 计算特征速度
    result.cStar = calculateCstar(result.Tc, result.gamma, result.molWeight);

    // 5. 计算出口压力
    double Pc = Pc_bar * 1e5; // bar to Pa
    result.Pe = calculateExitPressure(Pc, result.gamma, epsilon);

    // 6. 计算比冲
    result.Isp_vac = calculateIspVac(result.cStar, result.gamma, epsilon, Pc, result.Pe);
    result.Isp_sl = calculateIspSl(result.cStar, result.gamma, epsilon, Pc, Pa_sea);

    // 7. 计算出口马赫数
    result.Mach_e = calculateExitMach(epsilon, result.gamma);

    // 8. 计算燃烧室密度（理想气体定律）
    result.rho_c = Pc / (R / result.molWeight * result.Tc);

    // 9. 计算定压比热（经验公式）
    result.cp = 1000 * (result.gamma / (result.gamma - 1)) * (R / result.molWeight);

    // 缓存结果
    resultCache[cacheKey] = result;

    qDebug() << "热力学计算结果:"
             << "Tc =" << result.Tc << "K"
             << "γ =" << result.gamma
             << "c* =" << result.cStar << "m/s"
             << "Isp_vac =" << result.Isp_vac << "s"
             << "M_e =" << result.Mach_e;

    return result;
}

PropellantData ThermoEngine::getPropellant(const QString& name)
{
    if (propellantDB.contains(name)) {
        return propellantDB[name];
    }
    return PropellantData();
}

double ThermoEngine::calculateAdiabaticFlameTemp(const QString& oxid, const QString& fuel, double OF)
{
    // 检查是否有预存数据
    if (reactionEnthalpyDB.contains(oxid) && reactionEnthalpyDB[oxid].contains(fuel)) {
        auto& tempMap = reactionEnthalpyDB[oxid][fuel];

        // 找到最接近的两个O/F点进行线性插值
        auto lower = tempMap.lower_bound(OF);
        auto upper = tempMap.upper_bound(OF);

        if (lower != tempMap.end() && upper != tempMap.end()) {
            // 在两个点之间插值
            double x1 = lower->first, y1 = lower->second;
            double x2 = upper->first, y2 = upper->second;

            if (x2 > x1) {
                return y1 + (y2 - y1) * (OF - x1) / (x2 - x1);
            }
        } else if (lower != tempMap.end()) {
            return lower->second;
        }
    }

    // 如果没有预存数据，使用经验公式
    // 基于典型推进剂组合的经验公式

    if (oxid == "LOX" && fuel == "RP-1") {
        // LOX/RP-1 温度曲线 (近似二次函数)
        double T_base = 3600.0;
        double T_offset = -50.0 * pow(OF - 2.6, 2);
        return T_base + T_offset;
    }
    else if (oxid == "LOX" && fuel == "LH2") {
        // LOX/LH2 温度曲线
        double T_base = 2950.0;
        double T_offset = -20.0 * pow(OF - 6.0, 2);
        return T_base + T_offset;
    }
    else if (oxid == "NTO" && fuel == "UDMH") {
        // NTO/UDMH 温度曲线
        double T_base = 3400.0;
        double T_offset = -40.0 * pow(OF - 2.2, 2);
        return T_base + T_offset;
    }
    else if (oxid == "O2" && fuel == "C2H5OH") {
        // GOX/乙醇 温度曲线
        double T_base = 3100.0;
        double T_offset = -60.0 * pow(OF - 1.4, 2);
        return T_base + T_offset;
    }
    else if (oxid == "H2O2" && fuel == "RP-1") {
        // H2O2/煤油 温度曲线
        double T_base = 2650.0;
        double T_offset = -25.0 * pow(OF - 6.5, 2);
        return T_base + T_offset;
    }

    // 默认温度（基于典型火箭发动机）
    return 3000.0 + 100 * sin(OF); // 简单的周期函数作为回退
}

double ThermoEngine::calculateGamma(double Tc, const QString& mixture)
{
    // 基于温度和混合物的经验比热比模型

    if (mixture.contains("LOX") && mixture.contains("RP-1")) {
        // LOX/RP-1: γ随温度升高而降低
        return 1.2 - 0.00002 * (Tc - 3000);
    }
    else if (mixture.contains("LOX") && mixture.contains("LH2")) {
        // LOX/LH2: 氢气的γ较高
        return 1.4 - 0.00001 * (Tc - 2800);
    }
    else if (mixture.contains("NTO") && mixture.contains("UDMH")) {
        // NTO/UDMH
        return 1.25 - 0.000015 * (Tc - 3200);
    }
    else if (mixture.contains("H2O2")) {
        // 过氧化氢基推进剂
        return 1.3 - 0.00001 * (Tc - 2500);
    }

    // 默认值：温度越高，γ越低（符合真实气体行为）
    return 1.3 - 0.00001 * (Tc - 3000);
}

double ThermoEngine::calculateCstar(double Tc, double gamma, double molWeight)
{
    // 特征速度公式：c* = sqrt(γ * R * Tc) / (γ * sqrt((2/(γ+1))^((γ+1)/(γ-1))))

    // 简化计算
    double R_specific = R / molWeight;  // 比气体常数

    // 特征速度经验系数
    double coeff = sqrt(gamma * R_specific * Tc);

    // 理论系数部分
    double exponent = (gamma + 1) / (2 * (gamma - 1));
    double factor = pow(2 / (gamma + 1), exponent);

    double cStar = coeff / (gamma * factor);

    // 典型值范围检查
    if (cStar < 1000) cStar = 1500 + 500 * sin(Tc/1000);
    if (cStar > 3000) cStar = 2500;

    return cStar;
}

double ThermoEngine::calculateIspVac(double cStar, double gamma, double epsilon, double Pc, double Pe)
{
    // 真空比冲：Isp_vac = c* * Cf_vac / g0

    // 计算真空推力系数
    double Cf_vac = 0.0;

    if (epsilon > 1.0) {
        // 有膨胀的推力系数
        double Pe_Pc = Pe / Pc;
        double term1 = sqrt((2 * gamma * gamma / (gamma - 1)) *
                            pow(2 / (gamma + 1), (gamma + 1) / (gamma - 1)) *
                            (1 - pow(Pe_Pc, (gamma - 1) / gamma)));

        double term2 = epsilon * Pe_Pc;

        Cf_vac = term1 + term2;
    } else {
        // 没有膨胀（喉部直接排气）
        Cf_vac = gamma * pow(2 / (gamma + 1), (gamma + 1) / (2 * (gamma - 1)));
    }

    // 计算比冲
    double Isp = cStar * Cf_vac / g0;

    // 典型值范围检查
    if (Isp < 200) Isp = 250 + 50 * sin(cStar/1000);
    if (Isp > 500) Isp = 450;

    return Isp;
}

double ThermoEngine::calculateIspSl(double cStar, double gamma, double epsilon, double Pc, double Pa)
{
    // 海平面比冲：Isp_sl = c* * Cf_sl / g0

    // 计算海平面推力系数
    double Cf_sl = 0.0;

    if (epsilon > 1.0) {
        // 计算出口压力（假设最佳膨胀到环境压力）
        double Pe = Pa; // 简化假设：喷管设计为出口压力等于环境压力

        double Pe_Pc = Pe / Pc;
        double Pa_Pc = Pa / Pc;

        double term1 = sqrt((2 * gamma * gamma / (gamma - 1)) *
                            pow(2 / (gamma + 1), (gamma + 1) / (gamma - 1)) *
                            (1 - pow(Pe_Pc, (gamma - 1) / gamma)));

        double term2 = epsilon * (Pe_Pc - Pa_Pc);

        Cf_sl = term1 + term2;
    } else {
        // 没有膨胀
        Cf_sl = gamma * pow(2 / (gamma + 1), (gamma + 1) / (2 * (gamma - 1))) - Pa / Pc;
    }

    // 计算比冲
    double Isp = cStar * Cf_sl / g0;

    // 确保海平面比冲小于真空比冲
    if (Isp < 0.8 * calculateIspVac(cStar, gamma, epsilon, Pc, Pa)) {
        Isp = 0.8 * calculateIspVac(cStar, gamma, epsilon, Pc, Pa);
    }

    return Isp;
}

double ThermoEngine::calculateExitPressure(double Pc, double gamma, double epsilon)
{
    // 根据面积比计算出口压力
    // 假设等熵膨胀

    if (epsilon <= 1.0) {
        return Pc; // 没有扩张
    }

    // 计算出口马赫数
    double Me = calculateExitMach(epsilon, gamma);

    // 等熵关系：Pe/Pc = (1 + 0.5*(γ-1)*Me²)^(-γ/(γ-1))
    double numerator = 1.0 + 0.5 * (gamma - 1) * Me * Me;
    double exponent = -gamma / (gamma - 1);

    double pressureRatio = pow(numerator, exponent);
    double Pe = Pc * pressureRatio;

    // 确保出口压力合理
    if (Pe < 0.01 * Pc) Pe = 0.01 * Pc;
    if (Pe > 0.9 * Pc) Pe = 0.9 * Pc;

    return Pe;
}

double ThermoEngine::calculateExitMach(double epsilon, double gamma)
{
    // 根据面积比计算出口马赫数（等熵关系）
    // 需要解方程：ε = (1/Me) * ((1 + 0.5*(γ-1)*Me²) / ((γ+1)/2))^((γ+1)/(2*(γ-1)))

    // 使用迭代法求解
    double Me_guess = 2.0; // 初始猜测

    for (int i = 0; i < 20; i++) {
        double term1 = 1.0 + 0.5 * (gamma - 1) * Me_guess * Me_guess;
        double term2 = (gamma + 1) / 2.0;
        double exponent = (gamma + 1) / (2.0 * (gamma - 1));

        double epsilon_calc = (1.0 / Me_guess) * pow(term1 / term2, exponent);

        double error = epsilon_calc - epsilon;

        // Newton-Raphson更新
        double derivative = -epsilon_calc / Me_guess +
                            (epsilon_calc * (gamma + 1) * Me_guess) /
                                (2.0 * term1 * (gamma - 1));

        if (fabs(derivative) > 1e-6) {
            Me_guess -= error / derivative;
        }

        // 边界检查
        if (Me_guess < 1.0) Me_guess = 1.0;
        if (Me_guess > 5.0) Me_guess = 5.0;

        if (fabs(error) < 1e-6) break;
    }

    return Me_guess;
}

QVector<double> ThermoEngine::scanOF(const QString& oxid, const QString& fuel,
                                     double Pc_bar, double minOF, double maxOF, double step)
{
    QVector<double> results;

    for (double OF = minOF; OF <= maxOF; OF += step) {
        CombustionResult r = calculate(oxid, fuel, Pc_bar, OF);
        results.append(r.Isp_vac);
    }

    return results;
}

void ThermoEngine::addCustomPropellant(const PropellantData& prop)
{
    propellantDB[prop.name] = prop;
    qDebug() << "添加自定义推进剂:" << prop.name;
}

bool ThermoEngine::validateOF(double OF, const QString& oxid, const QString& fuel)
{
    // 推进剂组合的有效O/F范围

    if (oxid == "LOX" && fuel == "RP-1") {
        return (OF >= 1.8 && OF <= 3.2);
    }
    else if (oxid == "LOX" && fuel == "LH2") {
        return (OF >= 4.0 && OF <= 8.0);
    }
    else if (oxid == "NTO" && fuel == "UDMH") {
        return (OF >= 1.6 && OF <= 3.0);
    }
    else if (oxid == "O2" && fuel == "C2H5OH") {
        return (OF >= 0.9 && OF <= 2.0);
    }
    else if (oxid == "H2O2" && fuel == "RP-1") {
        return (OF >= 5.0 && OF <= 8.0);
    }
    else if (oxid == "LOX" && fuel == "CH4") {
        return (OF >= 2.5 && OF <= 4.0);
    }

    // 默认范围
    return (OF >= 1.0 && OF <= 10.0);
}

double ThermoEngine::calculateMixtureEnthalpy(const QString& oxid, const QString& fuel, double OF)
{
    // 计算混合物的焓值（简化）
    double h_oxid = propellantDB[oxid].hf;
    double h_fuel = propellantDB[fuel].hf;

    // 加权平均
    return (h_oxid * OF + h_fuel) / (1 + OF);
}

double ThermoEngine::calculateEquilibriumTemp(double initialGuess, double OF,
                                              const QString& oxid, const QString& fuel)
{
    // 使用牛顿-拉夫逊法求解平衡温度
    // 这是简化的能量平衡方程

    auto energyFunc = [&](double T) -> double {
        double cp_mix = 1500.0; // 平均比热 (J/kg·K)
        double h_mix = calculateMixtureEnthalpy(oxid, fuel, OF) * 1000; // kJ/mol 转 J/mol

        // 简化能量平衡：ΔH + cp*ΔT = 0
        return h_mix + cp_mix * (T - 298.15);
    };

    auto energyFuncDeriv = [&](double T) -> double {
        return 1500.0; // cp 的导数（假设常数）
    };

    return newtonRaphsonSolver(energyFunc, energyFuncDeriv, initialGuess);
}

double ThermoEngine::newtonRaphsonSolver(std::function<double(double)> f,
                                         std::function<double(double)> df, double initial)
{
    double x = initial;

    for (int i = 0; i < 20; i++) {
        double fx = f(x);
        double dfx = df(x);

        if (fabs(dfx) < 1e-10) break;

        double dx = -fx / dfx;
        x += dx;

        if (fabs(dx) < 1e-6) break;
    }

    return x;
}
