#pragma once
#include <QObject>
#include <QTimer>
#include <QThread>
#include <memory>

#include "Sensor.h"

// Aggregated snapshot of all six channels at one point in time.
// Used both by the simulator and (later) by SerialPortManager so the
// Dashboard only needs to know about one signal signature.
struct SensorSnapshot
{
    double ph = 0, nitrogen = 0, phosphorus = 0, potassium = 0;
    double temperature = 0, humidity = 0;
};

// ============================================================
// SensorSimulator
// Runs on its own QThread and emits a new SensorSnapshot once per
// second. Owns one instance of each Sensor subclass (composition).
// When real hardware is wired up, SerialPortManager emits the same
// snapshotReady(SensorSnapshot) signal, so Dashboard doesn't change.
// ============================================================
class SensorSimulator : public QObject
{
    Q_OBJECT
public:
    explicit SensorSimulator(QObject *parent = nullptr);

public slots:
    void start();   // called once the worker thread has started
    void stop();
    void setIntervalMs(int ms);

signals:
    void snapshotReady(SensorSnapshot snapshot);

private slots:
    void tick();

private:
    std::unique_ptr<PHSensor> m_ph;
    std::unique_ptr<NitrogenSensor> m_n;
    std::unique_ptr<PhosphorusSensor> m_p;
    std::unique_ptr<PotassiumSensor> m_k;
    std::unique_ptr<TemperatureSensor> m_t;
    std::unique_ptr<HumiditySensor> m_h;

    QTimer *m_timer = nullptr;
    int m_intervalMs = 1000;
};

// ============================================================
// SensorThreadController
// Convenience RAII wrapper that owns the QThread + SensorSimulator
// pair so MainWindow/Dashboard doesn't manage thread lifetime by hand.
// ============================================================
class SensorThreadController : public QObject
{
    Q_OBJECT
public:
    explicit SensorThreadController(QObject *parent = nullptr);
    ~SensorThreadController() override;

    void startReading();
    void stopReading();
    bool isRunning() const { return m_running; }

signals:
    void snapshotReady(SensorSnapshot snapshot);

private:
    QThread m_thread;
    SensorSimulator *m_simulator = nullptr;
    bool m_running = false;
};
