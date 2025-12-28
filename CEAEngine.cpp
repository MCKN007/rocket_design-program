#include "CEAEngine.h"
#include <QFile>
#include <QTextStream>
#include <QProcess>
#include <QRegularExpression>
#include <QDir>
#include <QDebug>
#include <QCoreApplication>
#include <QThread>
#include <QElapsedTimer>
#include <cmath>

// 推进剂名称映射
static QMap<QString, QString> ceaNameMap = {
    {"LOX", "O2(L)"},        // 液氧
    {"O2", "O2"},            // 气氧
    {"GOX", "O2"},           // 气氧（同义）
    {"NTO", "N2O4"},         // 四氧化二氮
    {"H2O2", "H2O2"},        // 过氧化氢
    {"RP-1", "RP-1"},        // 煤油
    {"LH2", "H2(L)"},        // 液氢
    {"H2", "H2"},            // 气氢
    {"UDMH", "UDMH"},        // 偏二甲肼
    {"C2H5OH", "C2H5OH(L)"}, // 乙醇
    {"CH4", "CH4"},          // 甲烷
    {"MMH", "MMH"}           // 一甲基肼
};

// 推进剂温度映射（单位：K）
static QMap<QString, double> propellantTempMap = {
    // 低温推进剂
    {"O2(L)", 90.17},        // 液氧（沸点）
    {"H2(L)", 20.27},        // 液氢（沸点）

    // 常温液态推进剂
    {"RP-1", 293.15},        // 煤油（室温）
    {"N2O4", 293.15},        // 四氧化二氮（室温）
    {"UDMH", 293.15},        // 偏二甲肼（室温）
    {"MMH", 293.15},         // 一甲基肼（室温）
    {"C2H5OH(L)", 293.15},   // 乙醇（室温）
    {"H2O2", 293.15},        // 过氧化氢（室温）

    // 气态推进剂
    {"O2", 298.15},          // 气氧（常温）
    {"H2", 298.15},          // 气氢（常温）
    {"CH4", 298.15},         // 甲烷（常温）
};

// 重力加速度常量
const double G0 = 9.80665;

CEAEngine::CEAEngine(QObject *parent) : QObject(parent)
{
    // 获取应用程序目录
    QString appDir = QCoreApplication::applicationDirPath();

    // 确保CEA目录存在
    QDir ceaDir("cea");
    if (!ceaDir.exists()) {
        qDebug() << "创建CEA目录...";
        if (!ceaDir.mkpath(".")) {
            qWarning() << "无法创建CEA目录";
        }
    }

    // 检查CEA目录下的文件
    qDebug() << "CEA目录内容:";
    QStringList ceaFiles = ceaDir.entryList(QDir::Files);
    if (ceaFiles.isEmpty()) {
        qDebug() << "  (空)";
    } else {
        for (const QString& file : ceaFiles) {
            qDebug() << "  " << file << "大小:" << QFileInfo("cea/" + file).size() << "字节";
        }
    }

    // 尝试多种可能的CEA路径
    QStringList possiblePaths = {
        "cea/FCEA2.exe",
        appDir + "/cea/FCEA2.exe",
        "FCEA2.exe",
        "./FCEA2.exe"
    };

    bool found = false;
    for (const QString& path : possiblePaths) {
        if (QFile::exists(path)) {
            m_ceaExe = QDir::toNativeSeparators(path);
            qDebug() << "找到CEA可执行文件:" << m_ceaExe;
            found = true;
            break;
        }
    }

    if (!found) {
        qWarning() << "未找到CEA可执行文件";
        qWarning() << "请确保FCEA2.exe位于以下位置之一:";
        qWarning() << "1. 应用程序目录下的cea/子目录";
        qWarning() << "2. 与应用程序相同的目录";
        qWarning() << "当前应用程序目录:" << appDir;
    } else {
        qDebug() << "CEA引擎初始化完成";
    }
}

