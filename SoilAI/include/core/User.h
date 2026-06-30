#pragma once
#include <QString>
#include <QDateTime>

// ============================================================
// User
// Plain data model representing an authenticated application user.
// Encapsulation: all fields are private with accessor methods.
// ============================================================
class User
{
public:
    User() = default;

    User(int id, QString username, QString passwordHash, QString role, QDateTime createdAt)
        : m_id(id),
          m_username(std::move(username)),
          m_passwordHash(std::move(passwordHash)),
          m_role(std::move(role)),
          m_createdAt(std::move(createdAt))
    {}

    int id() const { return m_id; }
    QString username() const { return m_username; }
    QString passwordHash() const { return m_passwordHash; }
    QString role() const { return m_role; }
    QDateTime createdAt() const { return m_createdAt; }

    void setId(int id) { m_id = id; }
    void setUsername(const QString &u) { m_username = u; }
    void setPasswordHash(const QString &h) { m_passwordHash = h; }
    void setRole(const QString &r) { m_role = r; }

    bool isValid() const { return m_id > 0 && !m_username.isEmpty(); }

private:
    int m_id = -1;
    QString m_username;
    QString m_passwordHash;
    QString m_role = "operator";   // "admin" | "operator"
    QDateTime m_createdAt;
};
