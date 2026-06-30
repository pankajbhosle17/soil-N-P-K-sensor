#include "LoginWindow.h"
#include "DatabaseManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QMessageBox>
#include <QSettings>
#include <QFrame>

LoginWindow::LoginWindow(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("SoilAI — Smart Soil Testing System");
    setFixedSize(420, 480);
    buildUi();
    applyTheme();
    loadRememberedUser();
}

void LoginWindow::buildUi()
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    // Card-style panel, mimicking the "rounded card" SCADA aesthetic
    auto *card = new QFrame(this);
    card->setObjectName("loginCard");
    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(36, 32, 36, 32);
    cardLayout->setSpacing(16);

    m_titleLabel = new QLabel("🌱  SoilAI Login", card);
    m_titleLabel->setObjectName("titleLabel");
    m_titleLabel->setAlignment(Qt::AlignCenter);

    auto *subtitle = new QLabel("AI-Driven Soil Nutrient Prediction System", card);
    subtitle->setObjectName("subtitleLabel");
    subtitle->setAlignment(Qt::AlignCenter);

    auto *form = new QFormLayout();
    form->setSpacing(12);

    m_usernameEdit = new QLineEdit(card);
    m_usernameEdit->setPlaceholderText("Username");

    auto *pwRow = new QHBoxLayout();
    m_passwordEdit = new QLineEdit(card);
    m_passwordEdit->setPlaceholderText("Password");
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_showPasswordBtn = new QPushButton("Show", card);
    m_showPasswordBtn->setCheckable(true);
    m_showPasswordBtn->setFixedWidth(56);
    pwRow->addWidget(m_passwordEdit);
    pwRow->addWidget(m_showPasswordBtn);

    form->addRow("Username", m_usernameEdit);
    form->addRow("Password", pwRow);

    m_rememberCheck = new QCheckBox("Remember me", card);

    m_loginBtn = new QPushButton("Login", card);
    m_loginBtn->setObjectName("primaryBtn");
    m_registerBtn = new QPushButton("Register new user", card);
    m_registerBtn->setObjectName("secondaryBtn");

    m_statusLabel = new QLabel("", card);
    m_statusLabel->setObjectName("statusLabel");
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setWordWrap(true);

    cardLayout->addWidget(m_titleLabel);
    cardLayout->addWidget(subtitle);
    cardLayout->addSpacing(12);
    cardLayout->addLayout(form);
    cardLayout->addWidget(m_rememberCheck);
    cardLayout->addSpacing(8);
    cardLayout->addWidget(m_loginBtn);
    cardLayout->addWidget(m_registerBtn);
    cardLayout->addWidget(m_statusLabel);
    cardLayout->addStretch();

    outer->addWidget(card);

    connect(m_loginBtn, &QPushButton::clicked, this, &LoginWindow::onLoginClicked);
    connect(m_registerBtn, &QPushButton::clicked, this, &LoginWindow::onRegisterClicked);
    connect(m_showPasswordBtn, &QPushButton::toggled, this, &LoginWindow::onTogglePasswordVisibility);
    connect(m_passwordEdit, &QLineEdit::returnPressed, this, &LoginWindow::onLoginClicked);
}

void LoginWindow::applyTheme()
{
    // Dark SCADA-style theme for the dialog.
    setStyleSheet(R"(
        QDialog { background-color: #0f1720; }
        #loginCard {
            background-color: #16212c;
            border-radius: 14px;
            border: 1px solid #233140;
        }
        #titleLabel { color: #5fe3a1; font-size: 22px; font-weight: 700; }
        #subtitleLabel { color: #8aa0b2; font-size: 11px; }
        QLabel { color: #c7d3dd; }
        QLineEdit {
            background-color: #0f1720;
            border: 1px solid #2a3a4a;
            border-radius: 6px;
            padding: 8px;
            color: #e8f0f5;
        }
        QLineEdit:focus { border: 1px solid #5fe3a1; }
        QPushButton#primaryBtn {
            background-color: #5fe3a1;
            color: #0f1720;
            font-weight: 700;
            border-radius: 8px;
            padding: 10px;
        }
        QPushButton#primaryBtn:hover { background-color: #74f0b3; }
        QPushButton#secondaryBtn {
            background-color: transparent;
            color: #5fe3a1;
            border: 1px solid #5fe3a1;
            border-radius: 8px;
            padding: 8px;
        }
        QPushButton#secondaryBtn:hover { background-color: #1c2a36; }
        #statusLabel { color: #ff6b6b; font-size: 11px; }
        QCheckBox { color: #8aa0b2; }
    )");
}

void LoginWindow::onTogglePasswordVisibility()
{
    m_passwordEdit->setEchoMode(m_showPasswordBtn->isChecked() ? QLineEdit::Normal : QLineEdit::Password);
    m_showPasswordBtn->setText(m_showPasswordBtn->isChecked() ? "Hide" : "Show");
}

void LoginWindow::onLoginClicked()
{
    const QString username = m_usernameEdit->text().trimmed();
    const QString password = m_passwordEdit->text();

    if (username.isEmpty() || password.isEmpty()) {
        m_statusLabel->setText("Please enter both username and password.");
        return;
    }

    try {
        auto result = DatabaseManager::instance().authenticate(username, password);
        if (!result.has_value()) {
            m_statusLabel->setText("Invalid username or password.");
            return;
        }

        m_authenticatedUser = result->username();
        persistRememberedUser(username, m_rememberCheck->isChecked());
        accept(); // closes dialog with QDialog::Accepted
    } catch (const DatabaseException &ex) {
        QMessageBox::critical(this, "Database Error", ex.what());
    }
}

void LoginWindow::onRegisterClicked()
{
    const QString username = m_usernameEdit->text().trimmed();
    const QString password = m_passwordEdit->text();

    if (username.isEmpty() || password.isEmpty()) {
        m_statusLabel->setText("Enter a username and password to register.");
        return;
    }
    if (password.length() < 4) {
        m_statusLabel->setText("Password must be at least 4 characters.");
        return;
    }

    try {
        const bool ok = DatabaseManager::instance().registerUser(username, password);
        if (!ok) {
            m_statusLabel->setText("Username already taken.");
            return;
        }
        m_statusLabel->setStyleSheet("color: #5fe3a1;");
        m_statusLabel->setText("Registration successful. You can now log in.");
    } catch (const DatabaseException &ex) {
        QMessageBox::critical(this, "Database Error", ex.what());
    }
}

void LoginWindow::loadRememberedUser()
{
    QSettings settings("SoilAI", "SoilAIApp");
    if (settings.value("remember/enabled", false).toBool()) {
        m_usernameEdit->setText(settings.value("remember/username").toString());
        m_rememberCheck->setChecked(true);
        m_passwordEdit->setFocus();
    }
}

void LoginWindow::persistRememberedUser(const QString &username, bool remember)
{
    QSettings settings("SoilAI", "SoilAIApp");
    settings.setValue("remember/enabled", remember);
    if (remember)
        settings.setValue("remember/username", username);
    else
        settings.remove("remember/username");
}