CEAEngine::~CEAEngine()
{
    // 清理临时文件
    cleanupTempFiles();
}

bool CEAEngine::runCEA(const QString& oxid, const QString& fuel,
                       double Pc_bar, double OF, double epsilon)
{
    // 重置结果和状态
    m_result = CEAResult();
    m_lastError.clear();
    m_isRunning = true;

    emit calculationStarted();

    qDebug() << "==========================================";
    qDebug() << "开始CEA计算:";
    qDebug() << "氧化剂:" << oxid;
    qDebug() << "燃料:" << fuel;
    qDebug() << "燃烧室压力:" << Pc_bar << "bar";
    qDebug() << "混合比(O/F):" << OF;
    qDebug() << "面积比(ε):" << epsilon;
    qDebug() << "==========================================";

    // 验证输入参数
    if (Pc_bar <= 0 || Pc_bar > 1000) {
        m_lastError = QString("燃烧室压力 %1 bar 超出有效范围 (0-1000 bar)").arg(Pc_bar);
        qWarning() << m_lastError;
        m_isRunning = false;
        emit calculationFinished(false, m_lastError);
        return false;
    }

    if (OF <= 0 || OF > 20) {
        m_lastError = QString("混合比 %1 超出有效范围 (0-20)").arg(OF);
        qWarning() << m_lastError;
        m_isRunning = false;
        emit calculationFinished(false, m_lastError);
        return false;
    }

    if (epsilon < 1 || epsilon > 500) {
        m_lastError = QString("面积比 %1 超出有效范围 (1-500)").arg(epsilon);
        qWarning() << m_lastError;
        m_isRunning = false;
        emit calculationFinished(false, m_lastError);
        return false;
    }

    // 检查CEA可用性
    if (!isCEAAvailable()) {
        m_lastError = "CEA可执行文件未找到";
        qWarning() << m_lastError;
        m_isRunning = false;
        emit calculationFinished(false, m_lastError);
        return false;
    }

    // 写入输入文件
    if (!writeInputFile(oxid, fuel, Pc_bar, OF, epsilon)) {
        m_lastError = "无法创建CEA输入文件";
        qWarning() << m_lastError;
        m_isRunning = false;
        emit calculationFinished(false, m_lastError);
        return false;
    }

    // 运行CEA进程
    if (!runCEAProcess()) {
        m_lastError = "CEA进程执行失败";
        qWarning() << m_lastError;
        m_isRunning = false;
        emit calculationFinished(false, m_lastError);
        return false;
    }

    // 解析输出文件
    bool parseSuccess = false;

    // 尝试增强解析模式
    if (m_parseMode == ParseMode::Enhanced || m_parseMode == ParseMode::Fallback) {
        parseSuccess = parseOutputFileEnhanced();
    }

    // 如果增强模式失败，尝试标准模式
    if (!parseSuccess) {
        parseSuccess = parseOutputFile();
    }

    // 如果解析失败，提供更多信息
    if (!parseSuccess) {
        m_lastError = "无法解析CEA输出文件";
        qWarning() << m_lastError;

        // 检查输出文件是否存在
        QFile outFile("cea/" + m_outputFile);
        if (outFile.exists()) {
            qDebug() << "输出文件存在，大小:" << outFile.size() << "字节";
            if (outFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QTextStream in(&outFile);
                QString content = in.read(1000); // 读取前1000个字符
                qDebug() << "输出文件前1000字符:";
                qDebug() << content;
                outFile.close();
            }
        } else {
            qDebug() << "输出文件不存在";
        }

        m_isRunning = false;
        emit calculationFinished(false, m_lastError);
        return false;
    }

    // 验证结果
    if (!m_result.isValid()) {
        m_lastError = "CEA计算结果无效";
        qWarning() << m_lastError;
        qWarning() << "计算结果:"
                   << "Tc=" << m_result.Tc
                   << "γ=" << m_result.gamma
                   << "c*=" << m_result.cStar
                   << "Isp=" << m_result.Isp_vac;
        m_isRunning = false;
        emit calculationFinished(false, m_lastError);
        return false;
    }

    qDebug() << "CEA计算成功完成:";
    qDebug() << "燃烧室温度:" << m_result.Tc << "K";
    qDebug() << "比热比:" << m_result.gamma;
    qDebug() << "特征速度:" << m_result.cStar << "m/s";
    qDebug() << "真空比冲:" << m_result.Isp_vac << "s";
    qDebug() << "出口压力:" << m_result.Pe << "Pa";
    qDebug() << "出口马赫数:" << m_result.Mach_e;
    qDebug() << "==========================================";

    m_isRunning = false;
    emit calculationFinished(true);
    return true;
}

