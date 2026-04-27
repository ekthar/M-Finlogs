#include "app/DesktopApplication.h"

#include <QAction>
#include <QApplication>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QMainWindow>
#include <QMessageBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QStringList>
#include <QTableView>
#include <QToolBar>
#include <QVBoxLayout>

#include <exception>

namespace mfinlogs::app {

DesktopApplication::DesktopApplication(AppContext& context)
    : context_(context) {
    setWindowTitle(QStringLiteral("M-Finlogs Native"));
    resize(1280, 820);
    buildNavigation();
    wireActions();
}

void DesktopApplication::buildNavigation() {
    QSplitter* splitter = new QSplitter(this);
    QListWidget* nav = new QListWidget(splitter);
    QStackedWidget* pages = new QStackedWidget(splitter);

    const QStringList pageNames = {
        QStringLiteral("Daily Transactions"),
        QStringLiteral("Dashboard"),
        QStringLiteral("Party Ledger"),
        QStringLiteral("Day Book"),
        QStringLiteral("Daily Summary"),
        QStringLiteral("Short / Excess"),
        QStringLiteral("Purchase Report"),
        QStringLiteral("Expenses"),
        QStringLiteral("Outstanding"),
        QStringLiteral("Trial Balance"),
        QStringLiteral("P & L"),
        QStringLiteral("Inventory Management"),
        QStringLiteral("Inventory Stock Value"),
        QStringLiteral("Add Party"),
        QStringLiteral("Audit Logs"),
        QStringLiteral("Settings")
    };

    for (const QString& name : pageNames) {
        nav->addItem(name);
        QWidget* page = new QWidget(pages);
        QVBoxLayout* layout = new QVBoxLayout(page);
        QLabel* title = new QLabel(QStringLiteral("<h2>%1</h2>").arg(name), page);
        QLabel* detail = new QLabel(
            QStringLiteral("Native screen placeholder. Services and models are wired through AppContext; feature parity is implemented screen by screen."),
            page
        );
        detail->setWordWrap(true);
        layout->addWidget(title);
        layout->addWidget(detail);
        layout->addStretch(1);
        pages->addWidget(page);
    }

    connect(nav, &QListWidget::currentRowChanged, pages, &QStackedWidget::setCurrentIndex);
    nav->setCurrentRow(0);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    setCentralWidget(splitter);
}

void DesktopApplication::wireActions() {
    QToolBar* toolbar = addToolBar(QStringLiteral("Main"));
    toolbar->setMovable(false);
    QAction* saveAction = toolbar->addAction(QStringLiteral("Save"));
    QAction* exportAction = toolbar->addAction(QStringLiteral("Export"));
    QAction* updateAction = toolbar->addAction(QStringLiteral("Check Updates"));

    connect(saveAction, &QAction::triggered, this, [this]() {
        Q_UNUSED(context_);
    });
    connect(exportAction, &QAction::triggered, this, [this]() {
        Q_UNUSED(context_);
    });
    connect(updateAction, &QAction::triggered, this, [this]() {
        try {
            context_.services().updates->checkForUpdates();
        } catch (const std::exception& err) {
            QMessageBox::warning(this, QStringLiteral("Native Rewrite"), QString::fromUtf8(err.what()));
        }
    });
}

void DesktopApplication::buildPlaceholderWorkspace() {}

int runDesktopApplication(int argc, char** argv) {
    QApplication qtApp(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("M-Finlogs"));
    QApplication::setApplicationName(QStringLiteral("M-Finlogs"));

    AppContext context(QStringLiteral("M-Finlogs"), QStringLiteral("M-Finlogs"));
    DesktopApplication window(context);
    window.show();
    return qtApp.exec();
}

} // namespace mfinlogs::app
