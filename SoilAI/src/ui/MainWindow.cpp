#include "MainWindow.h"
#include "Dashboard.h"
#include "DatabaseManager.h"
#include "SensorSimulator.h"   // for SensorSnapshot definition

#include <QMenuBar>
#include <QStatusBar>
#include <QMessageBox>
#include <QDateTime>
#include <QInputDialog>
#include <QJsonDocument>

MainWindow::MainWindow(QString currentUser, QWidget *parent)
    : QMainWindow(parent), m_currentUser(std::move(currentUser))
{
    setWindowTitle("SoilAI — Smart Soil Testing Dashboard");
    resize(1280, 800);

    m_dashboard = new Dashboard(m_currentUser, this);
    setCentralWidget(m_dashboard);

    connect(m_dashboard, &Dashboard::requestSaveData, this, &MainWindow::onSaveData);
    connect(m_dashboard, &Dashboard::requestPredictNutrients, this, &MainWindow::onPredictNutrients);
    connect(m_dashboard, &Dashboard::requestRecommendFertilizer, this, &MainWindow::onRecommendFertilizer);
    connect(m_dashboard, &Dashboard::requestGenerateReport, this, &MainWindow::onGenerateReport);
    connect(m_dashboard, &Dashboard::requestExportPdf, this, &MainWindow::onExportPdf);
    connect(m_dashboard, &Dashboard::requestOpenSettings, this, &MainWindow::onOpenSettings);

    buildMenuAndStatusBar();
    setStyleSheet("QMainWindow { background-color: #0f1720; } QMenuBar, QStatusBar { background-color: #16212c; color: #c7d3dd; }");
}

void MainWindow::buildMenuAndStatusBar()
{
    auto *fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction("Logout", [this]() { close(); });
    fileMenu->addAction("Exit", qApp, &QApplication::quit);

    auto *viewMenu = menuBar()->addMenu("&View");
    viewMenu->addAction("History");   // wired in the History/Analytics module
    viewMenu->addAction("Analytics");

    auto *helpMenu = menuBar()->addMenu("&Help");
    helpMenu->addAction("About", [this]() {
        QMessageBox::information(this, "About SoilAI",
            "SoilAI — AI-Driven Prediction of Soil Nutrient Availability\n"
            "using Minimal Sensor Data and Contextual Information.");
    });

    statusBar()->showMessage(QString("Logged in as %1").arg(m_currentUser));
}

void MainWindow::onSaveData(SensorSnapshot snapshot)
{
    try {
        SensorReadingRecord rec;
        rec.timestamp = QDateTime::currentDateTime();
        rec.ph = snapshot.ph;
        rec.nitrogen = snapshot.nitrogen;
        rec.phosphorus = snapshot.phosphorus;
        rec.potassium = snapshot.potassium;
        rec.temperature = snapshot.temperature;
        rec.humidity = snapshot.humidity;
        rec.location = "Bengaluru, KA";   // replaced by WeatherManager/Settings later
        rec.cropType = "Unspecified";

        const int id = DatabaseManager::instance().insertSensorReading(rec);
        statusBar()->showMessage(QString("Reading #%1 saved to database.").arg(id), 4000);
    } catch (const DatabaseException &ex) {
        QMessageBox::critical(this, "Database Error", ex.what());
    }
}

