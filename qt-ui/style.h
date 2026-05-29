#ifndef MFINLOGS_STYLE_H
#define MFINLOGS_STYLE_H

#include <QString>

namespace MFinlogs {

// ============================================================================
// M-Finlogs Native — Premium Mac-Grade QSS Theme
// ============================================================================
// Apply globally via: qApp->setStyleSheet(MFinlogs::globalStyleSheet());
// ============================================================================

inline QString globalStyleSheet()
{
    return QStringLiteral(R"qss(

/* ==========================================================================
   1. GLOBAL THEME & TYPOGRAPHY
   ========================================================================== */

* {
    font-family: "Inter", "Segoe UI", -apple-system, sans-serif;
    font-size: 10pt;
    color: #111827;
    outline: none;
}

QMainWindow {
    background-color: #FFFFFF;
    border: none;
}

QWidget {
    background-color: transparent;
    border: none;
}

QToolTip {
    background-color: #1F2937;
    color: #FFFFFF;
    border: none;
    border-radius: 4px;
    padding: 6px 10px;
    font-size: 9pt;
}

QScrollBar:vertical {
    background: transparent;
    width: 8px;
    margin: 0;
    border: none;
}

QScrollBar::handle:vertical {
    background: #D1D5DB;
    border-radius: 4px;
    min-height: 30px;
}

QScrollBar::handle:vertical:hover {
    background: #9CA3AF;
}

QScrollBar::add-line:vertical,
QScrollBar::sub-line:vertical,
QScrollBar::add-page:vertical,
QScrollBar::sub-page:vertical {
    background: transparent;
    height: 0px;
    border: none;
}

QScrollBar:horizontal {
    background: transparent;
    height: 8px;
    margin: 0;
    border: none;
}

QScrollBar::handle:horizontal {
    background: #D1D5DB;
    border-radius: 4px;
    min-width: 30px;
}

QScrollBar::handle:horizontal:hover {
    background: #9CA3AF;
}

QScrollBar::add-line:horizontal,
QScrollBar::sub-line:horizontal,
QScrollBar::add-page:horizontal,
QScrollBar::sub-page:horizontal {
    background: transparent;
    width: 0px;
    border: none;
}

/* ==========================================================================
   2. LEFT SIDEBAR (QListWidget — Navigation)
   ========================================================================== */

QListWidget#sidebarNav {
    background-color: #F3F4F6;
    border: none;
    border-right: none;
    padding: 8px;
    outline: none;
}

QListWidget#sidebarNav::item {
    background-color: transparent;
    color: #6B7280;
    padding: 8px 12px;
    border-radius: 6px;
    margin: 1px 4px;
    font-size: 10pt;
}

QListWidget#sidebarNav::item:hover {
    background-color: #E5E7EB;
    color: #374151;
}

QListWidget#sidebarNav::item:selected {
    background-color: #3B82F6;
    color: #FFFFFF;
    font-weight: 600;
}

QListWidget#sidebarNav::item:selected:hover {
    background-color: #2563EB;
    color: #FFFFFF;
}

/* Remove dotted focus rectangle */
QListWidget#sidebarNav:focus {
    outline: none;
    border: none;
}

/* ==========================================================================
   3. HEADER & INPUT FORM CONTAINER (QFrame)
   ========================================================================== */

QFrame#inputFormContainer {
    background-color: #FFFFFF;
    border: 1px solid #E5E7EB;
    border-radius: 8px;
    padding: 16px;
}

/* --- Labels inside form --- */
QLabel {
    color: #374151;
    font-size: 9pt;
    font-weight: 500;
    background: transparent;
    border: none;
    padding: 0px;
    margin-bottom: 2px;
}

/* --- Input Fields: QLineEdit, QDateEdit, QComboBox, QSpinBox, QDoubleSpinBox --- */

QLineEdit, QDateEdit, QComboBox, QSpinBox, QDoubleSpinBox {
    min-height: 32px;
    padding: 4px 10px;
    background-color: #FFFFFF;
    border: 1px solid #D1D5DB;
    border-radius: 5px;
    color: #111827;
    font-size: 10pt;
    selection-background-color: #BFDBFE;
    selection-color: #1E3A5F;
}

QLineEdit:focus, QDateEdit:focus, QComboBox:focus,
QSpinBox:focus, QDoubleSpinBox:focus {
    border: 1px solid #3B82F6;
}

QLineEdit:hover, QDateEdit:hover, QComboBox:hover,
QSpinBox:hover, QDoubleSpinBox:hover {
    border: 1px solid #9CA3AF;
}

QLineEdit:focus:hover, QDateEdit:focus:hover, QComboBox:focus:hover,
QSpinBox:focus:hover, QDoubleSpinBox:focus:hover {
    border: 1px solid #3B82F6;
}

