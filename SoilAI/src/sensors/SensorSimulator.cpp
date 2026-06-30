#include "SensorSimulator.h"

SensorSimulator::SensorSimulator(QObject *parent)
    : QObject(parent),
      m_ph(std::make_unique<PHSensor>()),
      m_n(std::make_unique<NitrogenSensor>()),
      m_p(std::make_unique<PhosphorusSensor>()),
      m_k(std::make_unique<PotassiumSensor>()),
      m_t(std::make_unique<TemperatureSensor>()),
      m_h(std::make_unique<HumiditySensor>())
{
}

void SensorSimulator::start()
{
    if (!m_timer) {
        m_timer = new QTimer(this);   // parented -> lives on this object's thread
        connect(m_timer, &QTimer::timeout, this, &SensorSimulator::tick);
    }
    m_timer->start(m_intervalMs);
}

void SensorSimulator::stop()
{
    if (m_timer)
        m_timer->stop();
}

void SensorSimulator::setIntervalMs(int ms)
{
    m_intervalMs = ms;
    if (m_timer && m_timer->isActive())
        m_timer->setInterval(ms);
}

void SensorSimulator::tick()
{
    SensorSnapshot snap;
    snap.ph = m_ph->readValue();
    snap.nitrogen = m_n->readValue();
    snap.phosphorus = m_p->readValue();
    snap.potassium = m_k->readValue();
    snap.temperature = m_t->readValue();
    snap.humidity = m_h->readValue();
    emit snapshotReady(snap);
}

// ---------------- SensorThreadController ----------------

SensorThreadController::SensorThreadController(QObject *parent)
    : QObject(parent)
{
    m_simulator = new SensorSimulator(); // no parent: will be moved to m_thread
    m_simulator->moveToThread(&m_thread);

    connect(&m_thread, &QThread::finished, m_simulator, &QObject::deleteLater);
    connect(m_simulator, &SensorSimulator::snapshotReady,
            this, &SensorThreadController::snapshotReady);

    m_thread.start();
}

SensorThreadController::~SensorThreadController()
{
    m_thread.quit();
    m_thread.wait();
}

void SensorThreadController::startReading()
{
    QMetaObject::invokeMethod(m_simulator, "start", Qt::QueuedConnection);
    m_running = true;
}

void SensorThreadController::stopReading()
{
    QMetaObject::invokeMethod(m_simulator, "stop", Qt::QueuedConnection);
    m_running = false;
}
