#ifndef NOZZLE_H
#define NOZZLE_H

class Nozzle {
public:
    struct NozzleParams {
        double Dt;          // 喉部直径 (m)
        double At;          // 喉部面积 (m²)
        double De;          // 出口直径 (m)
        double Ae;          // 出口面积 (m²)
        double epsilon;     // 面积膨胀比 (Ae/At)
        double halfAngle;   // 扩张半角 (度)
        double nozzleLength;// 喷管长度 (m)
        double Cf;          // 推力系数
    };

    Nozzle();

    // 计算喷管参数
    NozzleParams calculateNozzle(double thrust_N, double Pc_Pa,
                                 double Pa, double gamma,
                                 double Pe_Pa = 0.0,
                                 double targetEpsilon = 40.0);

    // 计算推力系数
    double calculateThrustCoefficient(double Pc, double Pe, double Pa,
                                      double gamma, double epsilon);

private:
    double calculateExitMach(double Pe, double Pc, double gamma);
    double calculateAreaRatio(double Me, double gamma);
};
#endif // NOZZLE_H