bool CEAEngine::writeInputFile(const QString& oxid, const QString& fuel,
                               double Pc_bar, double OF, double epsilon)
{
    // 获取CEA识别的推进剂名称
    QString ceaOxid = ceaNameMap.value(oxid.toUpper(), oxid);
    QString ceaFuel = ceaNameMap.value(fuel.toUpper(), fuel);

    // 如果映射失败，使用原始名称
    if (ceaOxid.isEmpty()) ceaOxid = oxid;
    if (ceaFuel.isEmpty()) ceaFuel = fuel;

    // 获取推进剂温度
    double oxidTemp = propellantTempMap.value(ceaOxid, 298.15);
    double fuelTemp = propellantTempMap.value(ceaFuel, 298.15);

    qDebug() << "推进剂温度 - 氧化剂:" << ceaOxid << oxidTemp << "K, 燃料:" << ceaFuel << fuelTemp << "K";

    QFile file("cea/" + m_inputFile);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "无法打开输入文件:" << file.fileName();
        return false;
    }

    QTextStream out(&file);

    // 标准CEA输入格式（火箭模式）
    out << "problem rocket\n";
    out << "p,bar=" << QString::number(Pc_bar, 'f', 2) << "\n";
    out << "o/f=" << QString::number(OF, 'f', 3) << "\n";
    out << "supar=" << QString::number(epsilon, 'f', 2) << "\n";
    out << "react\n";
    out << "fuel=" << ceaFuel << "  wt=100  t,k=" << QString::number(fuelTemp, 'f', 2) << "\n";
    out << "oxid=" << ceaOxid << "  wt=100  t,k=" << QString::number(oxidTemp, 'f', 2) << "\n";
    out << "output\n";
    out << "short\n";
    out << "siunits\n";
    out << "end\n";

    file.close();

    qDebug() << "输入文件已创建:" << file.fileName();

    // 显示文件内容用于验证
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "输入文件内容:";
        QTextStream in(&file);
        while (!in.atEnd()) {
            qDebug() << "  " << in.readLine();
        }
        file.close();
    }

    return true;
}

