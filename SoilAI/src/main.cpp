#include <QApplication>
#include <QMessageBox>
#include "DatabaseManager.h"
#include "LoginWindow.h"
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("SoilAI");
    QApplication::setOrganizationName("SoilAI");

    try {
        DatabaseManager::instance().initialize("soilai.db");
    } catch (const DatabaseException &ex) {
        QMessageBox::critical(nullptr, "Fatal Database Error", ex.what());
        return 1;
    }

    LoginWindow login;
    if (login.exec() != QDialog::Accepted) {
        return 0; // user closed/cancelled login
    }

    MainWindow window(login.authenticatedUser());
    window.show();
    return app.exec();
}
