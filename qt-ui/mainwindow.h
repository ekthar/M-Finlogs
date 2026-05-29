#ifndef MFINLOGS_MAINWINDOW_H
#define MFINLOGS_MAINWINDOW_H

#include <QMainWindow>
#include <QListWidget>
#include <QTableWidget>
#include <QLineEdit>
#include <QDateEdit>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QFrame>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

private:
    void setupUi();
    void setupSidebar();
    void setupContentArea();
    void setupInputForm();
    void setupTable();
    void populateSampleData();

    // Sidebar
    QListWidget *m_sidebarNav = nullptr;

    // Input Form
    QFrame      *m_inputFormContainer = nullptr;
    QDateEdit   *m_dateEdit = nullptr;
    QLineEdit   *m_billNoEdit = nullptr;
    QLineEdit   *m_partyEdit = nullptr;
    QComboBox   *m_typeCombo = nullptr;
    QComboBox   *m_modeCombo = nullptr;
    QDoubleSpinBox *m_amountSpin = nullptr;
    QPushButton *m_btnSave = nullptr;
    QPushButton *m_btnClear = nullptr;

    // Table
    QTableWidget *m_table = nullptr;
    QLineEdit    *m_searchBar = nullptr;

    // Header
    QLabel *m_pageTitle = nullptr;
    QLabel *m_pageSubtitle = nullptr;
};

#endif // MFINLOGS_MAINWINDOW_H
