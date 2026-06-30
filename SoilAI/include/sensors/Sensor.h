#pragma once
#include <QString>
#include <QRandomGenerator>
#include <algorithm>

// ============================================================
// Sensor
// Abstract base class for all simulated/real soil sensors.
// Demonstrates inheritance + polymorphism: each concrete sensor
// only needs to define its valid range and unit; readValue() handles
// realistic randomized drift simulation for all of them.
// ============================================================
class Sensor
{
public:
    Sensor(QString name, QString unit, double minVal, double maxVal)
        : m_name(std::move(name)), m_unit(std::move(unit)),
          m_min(minVal), m_max(maxVal), m_lastValue((minVal + maxVal) / 2.0)
    {}

    virtual ~Sensor() = default;

    // Generates the next simulated reading. Uses a small random walk from the
    // last value (rather than pure uniform noise) so trends look realistic.
    virtual double readValue()
    {
        const double range = m_max - m_min;
        const double step = (QRandomGenerator::global()->generateDouble() - 0.5) * range * 0.08;
        double next = m_lastValue + step;
        next = std::clamp(next, m_min, m_max);
        m_lastValue = next;
        return next;
    }

    QString name() const { return m_name; }
    QString unit() const { return m_unit; }
    double minValue() const { return m_min; }
    double maxValue() const { return m_max; }
    double lastValue() const { return m_lastValue; }

    // Allows SerialPortManager to inject a real reading instead of simulating.
    void overrideValue(double v) { m_lastValue = std::clamp(v, m_min, m_max); }

protected:
    QString m_name;
    QString m_unit;
    double m_min;
    double m_max;
    double m_lastValue;
};

// ---------------- Concrete sensors ----------------

class PHSensor : public Sensor
{
public:
    PHSensor() : Sensor("pH", "", 4.5, 9.0) {}
};

class NitrogenSensor : public Sensor
{
public:
    NitrogenSensor() : Sensor("Nitrogen", "ppm", 0, 300) {}
};

class PhosphorusSensor : public Sensor
{
public:
    PhosphorusSensor() : Sensor("Phosphorus", "ppm", 0, 150) {}
};

class PotassiumSensor : public Sensor
{
public:
    PotassiumSensor() : Sensor("Potassium", "ppm", 0, 400) {}
};

class TemperatureSensor : public Sensor
{
public:
    TemperatureSensor() : Sensor("Temperature", "°C", 15, 45) {}
};

class HumiditySensor : public Sensor
{
public:
    HumiditySensor() : Sensor("Humidity", "%", 20, 100) {}
};
