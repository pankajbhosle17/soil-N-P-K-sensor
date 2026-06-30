#include "Dashboard.h"
#include "GaugeWidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QFrame>
#include <QTimer>
#include <QDateTime>
#include <QMessageBox>
#include <QDebug>

Dashboard::Dashboard(QString currentUser, QWidget *parent)
    : QWidget(parent), m_currentUser(std::move(currentUser))
{
    m_sensorController = std::make_unique<SensorThreadController>(this);
    connect(m_sensorController.get(), &SensorThreadController::snapshotReady,
            this, &Dashboard::onSnapshot);

    m_serialManager = std::make_unique<SerialPortManager>(this);
    connect(m_serialManager.get(), &SerialPortManager::snapshotReady,
            this, &Dashboard::onSnapshot);
    connect(m_serialManager.get(), &SerialPortManager::connectionStateChanged,
            this, &Dashboard::onSerialConnectionStateChanged);
    connect(m_serialManager.get(), &SerialPortManager::portError,
            this, &Dashboard::onSerialPortError);

    buildUi();
    applyTheme();

    m_clockTimer = new QTimer(this);
    connect(m_clockTimer, &QTimer::timeout, this, &Dashboard::updateClock);
    m_clockTimer->start(1000);
    updateClock();
}

Dashboard::~Dashboard() = default;

void Dashboard::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 16, 20, 16);
    root->setSpacing(16);

    root->addWidget(buildHeaderBar());
    root->addWidget(buildSensorGrid());
    root->addWidget(buildActionBar());
    root->addStretch();
}

QWidget *Dashboard::buildHeaderBar()
{
    auto *bar = new QFrame(this);
    bar->setObjectName("headerBar");
    auto *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(18, 12, 18, 12);

    auto *welcome = new QLabel(QString("👤 %1").arg(m_currentUser), bar);
    welcome->setObjectName("welcomeLabel");

    m_dateLabel = new QLabel(bar);
    m_timeLabel = new QLabel(bar);
    m_dateLabel->setObjectName("clockLabel");
    m_timeLabel->setObjectName("clockLabel");

    m_statusLabel = new QLabel("● DEVICE IDLE", bar);
    m_statusLabel->setObjectName("statusIdle");

    m_sourceCombo = new QComboBox(bar);
    m_sourceCombo->addItem("Simulated");
    m_sourceCombo->addItem("Live Arduino (Serial)");
    m_sourceCombo->setObjectName("sourceCombo");

    m_portCombo = new QComboBox(bar);
    m_portCombo->addItems(SerialPortManager::availablePorts());
    m_portCombo->setObjectName("portCombo");
    m_portCombo->setVisible(false);
    m_portCombo->setMinimumWidth(90);

    connect(m_sourceCombo, &QComboBox::currentIndexChanged, this, &Dashboard::onSourceChanged);

    m_locationLabel = new QLabel("📍 Bengaluru, KA", bar);
    m_weatherLabel = new QLabel("⛅ -- °C", bar);
    m_locationLabel->setObjectName("infoLabel");
    m_weatherLabel->setObjectName("infoLabel");

    layout->addWidget(welcome);
    layout->addStretch();
    layout->addWidget(m_sourceCombo);
    layout->addWidget(m_portCombo);
    layout->addSpacing(8);
    layout->addWidget(m_locationLabel);
    layout->addSpacing(16);
    layout->addWidget(m_weatherLabel);
    layout->addSpacing(16);
    layout->addWidget(m_dateLabel);
    layout->addWidget(m_timeLabel);
    layout->addSpacing(16);
    layout->addWidget(m_statusLabel);

    return bar;
}

