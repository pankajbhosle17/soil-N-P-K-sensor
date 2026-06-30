#include "DatabaseManager.h"

#include <QSqlQuery>
#include <QSqlRecord>
#include <QVariant>
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QDir>
#include <QDebug>
#include <QJsonDocument>

DatabaseManager &DatabaseManager::instance()
{
    static DatabaseManager mgr;   // thread-safe in C++11+
    return mgr;
}

void DatabaseManager::initialize(const QString &dbFilePath)
{
    if (m_initialized)
        return;

    m_db = QSqlDatabase::addDatabase("QSQLITE", "soilai_connection");
    m_db.setDatabaseName(dbFilePath);

    if (!m_db.open())
        throw DatabaseException("Failed to open database: " + m_db.lastError().text());

    // Enforce foreign keys in SQLite (off by default).
    QSqlQuery pragma(m_db);
    pragma.exec("PRAGMA foreign_keys = ON;");

    createSchema();
    m_initialized = true;
}

void DatabaseManager::createSchema()
{
    QSqlQuery q(m_db);

    const QStringList statements = {
        R"(CREATE TABLE IF NOT EXISTS Users (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                username TEXT UNIQUE NOT NULL,
                password_hash TEXT NOT NULL,
                salt TEXT NOT NULL,
                role TEXT DEFAULT 'operator',
                created_at TEXT NOT NULL
            );)",

        R"(CREATE TABLE IF NOT EXISTS SensorReadings (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp TEXT NOT NULL,
                ph REAL, nitrogen REAL, phosphorus REAL, potassium REAL,
                temperature REAL, humidity REAL,
                location TEXT, crop_type TEXT
            );)",

        R"(CREATE TABLE IF NOT EXISTS Predictions (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                reading_id INTEGER NOT NULL,
                soil_health_score REAL,
                confidence REAL,
                predicted_nutrients_json TEXT,
                created_at TEXT NOT NULL,
                FOREIGN KEY(reading_id) REFERENCES SensorReadings(id) ON DELETE CASCADE
            );)",

        R"(CREATE TABLE IF NOT EXISTS Recommendations (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                prediction_id INTEGER NOT NULL,
                fertilizer TEXT,
                reason TEXT,
                quantity_kg_per_acre REAL,
                application_time TEXT,
                priority TEXT,
                FOREIGN KEY(prediction_id) REFERENCES Predictions(id) ON DELETE CASCADE
            );)",

        R"(CREATE TABLE IF NOT EXISTS Reports (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                reading_id INTEGER NOT NULL,
                file_path TEXT,
                generated_at TEXT NOT NULL,
                FOREIGN KEY(reading_id) REFERENCES SensorReadings(id) ON DELETE CASCADE
            );)"
    };

    for (const auto &stmt : statements) {
        if (!q.exec(stmt))
            throw DatabaseException("Schema creation failed: " + q.lastError().text());
    }
}

QString DatabaseManager::generateSalt()
{
    const QString chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    QString salt;
    for (int i = 0; i < 16; ++i)
        salt += chars.at(QRandomGenerator::global()->bounded(chars.size()));
    return salt;
}

