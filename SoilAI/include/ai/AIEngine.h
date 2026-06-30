#pragma once
#include <QString>
#include <QMap>
#include <QJsonObject>
#include <vector>
#include <array>

// ============================================================
// PredictionInput
// All contextual + sensor inputs the model uses, per the project
// brief: pH, N, P, K, Temperature, Humidity, Rainfall, Soil Type,
// Crop Type.
// ============================================================
struct PredictionInput
{
    double ph = 7.0;
    double nitrogen = 0;
    double phosphorus = 0;
    double potassium = 0;
    double temperature = 25;
    double humidity = 50;
    double rainfallMm = 0;
    QString soilType = "Loamy";     // Sandy | Loamy | Clayey | Black | Red
    QString cropType = "Wheat";     // Wheat | Rice | Maize | Cotton | Sugarcane ...
};

// ============================================================
// PredictionResult
// Output of the AI engine: predicted available nutrients (the model's
// estimate of what's actually plant-available, vs. raw sensor ppm),
// an overall soil health score (0-100), and a confidence score (0-1)
// reflecting how well-conditioned the inputs were for the model.
// ============================================================
struct PredictionResult
{
    double predictedNitrogen = 0;
    double predictedPhosphorus = 0;
    double predictedPotassium = 0;
    double soilHealthScore = 0;     // 0-100
    double confidence = 0;          // 0-1

    QString soilHealthLabel() const
    {
        if (soilHealthScore >= 80) return "Excellent";
        if (soilHealthScore >= 60) return "Good";
        if (soilHealthScore >= 40) return "Moderate";
        if (soilHealthScore >= 20) return "Poor";
        return "Critical";
    }

    // Serializes to JSON for storage in Predictions.predicted_nutrients_json.
    QJsonObject toJson() const;
};

// ============================================================
// AIEngine
// Simulates an XGBoost regression model using weighted linear/
// nonlinear equations calibrated by domain heuristics (soil science
// rules of thumb), so the app behaves sensibly without a trained
// model file. Coefficients can be overridden at runtime by loading a
// simple key=value text file (loadCoefficients), which is where a
// real trained model's exported weights would be plugged in later.
//
// This class has NO Qt Widgets dependency — pure computation — so it
// is trivially unit-testable and reusable from CLI tools or tests.
// ============================================================
class AIEngine
{
public:
    AIEngine();

    // Loads/overrides coefficients from a simple "name=value" text file.
    // Unknown keys are ignored; missing file silently keeps defaults
    // (throws only on malformed numeric values, to fail loudly on typos).
    void loadCoefficients(const QString &filePath);

    PredictionResult predict(const PredictionInput &input) const;

private:
    double soilTypeFactor(const QString &soilType) const;
    double cropDemandFactor(const QString &cropType, const QString &nutrient) const;
    double clamp01(double v) const;

    // Tunable model weights (defaults chosen from general agronomy
    // heuristics; replace via loadCoefficients() with trained values).
    QMap<QString, double> m_coeffs;
};