QLineEdit:disabled, QDateEdit:disabled, QComboBox:disabled {
    background-color: #F9FAFB;
    color: #9CA3AF;
    border: 1px solid #E5E7EB;
}

/* --- QComboBox dropdown arrow --- */
QComboBox::drop-down {
    subcontrol-origin: padding;
    subcontrol-position: center right;
    width: 24px;
    border: none;
    background: transparent;
}

QComboBox::down-arrow {
    image: none;
    width: 0;
    height: 0;
    border-left: 4px solid transparent;
    border-right: 4px solid transparent;
    border-top: 5px solid #6B7280;
}

QComboBox QAbstractItemView {
    background-color: #FFFFFF;
    border: 1px solid #E5E7EB;
    border-radius: 6px;
    padding: 4px;
    selection-background-color: #EFF6FF;
    selection-color: #111827;
    outline: none;
}

QComboBox QAbstractItemView::item {
    padding: 6px 10px;
    border-radius: 4px;
    min-height: 28px;
}

QComboBox QAbstractItemView::item:hover {
    background-color: #F3F4F6;
}

QComboBox QAbstractItemView::item:selected {
    background-color: #EFF6FF;
    color: #1D4ED8;
}

/* --- QDateEdit calendar button --- */
QDateEdit::drop-down {
    subcontrol-origin: padding;
    subcontrol-position: center right;
    width: 28px;
    border: none;
    background: transparent;
}

/* --- QSpinBox / QDoubleSpinBox buttons --- */
QSpinBox::up-button, QDoubleSpinBox::up-button,
QSpinBox::down-button, QDoubleSpinBox::down-button {
    width: 20px;
    border: none;
    background: transparent;
}

QSpinBox::up-arrow, QDoubleSpinBox::up-arrow {
    width: 0; height: 0;
    border-left: 4px solid transparent;
    border-right: 4px solid transparent;
    border-bottom: 5px solid #6B7280;
}

QSpinBox::down-arrow, QDoubleSpinBox::down-arrow {
    width: 0; height: 0;
    border-left: 4px solid transparent;
    border-right: 4px solid transparent;
    border-top: 5px solid #6B7280;
}

/* ==========================================================================
   3b. BUTTONS (QPushButton)
   ========================================================================== */

/* --- Primary Action: "Save Entry" --- */
QPushButton#btnSaveEntry {
    background-color: #3B82F6;
    color: #FFFFFF;
    font-weight: 600;
    font-size: 10pt;
    border: none;
    border-radius: 6px;
    min-height: 32px;
    padding: 6px 20px;
}

QPushButton#btnSaveEntry:hover {
    background-color: #2563EB;
}

QPushButton#btnSaveEntry:pressed {
    background-color: #1D4ED8;
}

QPushButton#btnSaveEntry:disabled {
    background-color: #93C5FD;
    color: #FFFFFF;
}

/* --- Secondary Action: "Clear" / "Delete" --- */
QPushButton#btnClear, QPushButton#btnDelete {
    background-color: #E5E7EB;
    color: #374151;
    font-weight: 500;
    font-size: 10pt;
    border: none;
    border-radius: 6px;
    min-height: 32px;
    padding: 6px 20px;
}

QPushButton#btnClear:hover, QPushButton#btnDelete:hover {
    background-color: #D1D5DB;
    color: #1F2937;
}

QPushButton#btnClear:pressed, QPushButton#btnDelete:pressed {
    background-color: #9CA3AF;
}

/* --- Generic button fallback --- */
QPushButton {
    background-color: #F3F4F6;
    color: #374151;
    font-weight: 500;
    border: 1px solid #E5E7EB;
    border-radius: 6px;
    min-height: 32px;
    padding: 6px 16px;
}

QPushButton:hover {
    background-color: #E5E7EB;
    border-color: #D1D5DB;
}

QPushButton:pressed {
    background-color: #D1D5DB;
}

/* ==========================================================================
   4. DATA TABLE (QTableWidget)
   ========================================================================== */

QTableWidget {
    background-color: #FFFFFF;
    alternate-background-color: #F9FAFB;
    border: 1px solid #E5E7EB;
    border-radius: 8px;
    gridline-color: transparent;
    outline: none;
    selection-background-color: #EFF6FF;
    selection-color: #111827;
}

QTableWidget::item {
    padding: 8px 12px;
    border: none;
    border-bottom: 1px solid #F3F4F6;
}

QTableWidget::item:selected {
    background-color: #EFF6FF;
    color: #111827;
}

QTableWidget::item:focus {
    outline: none;
    border: none;
    border-bottom: 1px solid #F3F4F6;
}