bool CEAEngine::runCEAProcess()
{
    QProcess process;

    // 设置工作目录为CEA目录
    QString ceaPath = "cea";
    QDir ceaDir(ceaPath);
    if (!ceaDir.exists()) {
        qWarning() << "CEA目录不存在:" << ceaPath;
        return false;
    }

    process.setWorkingDirectory(ceaPath);
    qDebug() << "工作目录设置为:" << process.workingDirectory();

    // 检查输入文件是否存在
    QString inputFilePath = ceaPath + "/" + m_inputFile;
    if (!QFile::exists(inputFilePath)) {
        qWarning() << "输入文件不存在:" << inputFilePath;
        return false;
    }

    qDebug() << "输入文件存在:" << inputFilePath;
    qDebug() << "启动CEA进程:" << m_ceaExe;

    // 启动CEA进程
    process.start(m_ceaExe, QStringList());

    if (!process.waitForStarted(5000)) {
        qWarning() << "CEA进程启动失败:" << process.errorString();
        return false;
    }

    qDebug() << "CEA进程已启动";

    // 等待CEA输出提示信息
    int maxWaitTime = 5000; // 5秒
    QElapsedTimer timer;
    timer.start();

    QString allOutput;
    bool gotPrompt = false;

    while (timer.elapsed() < maxWaitTime) {
        if (process.waitForReadyRead(100)) {
            QByteArray newData = process.readAllStandardOutput();
            allOutput += QString::fromLocal8Bit(newData);

            // 检查是否出现输入提示
            if (allOutput.contains("ENTER INPUT FILE NAME", Qt::CaseInsensitive)) {
                qDebug() << "检测到CEA输入提示";
                gotPrompt = true;
                break;
            }
        }
    }

    if (!gotPrompt) {
        qDebug() << "未收到CEA输入提示，继续尝试...";
        qDebug() << "已接收的输出:" << allOutput;
    }

    // 准备文件名（不带.inp扩展名）
    QString baseName = m_inputFile;
    if (baseName.endsWith(".inp", Qt::CaseInsensitive)) {
        baseName = baseName.left(baseName.length() - 4);
    }

    qDebug() << "向CEA输入文件名:" << baseName;

    // 发送文件名并回车（Windows使用\r\n换行）
    QString input = baseName + "\r\n";
    qint64 bytesWritten = process.write(input.toLocal8Bit());
    qDebug() << "写入字节数:" << bytesWritten;

    process.waitForBytesWritten(1000);
    process.closeWriteChannel();

    qDebug() << "等待CEA计算完成...";

    // 等待计算完成（最多30秒）
    if (!process.waitForFinished(30000)) {
        qWarning() << "CEA计算超时";

        // 读取剩余输出
        QString output = QString::fromLocal8Bit(process.readAllStandardOutput());
        QString error = QString::fromLocal8Bit(process.readAllStandardError());

        qDebug() << "超时时的标准输出:" << output;
        qDebug() << "超时时的错误输出:" << error;

        process.kill();
        process.waitForFinished(5000);
        return false;
    }

    // 读取所有输出
    QString output = QString::fromLocal8Bit(process.readAllStandardOutput());
    QString error = QString::fromLocal8Bit(process.readAllStandardError());

    qDebug() << "CEA进程退出代码:" << process.exitCode();

    if (!output.isEmpty()) {
        qDebug() << "CEA标准输出:";
        QStringList lines = output.split('\n');
        for (int i = 0; i < qMin(10, lines.size()); i++) {
            qDebug() << "  " << lines[i];
        }
    }

    if (!error.isEmpty()) {
        qDebug() << "CEA错误输出:" << error;
    }

    // 检查输出文件
    QString outputFilePath = ceaPath + "/" + m_outputFile;
    QFile outFile(outputFilePath);

    if (outFile.exists()) {
        qint64 size = outFile.size();
        qDebug() << "输出文件已生成，大小:" << size << "字节";

        if (size > 1000) {
            // 读取前几行看看内容
            if (outFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QTextStream in(&outFile);
                for (int i = 0; i < 5 && !in.atEnd(); i++) {
                    QString line = in.readLine();
                    qDebug() << "输出文件第" << i+1 << "行:" << line;
                }
                outFile.close();
            }
            return true;
        } else {
            qWarning() << "输出文件过小，可能计算失败";

            // 显示文件内容
            if (outFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QString content = outFile.readAll();
                qDebug() << "输出文件完整内容:" << content;
                outFile.close();
            }
            return false;
        }
    } else {
        qWarning() << "输出文件未生成:" << outputFilePath;
        return false;
    }
}