QWidget *Dashboard::buildSensorGrid()
{
    auto *card = new QFrame(this);
    card->setObjectName("sensorCard");
    auto *grid = new QGridLayout(card);
    grid->setContentsMargins(20, 20, 20, 20);
    grid->setSpacing(18);

    m_gaugePh        = new GaugeWidget("pH", "", 4.5, 9.0, QColor("#5fe3a1"), card);
    m_gaugeN         = new GaugeWidget("Nitrogen", "ppm", 0, 300, QColor("#4fb6ff"), card);
    m_gaugeP         = new GaugeWidget("Phosphorus", "ppm", 0, 150, QColor("#ffb74f"), card);
    m_gaugeK         = new GaugeWidget("Potassium", "ppm", 0, 400, QColor("#d291ff"), card);
    m_gaugeTemp      = new GaugeWidget("Temperature", "°C", 15, 45, QColor("#ff6b6b"), card);
    m_gaugeHumidity  = new GaugeWidget("Humidity", "%", 20, 100, QColor("#4fd1ff"), card);

    grid->addWidget(m_gaugePh,       0, 0);
    grid->addWidget(m_gaugeN,        0, 1);
    grid->addWidget(m_gaugeP,        0, 2);
    grid->addWidget(m_gaugeK,        0, 3);
    grid->addWidget(m_gaugeTemp,     0, 4);
    grid->addWidget(m_gaugeHumidity, 0, 5);

    return card;
}

QWidget *Dashboard::buildActionBar()
{
    auto *bar = new QFrame(this);
    bar->setObjectName("actionBar");
    auto *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(18, 14, 18, 14);
    layout->setSpacing(10);

    m_startBtn = new QPushButton("▶ Start Reading", bar);
    m_stopBtn = new QPushButton("■ Stop Reading", bar);
    m_saveBtn = new QPushButton("💾 Save Data", bar);
    m_reportBtn = new QPushButton("📄 Generate Report", bar);
    m_predictBtn = new QPushButton("🧠 Predict Nutrients", bar);
    m_recommendBtn = new QPushButton("🌿 Recommend Fertilizer", bar);
    m_exportBtn = new QPushButton("⬇ Export PDF", bar);
    m_settingsBtn = new QPushButton("⚙ Settings", bar);

    m_startBtn->setObjectName("primaryBtn");
    m_stopBtn->setObjectName("dangerBtn");
    m_stopBtn->setEnabled(false);

    for (auto *btn : {m_saveBtn, m_reportBtn, m_predictBtn, m_recommendBtn, m_exportBtn, m_settingsBtn})
        btn->setObjectName("secondaryBtn");

    layout->addWidget(m_startBtn);
    layout->addWidget(m_stopBtn);
    layout->addWidget(m_saveBtn);
    layout->addStretch();
    layout->addWidget(m_predictBtn);
    layout->addWidget(m_recommendBtn);
    layout->addWidget(m_reportBtn);
    layout->addWidget(m_exportBtn);
    layout->addWidget(m_settingsBtn);

    connect(m_startBtn, &QPushButton::clicked, this, &Dashboard::onStartClicked);
    connect(m_stopBtn, &QPushButton::clicked, this, &Dashboard::onStopClicked);
    connect(m_saveBtn, &QPushButton::clicked, this, &Dashboard::onSaveClicked);
    connect(m_reportBtn, &QPushButton::clicked, this, &Dashboard::requestGenerateReport);
    connect(m_predictBtn, &QPushButton::clicked, this, &Dashboard::requestPredictNutrients);
    connect(m_recommendBtn, &QPushButton::clicked, this, &Dashboard::requestRecommendFertilizer);
    connect(m_exportBtn, &QPushButton::clicked, this, &Dashboard::requestExportPdf);
    connect(m_settingsBtn, &QPushButton::clicked, this, &Dashboard::requestOpenSettings);

    return bar;
}

void Dashboard::onStartClicked()
{
    if (m_useSerial) {
        const QString port = m_portCombo->currentText();
        if (port.isEmpty()) {
            m_statusLabel->setText("● NO COM PORT SELECTED");
            return;
        }
        m_serialManager->setAutoReconnect(true);
        m_serialManager->connectToPort(port, 9600);
    } else {
        m_sensorController->startReading();
        m_statusLabel->setText("● DEVICE READING");
        m_statusLabel->setObjectName("statusActive");
        m_statusLabel->style()->unpolish(m_statusLabel);
        m_statusLabel->style()->polish(m_statusLabel);
    }
    m_reading = true;
    m_startBtn->setEnabled(false);
    m_stopBtn->setEnabled(true);
}

