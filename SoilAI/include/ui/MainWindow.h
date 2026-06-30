#pragma once
#include <QMainWindow>
#include <QString>
#include <memory>

#include "AIEngine.h"

class Dashboard;

// ============================================================
// MainWindow
// Top-level application window shown after a successful login.
// Hosts the Dashboard as its central widget and will host History,
// Analytics, and Settings as additional pages/tabs in later modules.
// Acts as the "Controller" in the MVC-ish split: it owns the
// DatabaseManager calls triggered by Dashboard's signals.
// ============================================================
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QString currentUser, QWidget *parent = nullptr);

private slots:
    void onSaveData(struct SensorSnapshot snapshot);
    void onPredictNutrients();
    void onRecommendFertilizer();
    void onGenerateReport();
    void onExportPdf();
    void onOpenSettings();

private:
    void buildMenuAndStatusBar();

    QString m_currentUser;
    Dashboard *m_dashboard = nullptr;
    AIEngine m_aiEngine;
    PredictionResult m_lastPrediction;
    bool m_hasLastPrediction = false;
};