bool CEAEngine::parseOutputFile()
{
    QFile file("cea/" + m_outputFile);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "无法打开输出文件:" << file.fileName();
        return false;
    }

    QString content = file.readAll();
    file.close();

    if (content.isEmpty()) {
        qWarning() << "输出文件为空";
        return false;
    }

    qDebug() << "输出文件大小:" << content.size() << "字节";

    // 检查是否有错误
    if (content.contains("ERROR", Qt::CaseInsensitive) ||
        content.contains("FATAL", Qt::CaseInsensitive)) {
        qWarning() << "CEA输出文件中包含错误";

        // 提取错误信息
        QRegularExpression errorRe(R"(ERROR.*\n)");
        auto errorMatch = errorRe.match(content);
        if (errorMatch.hasMatch()) {
            qWarning() << "错误详情:" << errorMatch.captured();
        }
        return false;
    }

    // 重置结果
    m_result = CEAResult();

    // 解析CEA输出格式
    QStringList lines = content.split('\n');

    for (int i = 0; i < lines.size(); i++) {
        QString line = lines[i];

        // 1. 燃烧室温度 (K)
        if (line.contains("T, K") && m_result.Tc <= 0) {
            // 提取数值
            QRegularExpression re(R"([\d\.]+)");
            auto match = re.match(line);
            if (match.hasMatch()) {
                m_result.Tc = match.captured().toDouble();
                qDebug() << "找到燃烧室温度:" << m_result.Tc << "K";
            }
        }

        // 2. 比热比 (GAMMAs)
        else if (line.contains("GAMMAs") && m_result.gamma <= 0) {
            QRegularExpression re(R"([\d\.]+)");
            auto match = re.match(line);
            if (match.hasMatch()) {
                m_result.gamma = match.captured().toDouble();
                qDebug() << "找到比热比:" << m_result.gamma;
            }
        }

        // 3. 特征速度 (CSTAR, M/SEC)
        else if ((line.contains("CSTAR, M/SEC") || line.contains("C*, M/SEC")) && m_result.cStar <= 0) {
            QRegularExpression re(R"([\d\.]+)");
            auto match = re.match(line);
            if (match.hasMatch()) {
                m_result.cStar = match.captured().toDouble();
                qDebug() << "找到特征速度:" << m_result.cStar << "m/s";
            }
        }

        // 4. 真空比冲 (Ivac, M/SEC)
        else if ((line.contains("Ivac, M/SEC") || line.contains("Isp, M/SEC")) && m_result.Isp_vac <= 0) {
            QRegularExpression re(R"([\d\.]+)");
            auto match = re.match(line);
            if (match.hasMatch()) {
                double isp_msec = match.captured().toDouble();
                m_result.Isp_vac = isp_msec / G0;  // 转换为秒
                qDebug() << "找到真空比冲:" << m_result.Isp_vac << "s (原始值:" << isp_msec << "m/s)";
            }
        }

        // 5. 出口压力 (从bar转换为Pa)
        else if (line.contains("P, BAR") && line.contains("EQUIL") && m_result.Pe <= 0) {
            // 查找出口压力行
            for (int j = i + 1; j < lines.size() && j < i + 10; j++) {
                QString nextLine = lines[j];
                QRegularExpression re(R"(\d+\.\d+)");
                auto match = re.match(nextLine);
                if (match.hasMatch()) {
                    m_result.Pe = match.captured().toDouble() * 1e5;  // bar to Pa
                    qDebug() << "找到出口压力:" << m_result.Pe << "Pa";
                    break;
                }
            }
        }

        // 6. 出口马赫数
        else if (line.contains("Mach Number") && m_result.Mach_e <= 0) {
            QRegularExpression re(R"([\d\.]+)");
            auto match = re.match(line);
            if (match.hasMatch()) {
                m_result.Mach_e = match.captured().toDouble();
                qDebug() << "找到出口马赫数:" << m_result.Mach_e;
            }
        }

        // 7. 分子量 (kg/kmol)
        else if (line.contains("M, (1/n)") && m_result.molWeight <= 0) {
            QRegularExpression re(R"([\d\.]+)");
            auto match = re.match(line);
            if (match.hasMatch()) {
                m_result.molWeight = match.captured().toDouble();
                qDebug() << "找到分子量:" << m_result.molWeight;
            }
        }

        // 8. 定压比热 (J/kg·K)
        else if (line.contains("Cp, KJ/(KG)(K)") && m_result.cp <= 0) {
            QRegularExpression re(R"([\d\.]+)");
            auto match = re.match(line);
            if (match.hasMatch()) {
                m_result.cp = match.captured().toDouble() * 1000;  // kJ to J
                qDebug() << "找到定压比热:" << m_result.cp << "J/kg·K";
            }
        }
    }

    // 如果关键参数缺失，尝试备选解析
    if (m_result.Isp_vac <= 0) {
        // 尝试查找"Isp, M/SEC"格式
        for (int i = 0; i < lines.size(); i++) {
            if (lines[i].contains("Isp, M/SEC")) {
                QRegularExpression re(R"([\d\.]+)");
                auto match = re.match(lines[i]);
                if (match.hasMatch()) {
                    double isp_msec = match.captured().toDouble();
                    m_result.Isp_vac = isp_msec / G0;
                    break;
                }
            }
        }
    }

    // 计算燃烧室密度（使用理想气体定律）
    if (m_result.Tc > 0 && m_result.molWeight > 0 && m_result.Pe > 0) {
        double R = 8314.4621; // 通用气体常数 (J/kmol·K)
        m_result.rho_c = m_result.Pe / (R / m_result.molWeight * m_result.Tc);
    }

    // 验证结果
    if (m_result.isValid()) {
        qDebug() << "CEA解析成功:";
        qDebug() << "  Tc:" << m_result.Tc << "K";
        qDebug() << "  gamma:" << m_result.gamma;
        qDebug() << "  cStar:" << m_result.cStar << "m/s";
        qDebug() << "  Isp_vac:" << m_result.Isp_vac << "s";
        return true;
    } else {
        qWarning() << "CEA解析失败，关键参数缺失";
        qDebug() << "当前解析结果:"
                 << "Tc=" << m_result.Tc
                 << "gamma=" << m_result.gamma
                 << "cStar=" << m_result.cStar
                 << "Isp_vac=" << m_result.Isp_vac;
        return false;
    }
}