void Dashboard::onStopClicked()
{
    if (m_useSerial) {
        m_serialManager->setAutoReconnect(false);
        m_serialManager->disconnectPort();
    } else {
        m_sensorController->stopReading();
    }
    m_reading = false;
    m_startBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);
    m_statusLabel->setText("● DEVICE IDLE");
    m_statusLabel->setObjectName("statusIdle");
    m_statusLabel->style()->unpolish(m_statusLabel);
    m_statusLabel->style()->polish(m_statusLabel);
}

void Dashboard::onSourceChanged(int index)
{
    m_useSerial = (index == 1); // 0 = Simulated, 1 = Live Arduino
    m_portCombo->setVisible(m_useSerial);

    // Switching source while reading: stop whichever was active first.
    if (m_reading)
        onStopClicked();
}

void Dashboard::onSerialConnectionStateChanged(bool connected)
{
    if (!m_useSerial)
        return;
    m_statusLabel->setText(connected ? "● ARDUINO CONNECTED" : "● ARDUINO DISCONNECTED — reconnecting…");
    m_statusLabel->setObjectName(connected ? "statusActive" : "statusIdle");
    m_statusLabel->style()->unpolish(m_statusLabel);
    m_statusLabel->style()->polish(m_statusLabel);
}

void Dashboard::onSerialPortError(QString message)
{
    if (m_useSerial)
        m_statusLabel->setText("● SERIAL ERROR — retrying…");
    qWarning().noquote() << "[SerialPortManager]" << message;
}

void Dashboard::onSaveClicked()
{
    emit requestSaveData(m_latest);
    QMessageBox::information(this, "Saved", "Current sensor reading queued for save.");
}

void Dashboard::onSnapshot(SensorSnapshot snapshot)
{
    m_latest = snapshot;
    m_gaugePh->setValue(snapshot.ph);
    m_gaugeN->setValue(snapshot.nitrogen);
    m_gaugeP->setValue(snapshot.phosphorus);
    m_gaugeK->setValue(snapshot.potassium);
    m_gaugeTemp->setValue(snapshot.temperature);
    m_gaugeHumidity->setValue(snapshot.humidity);
}

void Dashboard::updateClock()
{
    const QDateTime now = QDateTime::currentDateTime();
    m_dateLabel->setText(now.toString("dd MMM yyyy"));
    m_timeLabel->setText(now.toString("hh:mm:ss"));
}

void Dashboard::applyTheme()
{
    setStyleSheet(R"(
        Dashboard { background-color: #0f1720; }
        #headerBar, #sensorCard, #actionBar {
            background-color: #16212c;
            border-radius: 12px;
            border: 1px solid #233140;
        }
        #welcomeLabel { color: #e8f0f5; font-size: 14px; font-weight: 600; }
        #clockLabel { color: #8aa0b2; font-size: 12px; }
        #infoLabel { color: #8aa0b2; font-size: 12px; }
        #statusIdle { color: #8aa0b2; font-weight: 700; font-size: 11px; }
        #statusActive { color: #5fe3a1; font-weight: 700; font-size: 11px; }
        QPushButton#primaryBtn {
            background-color: #5fe3a1; color: #0f1720; font-weight: 700;
            border-radius: 8px; padding: 8px 14px;
        }
        QPushButton#primaryBtn:disabled { background-color: #2a3a40; color: #5a6a72; }
        QPushButton#dangerBtn {
            background-color: #ff6b6b; color: #0f1720; font-weight: 700;
            border-radius: 8px; padding: 8px 14px;
        }
        QPushButton#dangerBtn:disabled { background-color: #2a3a40; color: #5a6a72; }
        QPushButton#secondaryBtn {
            background-color: transparent; color: #c7d3dd;
            border: 1px solid #2a3a4a; border-radius: 8px; padding: 8px 12px;
        }
        QPushButton#secondaryBtn:hover { background-color: #1c2a36; }
        QComboBox#sourceCombo, QComboBox#portCombo {
            background-color: #0f1720; color: #c7d3dd;
            border: 1px solid #2a3a4a; border-radius: 6px; padding: 4px 8px;
        }
    )");
}
