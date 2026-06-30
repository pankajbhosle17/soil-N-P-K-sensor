#pragma once
#include <QObject>
#include <QString>
#include <QTimer>
#include <memory>

#include "SensorSimulator.h"   // reuse SensorSnapshot so Dashboard needs zero changes

class QSerialPort;

// ============================================================
// SerialPortManager
// Reads live data from an Arduino over QSerialPort. Expects lines in
// the format:
//     PH:6.8,N:120,P:40,K:160,T:31,H:65\n
//
// Emits the SAME SensorSnapshot signal that SensorSimulator emits, so
// Dashboard / MainWindow can swap between simulated and real hardware
// without any change to their code (Liskov-style substitutability).
//
// Handles:
//  - Connecting to a named COM port at a given baud rate
//  - Buffering partial serial reads until a full line (\n) arrives
//  - Parsing "KEY:VALUE,KEY:VALUE,..." into a SensorSnapshot
//  - Detecting connection loss (port error / no data within timeout)
//  - Auto-reconnect with a backoff timer
// ============================================================
class SerialPortManager : public QObject
{
    Q_OBJECT
public:
    explicit SerialPortManager(QObject *parent = nullptr);
    ~SerialPortManager() override;

    // Lists COM/tty ports currently available on the system (for Settings dialog).
    static QStringList availablePorts();

    bool isConnected() const { return m_connected; }
    QString portName() const { return m_portName; }

public slots:
    void connectToPort(const QString &portName, qint32 baudRate = 9600);
    void disconnectPort();
    void setAutoReconnect(bool enabled) { m_autoReconnect = enabled; }
    void setReconnectIntervalMs(int ms) { m_reconnectIntervalMs = ms; }

signals:
    void snapshotReady(SensorSnapshot snapshot);
    void connectionStateChanged(bool connected);
    void rawLineReceived(QString line);
    void parseError(QString rawLine, QString reason);
    void portError(QString message);

private slots:
    void onReadyRead();
    void onErrorOccurred(int errorCode);
    void attemptReconnect();
    void checkDataTimeout();

private:
    bool parseLine(const QString &line, SensorSnapshot &outSnapshot) const;
    void setConnected(bool connected);

    std::unique_ptr<QSerialPort> m_port;
    QString m_buffer;
    QString m_portName;
    qint32 m_baudRate = 9600;

    bool m_connected = false;
    bool m_autoReconnect = true;
    int m_reconnectIntervalMs = 3000;

    QTimer *m_reconnectTimer = nullptr;
    QTimer *m_dataWatchdog = nullptr;   // fires if no valid line arrives for N ms while "connected"
    int m_watchdogTimeoutMs = 5000;
};