bool CEAEngine::parseOutputFileEnhanced()
{
    // 如果标准解析失败，尝试增强解析
    // 这里可以实现更复杂的解析逻辑
    return parseOutputFile(); // 暂时使用相同的解析
}

bool CEAEngine::isCEAAvailable() const
{
    QFile ceaFile(m_ceaExe);
    return ceaFile.exists();
}

void CEAEngine::cleanupTempFiles()
{
    // 清理临时文件
    QStringList tempFiles = {
        m_inputFile,
        m_outputFile,
        "thermo.lib",
        "trans.lib",
        "thermo.bkp",
        "fort.15",
        "fort.16",
        "thermo.out.bak"
    };

    for (const QString& tempFile : tempFiles) {
        QString filePath = "cea/" + tempFile;
        if (QFile::exists(filePath)) {
            QFile::remove(filePath);
        }
    }
}

bool CEAEngine::checkInputFile() const
{
    QFile file("cea/" + m_inputFile);
    if (!file.exists()) {
        return false;
    }

    if (file.size() < 10) {
        return false;
    }

    return true;
}

bool CEAEngine::checkOutputFile() const
{
    QFile file("cea/" + m_outputFile);
    if (!file.exists()) {
        return false;
    }

    if (file.size() < 100) {
        qWarning() << "输出文件过小:" << file.size() << "字节";
        return false;
    }

    return true;
}

