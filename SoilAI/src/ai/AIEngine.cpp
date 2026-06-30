#include "AIEngine.h"

#include <QFile>
#include <QTextStream>
#include <QJsonObject>
#include <cmath>
#include <algorithm>
#include <stdexcept>

AIEngine::AIEngine()
{
    // ---- Default coefficients (stand-ins for trained XGBoost weights) ----
    // Nutrient availability adjustment factors: how much of the raw sensor
    // ppm value is actually "available" to the plant, modulated by pH and
    // moisture, based on common agronomy heuristics (e.g. P availability
    // drops sharply outside pH 6-7.5; N leaches with high rainfall/humidity).
    m_coeffs["n_base_availability"]   = 0.78;
    m_coeffs["p_base_availability"]   = 0.55;
    m_coeffs["k_base_availability"]   = 0.85;

    m_coeffs["ph_penalty_weight"]     = 0.12;   // penalty per unit pH deviation from optimum
    m_coeffs["ph_optimum"]            = 6.5;

    m_coeffs["rainfall_leach_weight"] = 0.0009; // N/K loss per mm rainfall
    m_coeffs["humidity_leach_weight"] = 0.0015;

    m_coeffs["health_w_ph"]           = 0.25;
    m_coeffs["health_w_n"]            = 0.20;
    m_coeffs["health_w_p"]            = 0.20;
    m_coeffs["health_w_k"]            = 0.20;
    m_coeffs["health_w_moisture"]     = 0.15;

    m_coeffs["confidence_base"]       = 0.92;
}

void AIEngine::loadCoefficients(const QString &filePath)
{
    QFile file(filePath);
    if (!file.exists())
        return; // no override file present — keep defaults

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        throw std::runtime_error(("Failed to open coefficient file: " + filePath).toStdString());

    QTextStream in(&file);
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#'))
            continue;

        const int eqIdx = line.indexOf('=');
        if (eqIdx <= 0)
            continue;

        const QString key = line.left(eqIdx).trimmed();
        bool ok = false;
        const double value = line.mid(eqIdx + 1).trimmed().toDouble(&ok);
        if (!ok)
            throw std::runtime_error(("Malformed coefficient value for key: " + key).toStdString());

        m_coeffs[key] = value;
    }
}

double AIEngine::clamp01(double v) const
{
    return std::clamp(v, 0.0, 1.0);
}

double AIEngine::soilTypeFactor(const QString &soilType) const
{
    // Retention capacity multiplier — sandy soils retain less, clayey more.
    static const QMap<QString, double> factors = {
        {"Sandy", 0.85}, {"Loamy", 1.05}, {"Clayey", 1.10},
        {"Black", 1.08}, {"Red", 0.95}
    };
    return factors.value(soilType, 1.0);
}

double AIEngine::cropDemandFactor(const QString &cropType, const QString &nutrient) const
{
    // Relative nutrient demand intensity by crop (used only to flavor the
    // confidence score here; RecommendationEngine uses fuller crop tables).
    static const QMap<QString, QMap<QString, double>> demand = {
        {"Rice",      {{"N", 1.15}, {"P", 0.95}, {"K", 1.00}}},
        {"Wheat",     {{"N", 1.05}, {"P", 1.00}, {"K", 0.95}}},
        {"Maize",     {{"N", 1.20}, {"P", 1.05}, {"K", 1.05}}},
        {"Cotton",    {{"N", 1.00}, {"P", 1.00}, {"K", 1.15}}},
        {"Sugarcane", {{"N", 1.25}, {"P", 1.00}, {"K", 1.10}}},
    };
    return demand.value(cropType).value(nutrient, 1.0);
}

PredictionResult AIEngine::predict(const PredictionInput &in) const
{
    PredictionResult result;

    // ---- pH penalty: nutrients are less available the further pH is from optimum ----
    const double phOptimum = m_coeffs.value("ph_optimum");
    const double phDeviation = std::abs(in.ph - phOptimum);
    const double phPenalty = 1.0 - clamp01(phDeviation * m_coeffs.value("ph_penalty_weight"));

    // ---- Leaching loss from rainfall + humidity (affects mobile nutrients N, K) ----
    const double leachLoss = clamp01(
        in.rainfallMm * m_coeffs.value("rainfall_leach_weight") +
        in.humidity * m_coeffs.value("humidity_leach_weight") / 100.0
    );

    const double soilFactor = soilTypeFactor(in.soilType);

    // ---- Predicted plant-available nutrients (XGBoost stand-in equations) ----
    result.predictedNitrogen = in.nitrogen
        * m_coeffs.value("n_base_availability")
        * phPenalty
        * soilFactor
        * (1.0 - leachLoss * 0.6);   // N is highly mobile/leachable

    result.predictedPhosphorus = in.phosphorus
        * m_coeffs.value("p_base_availability")
        * phPenalty                  // P availability is *very* pH-sensitive
        * soilFactor;

    result.predictedPotassium = in.potassium
        * m_coeffs.value("k_base_availability")
        * phPenalty
        * soilFactor
        * (1.0 - leachLoss * 0.3);   // K is moderately mobile

    result.predictedNitrogen   = std::max(0.0, result.predictedNitrogen);
    result.predictedPhosphorus = std::max(0.0, result.predictedPhosphorus);
    result.predictedPotassium  = std::max(0.0, result.predictedPotassium);

    // ---- Soil health score (0-100): weighted composite of normalized factors ----
    const double phScore = 100.0 * (1.0 - clamp01(phDeviation / 3.0));
    const double nScore = 100.0 * clamp01(in.nitrogen / 200.0);
    const double pScore = 100.0 * clamp01(in.phosphorus / 100.0);
    const double kScore = 100.0 * clamp01(in.potassium / 250.0);
    const double moistureScore = 100.0 * (1.0 - std::abs(in.humidity - 55.0) / 55.0);

    result.soilHealthScore =
        phScore * m_coeffs.value("health_w_ph") +
        nScore * m_coeffs.value("health_w_n") +
        pScore * m_coeffs.value("health_w_p") +
        kScore * m_coeffs.value("health_w_k") +
        std::clamp(moistureScore, 0.0, 100.0) * m_coeffs.value("health_w_moisture");

    result.soilHealthScore = std::clamp(result.soilHealthScore, 0.0, 100.0);

    // ---- Confidence: degrades for extreme/out-of-range inputs (model is less
    // trustworthy far from its "comfort zone", same idea as prediction-interval
    // widening in a real model) ----
    double confidence = m_coeffs.value("confidence_base");
    if (in.ph < 5.0 || in.ph > 8.5) confidence -= 0.10;
    if (in.rainfallMm > 200) confidence -= 0.08;
    if (in.temperature < 10 || in.temperature > 42) confidence -= 0.07;
    confidence -= phDeviation * 0.01; // gentle continuous penalty too

    // Slight boost when crop demand factor data exists for this crop (i.e.
    // engine "recognizes" the crop), mimicking better-fit confidence.
    if (cropDemandFactor(in.cropType, "N") != 1.0)
        confidence += 0.03;

    result.confidence = clamp01(confidence);

    return result;
}

QJsonObject PredictionResult::toJson() const
{
    QJsonObject obj;
    obj["predicted_nitrogen"] = predictedNitrogen;
    obj["predicted_phosphorus"] = predictedPhosphorus;
    obj["predicted_potassium"] = predictedPotassium;
    obj["soil_health_score"] = soilHealthScore;
    obj["soil_health_label"] = soilHealthLabel();
    obj["confidence"] = confidence;
    return obj;
}
