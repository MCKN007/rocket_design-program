#ifndef COMBUSTIONCHAMBER_H
#define COMBUSTIONCHAMBER_H

#include <cmath>

class CombustionChamber {
public:
    struct ChamberParams {
        double Dc;      // 燃烧室直径 (m)
        double Lc;      // 燃烧室长度 (m)
        double Vc;      // 燃烧室容积 (m³)
        double Ac;      // 横截面积 (m²)
        double Lstar;   // 特征长度 (m) - 停留时间参数
    };

    struct CoolingFinParams {
        double finHeight;    // 肋高 (m)
        double finWidth;     // 肋宽 (m)
        double finSpacing;   // 肋间距 (m)
        int numFins;         // 肋片数量
        double heatFlux;     // 热流密度 (W/m²)
        double wallTemp;     // 壁温 (K)
    };

    CombustionChamber();

    // 计算燃烧室尺寸
    ChamberParams calculateChamber(double thrust_N, double Pc_Pa,
                                   double mdot_total, double cStar,
                                   double gamma, double Tc);

    // 计算冷却肋参数（再生冷却）
    CoolingFinParams calculateCoolingFins(double heatTransferRate,
                                          double coolantTemp,
                                          double wallMaterialK,
                                          double chamberLength,
                                          double chamberDiameter);

private:
    const double LSTAR_TYPICAL = 0.8;  // 典型特征长度 (m)
};
#endif // COMBUSTIONCHAMBER_H