QVector<CEAResult> CEAEngine::batchCalculate(const QString& oxid, const QString& fuel,
                                             double Pc_bar, const QVector<double>& OFs,
                                             double epsilon)
{
    QVector<CEAResult> results;

    if (OFs.isEmpty()) {
        return results;
    }

    int total = OFs.size();

    for (int i = 0; i < total; i++) {
        emit batchProgress(i + 1, total);

        if (runCEA(oxid, fuel, Pc_bar, OFs[i], epsilon)) {
            results.append(m_result);
        } else {
            // 如果计算失败，添加一个无效结果作为占位符
            results.append(CEAResult());
        }

        // 避免过快连续调用
        QThread::msleep(100);
    }

    return results;
}

double CEAEngine::getDensity(const QString& propellant)
{
    static QMap<QString, double> densities = {
        {"LOX", 1141.0},       // 液氧 (kg/m³)
        {"O2(L)", 1141.0},     // 液氧 (CEA名称)
        {"O2", 1.429},         // 气氧
        {"GOX", 1.429},        // 气氧
        {"NTO", 1440.0},       // 四氧化二氮
        {"N2O4", 1440.0},      // 四氧化二氮 (CEA名称)
        {"H2O2", 1450.0},      // 过氧化氢 (90%)
        {"RP-1", 810.0},       // 煤油
        {"LH2", 70.85},        // 液氢
        {"H2(L)", 70.85},      // 液氢 (CEA名称)
        {"H2", 0.0899},        // 气氢
        {"UDMH", 793.0},       // 偏二甲肼
        {"C2H5OH", 789.0},     // 乙醇
        {"C2H5OH(L)", 789.0},  // 乙醇 (CEA名称)
        {"CH4", 0.656},        // 甲烷
        {"MMH", 878.0}         // 一甲基肼
    };

    QString key = propellant.toUpper();
    if (densities.contains(key)) {
        return densities[key];
    }

    // 尝试匹配部分名称
    for (auto it = densities.begin(); it != densities.end(); ++it) {
        if (key.contains(it.key())) {
            return it.value();
        }
    }

    qWarning() << "未找到推进剂密度:" << propellant << "，使用默认值1000 kg/m³";
    return 1000.0;
}

double CEAEngine::getViscosity(const QString& propellant)
{
    static QMap<QString, double> viscosities = {
        {"LOX", 0.00019},      // 液氧 (Pa·s)
        {"O2(L)", 0.00019},    // 液氧
        {"RP-1", 0.0015},      // 煤油
        {"LH2", 0.000013},     // 液氢
        {"H2(L)", 0.000013},   // 液氢
        {"NTO", 0.0004},       // NTO
        {"N2O4", 0.0004},      // N2O4
        {"UDMH", 0.0008},      // UDMH
        {"H2O2", 0.0012},      // 过氧化氢
        {"C2H5OH", 0.0012},    // 乙醇
        {"GOX", 0.00002},      // 气氧
        {"O2", 0.00002},       // 气氧
        {"CH4", 0.000011}      // 甲烷
    };

    QString key = propellant.toUpper();
    if (viscosities.contains(key)) {
        return viscosities[key];
    }

    return 0.001;  // 默认值
}

double CEAEngine::getSurfaceTension(const QString& propellant)
{
    static QMap<QString, double> surfaceTensions = {
        {"LOX", 0.013},        // 液氧 (N/m)
        {"O2(L)", 0.013},      // 液氧
        {"RP-1", 0.025},       // 煤油
        {"LH2", 0.002},        // 液氢
        {"H2(L)", 0.002},      // 液氢
        {"NTO", 0.035},        // NTO
        {"N2O4", 0.035},       // N2O4
        {"UDMH", 0.030},       // UDMH
        {"H2O2", 0.075},       // 过氧化氢
        {"C2H5OH", 0.022}      // 乙醇
    };

    QString key = propellant.toUpper();
    if (surfaceTensions.contains(key)) {
        return surfaceTensions[key];
    }

    return 0.025;  // 默认值
}
