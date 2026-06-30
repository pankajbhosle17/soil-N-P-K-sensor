#pragma once
#include <QString>
#include <QSqlDatabase>
#include <QSqlError>
#include <QVector>
#include <QDateTime>
#include <optional>
#include <stdexcept>

#include "User.h"

// ============================================================
// DatabaseException
// Custom exception type used throughout the data layer.
// ============================================================
class DatabaseException : public std::runtime_error
{
public:
    explicit DatabaseException(const QString &msg)
        : std::runtime_error(msg.toStdString()) {}
};

// Simple POD struct representing one stored sensor reading.
struct SensorReadingRecord
{
    int id = -1;
    QDateTime timestamp;
    double ph = 0, nitrogen = 0, phosphorus = 0, potassium = 0;
    double temperature = 0, humidity = 0;
    QString location;
    QString cropType;
};

// ============================================================
// DatabaseManager
// Singleton (Meyers' singleton) responsible for opening the SQLite
// database, creating schema on first run, and providing CRUD access
// to Users / SensorReadings / Predictions / Recommendations / Reports.
//
// Design notes:
//  - Single Responsibility: this class only talks to SQLite.
//  - All public methods throw DatabaseException on failure so the
//    UI layer can catch and present friendly error dialogs.
// ============================================================
class DatabaseManager
{
public:
    static DatabaseManager &instance();

    // Opens (or creates) the database file and builds the schema.
    void initialize(const QString &dbFilePath = QStringLiteral("soilai.db"));

    // ---------------- Users / Auth ----------------
    bool registerUser(const QString &username, const QString &plainPassword);
    std::optional<User> authenticate(const QString &username, const QString &plainPassword);
    bool userExists(const QString &username);

    // ---------------- Sensor Readings ----------------
    int insertSensorReading(const SensorReadingRecord &r);
    QVector<SensorReadingRecord> fetchReadings(const QDateTime &from = QDateTime(),
                                                const QDateTime &to = QDateTime(),
                                                const QString &cropFilter = QString(),
                                                const QString &locationFilter = QString());
    bool deleteReading(int id);

    // ---------------- Predictions / Recommendations / Reports ----------------
    int insertPrediction(int readingId, double soilHealthScore, double confidence,
                          const QString &predictedNutrientsJson);
    int insertRecommendation(int predictionId, const QString &fertilizer,
                              const QString &reason, double quantityKgPerAcre,
                              const QString &applicationTime, const QString &priority);
    int insertReportRecord(int readingId, const QString &filePath);

    QSqlDatabase &db() { return m_db; }

private:
    DatabaseManager() = default;
    ~DatabaseManager() = default;
    DatabaseManager(const DatabaseManager &) = delete;
    DatabaseManager &operator=(const DatabaseManager &) = delete;

    void createSchema();
    static QString hashPassword(const QString &plain, const QString &salt);
    static QString generateSalt();

    QSqlDatabase m_db;
    bool m_initialized = false;
};