void MainWindow::onPredictNutrients()
{
    SensorSnapshot snap = m_dashboard->latestSnapshot();

    // ---- Gather the contextual inputs the brief asks for: rainfall, soil type, crop type ----
    bool ok = false;
    const double rainfall = QInputDialog::getDouble(
        this, "Predict Nutrients — Context", "Recent rainfall (mm, last 7 days):",
        0.0, 0.0, 1000.0, 1, &ok);
    if (!ok) return;

    const QStringList soilTypes = {"Sandy", "Loamy", "Clayey", "Black", "Red"};
    const QString soilType = QInputDialog::getItem(
        this, "Predict Nutrients — Context", "Soil type:", soilTypes, 1, false, &ok);
    if (!ok) return;

    const QStringList cropTypes = {"Wheat", "Rice", "Maize", "Cotton", "Sugarcane"};
    const QString cropType = QInputDialog::getItem(
        this, "Predict Nutrients — Context", "Crop type:", cropTypes, 0, false, &ok);
    if (!ok) return;

    PredictionInput input;
    input.ph = snap.ph;
    input.nitrogen = snap.nitrogen;
    input.phosphorus = snap.phosphorus;
    input.potassium = snap.potassium;
    input.temperature = snap.temperature;
    input.humidity = snap.humidity;
    input.rainfallMm = rainfall;
    input.soilType = soilType;
    input.cropType = cropType;

    m_lastPrediction = m_aiEngine.predict(input);
    m_hasLastPrediction = true;

    try {
        // Persist the reading + prediction together so History/Reports can link them.
        SensorReadingRecord rec;
        rec.timestamp = QDateTime::currentDateTime();
        rec.ph = snap.ph;
        rec.nitrogen = snap.nitrogen;
        rec.phosphorus = snap.phosphorus;
        rec.potassium = snap.potassium;
        rec.temperature = snap.temperature;
        rec.humidity = snap.humidity;
        rec.location = "Bengaluru, KA";
        rec.cropType = cropType;

        const int readingId = DatabaseManager::instance().insertSensorReading(rec);
        const QString json = QJsonDocument(m_lastPrediction.toJson()).toJson(QJsonDocument::Compact);
        DatabaseManager::instance().insertPrediction(
            readingId, m_lastPrediction.soilHealthScore, m_lastPrediction.confidence, json);

        statusBar()->showMessage(
            QString("Prediction saved (reading #%1, health score %.0f%%).")
                .arg(readingId).arg(m_lastPrediction.soilHealthScore), 5000);
    } catch (const DatabaseException &ex) {
        QMessageBox::warning(this, "Database Warning",
            QString("Prediction computed but could not be saved: %1").arg(ex.what()));
    }

    QMessageBox::information(this, "AI Prediction Result", QString(
        "Predicted Available Nutrients\n"
        "  Nitrogen:   %1 ppm\n"
        "  Phosphorus: %2 ppm\n"
        "  Potassium:  %3 ppm\n\n"
        "Soil Health Score: %4 / 100  (%5)\n"
        "Prediction Confidence: %6%%"
    ).arg(m_lastPrediction.predictedNitrogen, 0, 'f', 1)
     .arg(m_lastPrediction.predictedPhosphorus, 0, 'f', 1)
     .arg(m_lastPrediction.predictedPotassium, 0, 'f', 1)
     .arg(m_lastPrediction.soilHealthScore, 0, 'f', 0)
     .arg(m_lastPrediction.soilHealthLabel())
     .arg(m_lastPrediction.confidence * 100.0, 0, 'f', 0));
}

void MainWindow::onRecommendFertilizer()
{
    if (!m_hasLastPrediction) {
        QMessageBox::information(this, "Recommend Fertilizer",
            "Run \"Predict Nutrients\" first so the recommendation engine has a prediction to work from.");
        return;
    }
    QMessageBox::information(this, "Recommend Fertilizer",
        "RecommendationEngine module not yet generated — it will consume this last prediction "
        "(health score, predicted N/P/K) directly. Coming in the next step.");
}

void MainWindow::onGenerateReport()
{
    QMessageBox::information(this, "Generate Report",
        "ReportGenerator module not yet generated — coming soon.");
}

void MainWindow::onExportPdf()
{
    QMessageBox::information(this, "Export PDF",
        "PDF export will use the ReportGenerator module — coming soon.");
}

void MainWindow::onOpenSettings()
{
    QMessageBox::information(this, "Settings",
        "SettingsManager + Settings dialog module not yet generated — coming soon.");
}