QString DatabaseManager::hashPassword(const QString &plain, const QString &salt)
{
    // SHA-256(plain + salt). For production, prefer bcrypt/argon2 via a
    // dedicated crypto library; this keeps the project dependency-light.
    QByteArray data = (plain + salt).toUtf8();
    return QString(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}

bool DatabaseManager::userExists(const QString &username)
{
    QSqlQuery q(m_db);
    q.prepare("SELECT id FROM Users WHERE username = :u");
    q.bindValue(":u", username);
    if (!q.exec())
        throw DatabaseException("userExists query failed: " + q.lastError().text());
    return q.next();
}

bool DatabaseManager::registerUser(const QString &username, const QString &plainPassword)
{
    if (username.trimmed().isEmpty() || plainPassword.isEmpty())
        throw DatabaseException("Username/password cannot be empty.");

    if (userExists(username))
        return false; // caller should inform "username taken"

    const QString salt = generateSalt();
    const QString hash = hashPassword(plainPassword, salt);

    QSqlQuery q(m_db);
    q.prepare(R"(INSERT INTO Users (username, password_hash, salt, role, created_at)
                 VALUES (:u, :h, :s, :r, :c))");
    q.bindValue(":u", username);
    q.bindValue(":h", hash);
    q.bindValue(":s", salt);
    q.bindValue(":r", "operator");
    q.bindValue(":c", QDateTime::currentDateTime().toString(Qt::ISODate));

    if (!q.exec())
        throw DatabaseException("Failed to register user: " + q.lastError().text());

    return true;
}

std::optional<User> DatabaseManager::authenticate(const QString &username, const QString &plainPassword)
{
    QSqlQuery q(m_db);
    q.prepare("SELECT id, username, password_hash, salt, role, created_at FROM Users WHERE username = :u");
    q.bindValue(":u", username);

    if (!q.exec())
        throw DatabaseException("Authenticate query failed: " + q.lastError().text());

    if (!q.next())
        return std::nullopt; // no such user

    const QString storedHash = q.value("password_hash").toString();
    const QString salt = q.value("salt").toString();
    const QString candidateHash = hashPassword(plainPassword, salt);

    if (candidateHash != storedHash)
        return std::nullopt; // wrong password

    User u(q.value("id").toInt(),
           q.value("username").toString(),
           storedHash,
           q.value("role").toString(),
           QDateTime::fromString(q.value("created_at").toString(), Qt::ISODate));
    return u;
}

int DatabaseManager::insertSensorReading(const SensorReadingRecord &r)
{
    QSqlQuery q(m_db);
    q.prepare(R"(INSERT INTO SensorReadings
                 (timestamp, ph, nitrogen, phosphorus, potassium, temperature, humidity, location, crop_type)
                 VALUES (:ts, :ph, :n, :p, :k, :t, :h, :loc, :crop))");
    q.bindValue(":ts", r.timestamp.toString(Qt::ISODate));
    q.bindValue(":ph", r.ph);
    q.bindValue(":n", r.nitrogen);
    q.bindValue(":p", r.phosphorus);
    q.bindValue(":k", r.potassium);
    q.bindValue(":t", r.temperature);
    q.bindValue(":h", r.humidity);
    q.bindValue(":loc", r.location);
    q.bindValue(":crop", r.cropType);

    if (!q.exec())
        throw DatabaseException("Failed to insert reading: " + q.lastError().text());

    return q.lastInsertId().toInt();
}

QVector<SensorReadingRecord> DatabaseManager::fetchReadings(const QDateTime &from, const QDateTime &to,
                                                              const QString &cropFilter,
                                                              const QString &locationFilter)
{
    QString sql = "SELECT * FROM SensorReadings WHERE 1=1";
    if (from.isValid()) sql += " AND timestamp >= :from";
    if (to.isValid())   sql += " AND timestamp <= :to";
    if (!cropFilter.isEmpty()) sql += " AND crop_type = :crop";
    if (!locationFilter.isEmpty()) sql += " AND location = :loc";
    sql += " ORDER BY timestamp DESC";

    QSqlQuery q(m_db);
    q.prepare(sql);
    if (from.isValid()) q.bindValue(":from", from.toString(Qt::ISODate));
    if (to.isValid())   q.bindValue(":to", to.toString(Qt::ISODate));
    if (!cropFilter.isEmpty()) q.bindValue(":crop", cropFilter);
    if (!locationFilter.isEmpty()) q.bindValue(":loc", locationFilter);

    if (!q.exec())
        throw DatabaseException("Failed to fetch readings: " + q.lastError().text());

    QVector<SensorReadingRecord> results;
    while (q.next()) {
        SensorReadingRecord r;
        r.id = q.value("id").toInt();
        r.timestamp = QDateTime::fromString(q.value("timestamp").toString(), Qt::ISODate);
        r.ph = q.value("ph").toDouble();
        r.nitrogen = q.value("nitrogen").toDouble();
        r.phosphorus = q.value("phosphorus").toDouble();
        r.potassium = q.value("potassium").toDouble();
        r.temperature = q.value("temperature").toDouble();
        r.humidity = q.value("humidity").toDouble();
        r.location = q.value("location").toString();
        r.cropType = q.value("crop_type").toString();
        results.push_back(r);
    }
    return results;
}

bool DatabaseManager::deleteReading(int id)
{
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM SensorReadings WHERE id = :id");
    q.bindValue(":id", id);
    if (!q.exec())
        throw DatabaseException("Failed to delete reading: " + q.lastError().text());
    return q.numRowsAffected() > 0;
}

int DatabaseManager::insertPrediction(int readingId, double soilHealthScore, double confidence,
                                       const QString &predictedNutrientsJson)
{
    QSqlQuery q(m_db);
    q.prepare(R"(INSERT INTO Predictions (reading_id, soil_health_score, confidence, predicted_nutrients_json, created_at)
                 VALUES (:rid, :score, :conf, :json, :created))");
    q.bindValue(":rid", readingId);
    q.bindValue(":score", soilHealthScore);
    q.bindValue(":conf", confidence);
    q.bindValue(":json", predictedNutrientsJson);
    q.bindValue(":created", QDateTime::currentDateTime().toString(Qt::ISODate));

    if (!q.exec())
        throw DatabaseException("Failed to insert prediction: " + q.lastError().text());

    return q.lastInsertId().toInt();
}

int DatabaseManager::insertRecommendation(int predictionId, const QString &fertilizer,
                                           const QString &reason, double quantityKgPerAcre,
                                           const QString &applicationTime, const QString &priority)
{
    QSqlQuery q(m_db);
    q.prepare(R"(INSERT INTO Recommendations
                 (prediction_id, fertilizer, reason, quantity_kg_per_acre, application_time, priority)
                 VALUES (:pid, :fert, :reason, :qty, :time, :prio))");
    q.bindValue(":pid", predictionId);
    q.bindValue(":fert", fertilizer);
    q.bindValue(":reason", reason);
    q.bindValue(":qty", quantityKgPerAcre);
    q.bindValue(":time", applicationTime);
    q.bindValue(":prio", priority);

    if (!q.exec())
        throw DatabaseException("Failed to insert recommendation: " + q.lastError().text());

    return q.lastInsertId().toInt();
}

int DatabaseManager::insertReportRecord(int readingId, const QString &filePath)
{
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO Reports (reading_id, file_path, generated_at) VALUES (:rid, :path, :gen)");
    q.bindValue(":rid", readingId);
    q.bindValue(":path", filePath);
    q.bindValue(":gen", QDateTime::currentDateTime().toString(Qt::ISODate));

    if (!q.exec())
        throw DatabaseException("Failed to insert report record: " + q.lastError().text());

    return q.lastInsertId().toInt();
}
