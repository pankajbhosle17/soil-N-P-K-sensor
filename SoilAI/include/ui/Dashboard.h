#pragma once
#include <QWidget>
#include <QString>
#include <memory>

#include "SensorSimulator.h"
#include "SerialPortManager.h"

class QLabel;
class QPushButton;
class QComboBox;
class GaugeWidget;
class QTimer;

// ============================================================
// Dashboard
// The main operational screen: live sensor gauges, device status,
// date/time, location/weather strip, and the primary action buttons
// (Start/Stop/Save/Predict/Recommend/Export/Settings/Report).
//
// Dashboard does NOT know whether data comes from SensorSimulator or
// SerialPortManager — it only consumes SensorSnapshot via the slot
// onSnapshot(). This keeps it decoupled (Strategy-like substitution).
// ============================================================
class Dashboard : public QWidget
{
    Q_OBJECT
public:
    explicit Dashboard(QString currentUser, QWidget *parent = nullptr);
    ~Dashboard() override;

    SensorSnapshot latestSnapshot() const { return m_latest; }

signals:
    // Re-emitted so MainWindow can route them to other modules
    // (Report generator, Settings dialog, AI engine, etc.)
    void requestGenerateReport();
    void requestPredictNutrients();
    void requestRecommendFertilizer();
    void requestExportPdf();
    void requestOpenSettings();
    void requestSaveData(SensorSnapshot snapshot);

public slots:
    void onSnapshot(SensorSnapshot snapshot);

private slots:
    void onStartClicked();
    void onStopClicked();
    void onSaveClicked();
    void updateClock();
    void onSourceChanged(int index);
    void onSerialConnectionStateChanged(bool connected);
    void onSerialPortError(QString message);

private:
    void buildUi();
    void applyTheme();
    QWidget *buildSensorGrid();
    QWidget *buildHeaderBar();
    QWidget *buildActionBar();

    QString m_currentUser;
    SensorSnapshot m_latest;
    bool m_reading = false;

    std::unique_ptr<SensorThreadController> m_sensorController;
    std::unique_ptr<SerialPortManager> m_serialManager;
    bool m_useSerial = false;   // false = Simulated, true = Live Arduino

    // Header
    QLabel *m_dateLabel = nullptr;
    QLabel *m_timeLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_locationLabel = nullptr;
    QLabel *m_weatherLabel = nullptr;
    QComboBox *m_sourceCombo = nullptr;
    QComboBox *m_portCombo = nullptr;
    QTimer *m_clockTimer = nullptr;

    // Gauges
    GaugeWidget *m_gaugePh = nullptr;
    GaugeWidget *m_gaugeN = nullptr;
    GaugeWidget *m_gaugeP = nullptr;
    GaugeWidget *m_gaugeK = nullptr;
    GaugeWidget *m_gaugeTemp = nullptr;
    GaugeWidget *m_gaugeHumidity = nullptr;

    // Actions
    QPushButton *m_startBtn = nullptr;
    QPushButton *m_stopBtn = nullptr;
    QPushButton *m_saveBtn = nullptr;
    QPushButton *m_reportBtn = nullptr;
    QPushButton *m_predictBtn = nullptr;
    QPushButton *m_recommendBtn = nullptr;
    QPushButton *m_exportBtn = nullptr;
    QPushButton *m_settingsBtn = nullptr;
};
