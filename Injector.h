#ifndef INJECTOR_H
#define INJECTOR_H

#include <vector>

class Injector {
public:
    struct InjectorParams {
        // 同心圆式直流喷注器参数
        double d_orifice_ox;      // 氧化剂孔直径 (m)
        double d_orifice_fuel;    // 燃料孔直径 (m)
        int num_orifices_ox;      // 氧化剂孔数
        int num_orifices_fuel;    // 燃料孔数
        double deltaP_ox;         // 氧化剂压降 (Pa)
        double deltaP_fuel;       // 燃料压降 (Pa)
        double velocity_ox;       // 氧化剂喷射速度 (m/s)
        double velocity_fuel;     // 燃料喷射速度 (m/s)
        double SMD;               // 索特尔平均直径 (μm) - 雾化程度
        double Weber;             // 韦伯数
        double Reynolds;          // 雷诺数
    };

    struct Propellant {
        QString name;
        double density;      // 密度 (kg/m³)
        double viscosity;    // 粘度 (Pa·s)
        double surfaceTension; // 表面张力 (N/m)
    };

    Injector();

    // 计算喷注器最优参数
    InjectorParams calculateOptimalInjector(
        double mdot_ox, double mdot_fuel,
        const Propellant& ox, const Propellant& fuel,
        double Pc, double OF,
        double targetDeltaP_ratio = 0.2); // 压降比通常为燃烧室压力的15-25%

    // 计算雾化程度（SMD）
    double calculateSMD(double d_orifice, double velocity,
                        double density, double viscosity,
                        double surfaceTension);

    // 计算压降
    double calculatePressureDrop(double mdot, double Cd, double A,
                                 double density, double numOrifices);

private:
    const double Cd = 0.7;  // 流量系数
    const double PI = 3.141592653589793;
};
#endif // INJECTOR_H