/* --- Table Header --- */
QHeaderView {
    background-color: #FFFFFF;
    border: none;
}

QHeaderView::section {
    background-color: #FFFFFF;
    color: #6B7280;
    font-size: 8pt;
    font-weight: 700;
    text-transform: uppercase;
    letter-spacing: 0.5px;
    padding: 10px 12px;
    border: none;
    border-bottom: 1px solid #E5E7EB;
    border-right: none;
}

QHeaderView::section:hover {
    background-color: #F9FAFB;
    color: #374151;
}

/* Remove sort indicator styling */
QHeaderView::down-arrow {
    width: 0; height: 0;
    border-left: 4px solid transparent;
    border-right: 4px solid transparent;
    border-top: 5px solid #6B7280;
    margin-right: 8px;
}

QHeaderView::up-arrow {
    width: 0; height: 0;
    border-left: 4px solid transparent;
    border-right: 4px solid transparent;
    border-bottom: 5px solid #6B7280;
    margin-right: 8px;
}

/* --- Remove focus dotted border on table corner --- */
QTableCornerButton::section {
    background-color: #FFFFFF;
    border: none;
    border-bottom: 1px solid #E5E7EB;
}

/* ==========================================================================
   5. SEARCH BAR (QLineEdit used for search)
   ========================================================================== */

QLineEdit#searchBar {
    min-height: 32px;
    padding: 4px 10px 4px 30px;
    background-color: #F9FAFB;
    border: 1px solid #E5E7EB;
    border-radius: 6px;
    color: #111827;
    font-size: 10pt;
}

QLineEdit#searchBar:focus {
    background-color: #FFFFFF;
    border: 1px solid #3B82F6;
}

/* ==========================================================================
   6. STATUS BAR & MISC
   ========================================================================== */

QStatusBar {
    background-color: #F9FAFB;
    border-top: 1px solid #E5E7EB;
    color: #6B7280;
    font-size: 8pt;
    padding: 4px 12px;
}

QStatusBar::item {
    border: none;
}

QMenuBar {
    background-color: #FFFFFF;
    border-bottom: 1px solid #E5E7EB;
    padding: 2px 8px;
}

QMenuBar::item {
    background: transparent;
    padding: 6px 12px;
    border-radius: 4px;
    color: #374151;
}

QMenuBar::item:selected {
    background-color: #F3F4F6;
}

QMenu {
    background-color: #FFFFFF;
    border: 1px solid #E5E7EB;
    border-radius: 8px;
    padding: 4px;
}

QMenu::item {
    padding: 8px 24px 8px 12px;
    border-radius: 4px;
    color: #374151;
}

QMenu::item:selected {
    background-color: #EFF6FF;
    color: #1D4ED8;
}

QMenu::separator {
    height: 1px;
    background-color: #E5E7EB;
    margin: 4px 8px;
}

/* --- Group Box --- */
QGroupBox {
    background-color: #FFFFFF;
    border: 1px solid #E5E7EB;
    border-radius: 8px;
    margin-top: 16px;
    padding-top: 24px;
    font-weight: 600;
    color: #111827;
}

QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top left;
    padding: 4px 12px;
    color: #374151;
    font-size: 10pt;
    font-weight: 600;
}

/* --- Tab Widget (if used) --- */
QTabWidget::pane {
    background-color: #FFFFFF;
    border: 1px solid #E5E7EB;
    border-radius: 8px;
    top: -1px;
}

QTabBar::tab {
    background-color: transparent;
    color: #6B7280;
    padding: 8px 16px;
    border: none;
    border-bottom: 2px solid transparent;
    font-weight: 500;
}

QTabBar::tab:hover {
    color: #374151;
}

QTabBar::tab:selected {
    color: #3B82F6;
    border-bottom: 2px solid #3B82F6;
    font-weight: 600;
}

/* ==========================================================================
   7. PAGE TITLE / HEADING LABEL
   ========================================================================== */

QLabel#pageTitle {
    font-size: 18pt;
    font-weight: 700;
    color: #111827;
    background: transparent;
    border: none;
    padding: 0;
    margin: 0;
}

QLabel#pageSubtitle {
    font-size: 9pt;
    font-weight: 400;
    color: #6B7280;
    background: transparent;
    border: none;
    padding: 0;
    margin: 0;
}

/* ==========================================================================
   8. STATUS INDICATORS (badges in header)
   ========================================================================== */

QLabel#statusBadge {
    font-size: 8pt;
    color: #059669;
    background-color: #ECFDF5;
    border: none;
    border-radius: 10px;
    padding: 3px 8px;
    font-weight: 500;
}

)qss");
}

} // namespace MFinlogs

#endif // MFINLOGS_STYLE_H
