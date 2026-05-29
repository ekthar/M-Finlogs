#include <QApplication>
#include "mainwindow.h"

// ============================================================================
// M-Finlogs Native — Application Entry Point
// ============================================================================

int main(int argc, char *argv[])
{
    // Enable high-DPI scaling for crisp rendering on modern displays
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QApplication app(argc, argv);
    app.setApplicationName("M-Finlogs Native");
    app.setOrganizationName("M-Finlogs");
    app.setApplicationVersion("1.0.0");

    // Construct and show main window
    MainWindow mainWindow;
    mainWindow.show();

    return app.exec();
}
