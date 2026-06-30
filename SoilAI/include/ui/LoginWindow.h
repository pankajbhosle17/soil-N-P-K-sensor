#pragma once
#include <QDialog>
#include <QString>

class QLineEdit;
class QCheckBox;
class QPushButton;
class QLabel;

// ============================================================
// LoginWindow
// Modal dialog handling sign-in and new-user registration against
// DatabaseManager. On success it stores the authenticated username
// (and emits nothing further — MainWindow polls authenticatedUser()
// after exec() returns QDialog::Accepted).
// ============================================================
class LoginWindow : public QDialog
{
    Q_OBJECT
public:
    explicit LoginWindow(QWidget *parent = nullptr);

    QString authenticatedUser() const { return m_authenticatedUser; }

private slots:
    void onLoginClicked();
    void onRegisterClicked();
    void onTogglePasswordVisibility();

private:
    void buildUi();
    void applyTheme();
    void loadRememberedUser();
    void persistRememberedUser(const QString &username, bool remember);

    QLineEdit *m_usernameEdit = nullptr;
    QLineEdit *m_passwordEdit = nullptr;
    QCheckBox *m_rememberCheck = nullptr;
    QPushButton *m_loginBtn = nullptr;
    QPushButton *m_registerBtn = nullptr;
    QPushButton *m_showPasswordBtn = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_titleLabel = nullptr;

    QString m_authenticatedUser;
};
