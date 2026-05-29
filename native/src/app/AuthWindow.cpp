#include "AuthWindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QApplication>
#include <QScreen>
#include <QGraphicsDropShadowEffect>

AuthWindow::AuthWindow(QWidget *parent) 
    : QWidget(parent, Qt::FramelessWindowHint) {
    
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedSize(440, 500);

    if (const auto *screen = QApplication::primaryScreen()) {
        move(screen->geometry().center() - rect().center());
    }

    auto *shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(30);
    shadow->setColor(QColor(0, 0, 0, 70));
    shadow->setOffset(0, 10);

    auto *container = new QFrame(this);
    container->setGraphicsEffect(shadow);

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(20, 20, 20, 20);
    rootLayout->addWidget(container);

    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(40, 30, 40, 40);
    layout->setSpacing(16);

    // Top Header with close box placement
    auto *headerLayout = new QHBoxLayout();
    headerLayout->addStretch();
    closeButton = new QPushButton("✕", container);
    closeButton->setFixedSize(28, 28);
    closeButton->setStyleSheet(
        "QPushButton { background: transparent; color: #9CA3AF; border: none; font-size: 14px; border-radius: 14px; }"
        "QPushButton:hover { background: #fee2e2; color: #ef4444; }"
    );
    connect(closeButton, &QPushButton::clicked, qApp, &QApplication::quit);
    headerLayout->addWidget(closeButton);
    layout->addLayout(headerLayout);

    // Typography Identity
    auto *titleLabel = new QLabel("Welcome Back", container);
    titleLabel->setStyleSheet("font-size: 26px; font-weight: 700; color: #111827; font-family: 'Segoe UI';");
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel);

    auto *subtitleLabel = new QLabel("Sign in to manage your accounting workspaces", container);
    subtitleLabel->setStyleSheet("font-size: 12px; color: #6B7280; font-family: 'Segoe UI';");
    subtitleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(subtitleLabel);

    layout->addSpacing(15);

    // Custom Styled Input Fields
    QString inputStyle = 
        "QLineEdit {"
        "   background-color: #FFFFFF;"
        "   border: 1px solid #E5E7EB;"
        "   border-radius: 8px;"
        "   padding: 10px 14px;"
        "   color: #111827;"
        "   font-size: 13px;"
        "}"
        "QLineEdit:focus {"
        "   border: 2px solid #3B82F6;"
        "   background-color: #FFFFFF;"
        "}";

    usernameField = new QLineEdit(container);
    usernameField->setPlaceholderText("Username or Email");
    usernameField->setStyleSheet(inputStyle);
    layout->addWidget(usernameField);

    passwordField = new QLineEdit(container);
    passwordField->setPlaceholderText("Password");
    passwordField->setEchoMode(QLineEdit::Password);
    passwordField->setStyleSheet(inputStyle);
    layout->addWidget(passwordField);

    errorLabel = new QLabel("", container);
    errorLabel->setStyleSheet("color: #EF4444; font-size: 11px; font-weight: 500;");
    errorLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(errorLabel);

    layout->addSpacing(10);

    // Apple Blue Action Button
    loginButton = new QPushButton("Sign In", container);
    loginButton->setFixedHeight(42);
    loginButton->setStyleSheet(
        "QPushButton {"
        "   background-color: #3B82F6;"
        "   color: #FFFFFF;"
        "   font-size: 14px;"
        "   font-weight: 600;"
        "   border: none;"
        "   border-radius: 8px;"
        "}"
        "QPushButton:hover { background-color: #2563EB; }"
        "QPushButton:pressed { background-color: #1D4ED8; }"
    );
    connect(loginButton, &QPushButton::clicked, this, &AuthWindow::handleLogin);
    layout->addWidget(loginButton);

    layout->addStretch();
}

void AuthWindow::handleLogin() {
    // Simple modern auth check validation sequence
    if (usernameField->text() == "admin" && passwordField->text() == "admin") {
        emit authenticated();
        this->close();
    } else {
        errorLabel->setText("Invalid credentials. Please check and try again.");
    }
}

void AuthWindow::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QPainterPath path;
    QRect innerRect = rect().adjusted(20, 20, -20, -20);
    path.addRoundedRect(innerRect, 16, 16);

    // Light modern card look
    painter.fillPath(path, QColor(255, 255, 255));

    // Subtle exterior hairline perimeter
    painter.setPen(QPen(QColor(229, 231, 235, 180), 1));
    painter.drawPath(path);
}