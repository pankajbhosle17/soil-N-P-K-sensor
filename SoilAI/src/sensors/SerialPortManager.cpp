#include "SerialPortManager.h"

#include <QSerialPort>
#include <QSerialPortInfo>
#include <QDebug>

SerialPortManager::SerialPortManager(QObject *parent)
    : QObject(parent), m_port(std::make_unique<QSerialPort>())
{
    connect(m_port.get(), &QSerialPort::readyRead, this, &SerialPortManager::onReadyRead);
    connect(m_port.get(), &QSerialPort::errorOccurred, this, [this](QSerialPort::SerialPortError e) {
        onErrorOccurred(static_cast<int>(e));
    });

    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setSingleShot(false);
    connect(m_reconnectTimer, &QTimer::timeout, this, &SerialPortManager::attemptReconnect);

    m_dataWatchdog = new QTimer(this);
    m_dataWatchdog->setSingleShot(false);
    connect(m_dataWatchdog, &QTimer::timeout, this, &SerialPortManager::checkDataTimeout);
}

SerialPortManager::~SerialPortManager()
{
    disconnectPort();
}

QStringList SerialPortManager::availablePorts()
{
    QStringList names;
    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts())
        names << info.portName();
    return names;
}

void SerialPortManager::connectToPort(const QString &portName, qint32 baudRate)
{
    m_portName = portName;
    m_baudRate = baudRate;

    if (m_port->isOpen())
        m_port->close();

    m_port->setPortName(portName);
    m_port->setBaudRate(baudRate);
    m_port->setDataBits(QSerialPort::Data8);
    m_port->setParity(QSerialPort::NoParity);
    m_port->setStopBits(QSerialPort::OneStop);
    m_port->setFlowControl(QSerialPort::NoFlowControl);

    if (m_port->open(QIODevice::ReadWrite)) {
        m_buffer.clear();
        setConnected(true);
        m_reconnectTimer->stop();
        m_dataWatchdog->start(m_watchdogTimeoutMs);
    } else {
        setConnected(false);
        emit portError(QString("Failed to open %1: %2").arg(portName, m_port->errorString()));
        if (m_autoReconnect)
            m_reconnectTimer->start(m_reconnectIntervalMs);
    }
}

void SerialPortManager::disconnectPort()
{
    m_reconnectTimer->stop();
    m_dataWatchdog->stop();
    if (m_port->isOpen())
        m_port->close();
    setConnected(false);
}

void SerialPortManager::setConnected(bool connected)
{
    if (m_connected == connected)
        return;
    m_connected = connected;
    emit connectionStateChanged(connected);
}

void SerialPortManager::onReadyRead()
{
    m_buffer += QString::fromUtf8(m_port->readAll());

    // Process every complete line terminated by '\n' (Arduino's println()).
    int newlineIdx;
    while ((newlineIdx = m_buffer.indexOf('\n')) != -1) {
        QString line = m_buffer.left(newlineIdx).trimmed();
        m_buffer.remove(0, newlineIdx + 1);

        if (line.isEmpty())
            continue;

        emit rawLineReceived(line);

        SensorSnapshot snap;
        if (parseLine(line, snap)) {
            emit snapshotReady(snap);
            m_dataWatchdog->start(m_watchdogTimeoutMs); // reset watchdog on valid data
        } else {
            emit parseError(line, "Line did not match expected PH:..,N:..,P:..,K:..,T:..,H:.. format");
        }
    }
}

bool SerialPortManager::parseLine(const QString &line, SensorSnapshot &outSnapshot) const
{
    // Expected format: PH:6.8,N:120,P:40,K:160,T:31,H:65
    const QStringList parts = line.split(',', Qt::SkipEmptyParts);
    if (parts.isEmpty())
        return false;

    bool sawAnyField = false;
    SensorSnapshot snap{};

    for (const QString &part : parts) {
        const int colonIdx = part.indexOf(':');
        if (colonIdx <= 0)
            continue;

        const QString key = part.left(colonIdx).trimmed().toUpper();
        bool ok = false;
        const double value = part.mid(colonIdx + 1).trimmed().toDouble(&ok);
        if (!ok)
            continue;

        if (key == "PH")      { snap.ph = value;          sawAnyField = true; }
        else if (key == "N")  { snap.nitrogen = value;     sawAnyField = true; }
        else if (key == "P")  { snap.phosphorus = value;   sawAnyField = true; }
        else if (key == "K")  { snap.potassium = value;    sawAnyField = true; }
        else if (key == "T")  { snap.temperature = value;  sawAnyField = true; }
        else if (key == "H")  { snap.humidity = value;     sawAnyField = true; }
    }

    if (!sawAnyField)
        return false;

    outSnapshot = snap;
    return true;
}

void SerialPortManager::onErrorOccurred(int errorCode)
{
    // ResourceError / DeviceNotFoundError / TimeoutError etc. all indicate the
    // physical connection was lost (e.g. Arduino unplugged).
    if (errorCode == 0) // QSerialPort::NoError
        return;

    emit portError(QString("Serial error code %1 on %2: %3")
                        .arg(errorCode).arg(m_portName, m_port->errorString()));

    if (m_port->isOpen())
        m_port->close();

    setConnected(false);
    m_dataWatchdog->stop();

    if (m_autoReconnect)
        m_reconnectTimer->start(m_reconnectIntervalMs);
}

void SerialPortManager::attemptReconnect()
{
    if (m_connected || m_portName.isEmpty())
        return;
    connectToPort(m_portName, m_baudRate);
}

void SerialPortManager::checkDataTimeout()
{
    // No valid sensor line arrived within the watchdog window while the
    // port is still "open" — likely a silently dead connection (cable
    // unplugged without an OS-level error). Force a reconnect cycle.
    if (!m_connected)
        return;

    emit portError(QString("No data received from %1 for %2 ms — assuming connection lost.")
                        .arg(m_portName).arg(m_watchdogTimeoutMs));

    if (m_port->isOpen())
        m_port->close();
    setConnected(false);
    m_dataWatchdog->stop();

    if (m_autoReconnect)
        m_reconnectTimer->start(m_reconnectIntervalMs);
}
