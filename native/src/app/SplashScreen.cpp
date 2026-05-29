#include "SplashScreen.h"
#include <QVBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QApplication>
#include <QScreen>
#include <QGraphicsDropShadowEffect>

SplashScreen::SplashScreen(QWidget *parent) 
    : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool) {
    
    // Allows painting custom rounded shapes without OS backgrounds
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedSize(500, 320);
    setWindowOpacity(0.0); // Start fully transparent for animation

    if (const auto *screen = QApplication::primaryScreen()) {
        move(screen->geometry().center() - rect().center());
    }

    // Modern drop shadow layer
    auto *shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(25);
    shadow->setColor(QColor(0, 0, 0, 100));
    shadow->setOffset(0, 8);

    // Frame hosting the container components
    auto *containerFrame = new QFrame(this);
    containerFrame->setObjectName("ContainerFrame");
    containerFrame->setGraphicsEffect(shadow);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(15, 15, 15, 15);
    mainLayout->addWidget(containerFrame);

    // Layout inside the styled frame
    auto *innerLayout = new QVBoxLayout(containerFrame);
    innerLayout->setContentsMargins(35, 45, 35, 30);
    innerLayout->setSpacing(15);

    logoLabel = new QLabel(containerFrame);
    logoLabel->setAlignment(Qt::AlignCenter);
    logoLabel->setText("<span style='font-size:36px; font-weight:800; color:#FFFFFF; font-family:\"Segoe UI\",\"Inter\";'>M-Finlogs</span>"
                       "<br><span style='font-size:13px; font-weight:600; color:#3B82F6; letter-spacing:4px;'>NATIVE ACCOUNTING</span>");
    innerLayout->addWidget(logoLabel);

    innerLayout->addStretch();

    statusLabel = new QLabel("Connecting to native core engine...", containerFrame);
    statusLabel->setStyleSheet("color: #9CA3AF; font-size: 11px; font-family: \"Segoe UI\";");
    statusLabel->setAlignment(Qt::AlignLeft);
    innerLayout->addWidget(statusLabel);

    progressBar = new QProgressBar(containerFrame);
    progressBar->setRange(0, 100);
    progressBar->setValue(0);
    progressBar->setFixedHeight(3);
    progressBar->setTextVisible(false);
    progressBar->setStyleSheet(
        "QProgressBar { background-color: rgba(255, 255, 255, 0.08); border: none; border-radius: 1.5px; }"
        "QProgressBar::chunk { background-color: #3B82F6; border-radius: 1.5px; }"
    );
    innerLayout->addWidget(progressBar);

    versionLabel = new QLabel("Version 2026.1 • Native Runtime", containerFrame);
    versionLabel->setStyleSheet("color: #4B5563; font-size: 10px; font-family: \"Segoe UI\";");
    versionLabel->setAlignment(Qt::AlignRight);
    innerLayout->addWidget(versionLabel);
}

void SplashScreen::startFadeIn() {
    show();
    auto *anim = new QPropertyAnimation(this, "windowOpacity");
    anim->setDuration(400);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void SplashScreen::startFadeOut(std::function<void()> onFinished) {
    auto *anim = new QPropertyAnimation(this, "windowOpacity");
    anim->setDuration(350);
    anim->setStartValue(1.0);
    anim->setEndValue(0.0);
    anim->setEasingCurve(QEasingCurve::InCubic);
    connect(anim, &QPropertyAnimation::finished, this, [this, onFinished]() {
        hide();
        onFinished();
    });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void SplashScreen::setProgress(int value, const QString &message) {
    progressBar->setValue(value);
    statusLabel->setText(message);
    qApp->processEvents();
}

void SplashScreen::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Redraw the main frame with high-fidelity rounded corners
    QPainterPath path;
    // Account for padding margins introduced by the drop-shadow boundary
    QRect shadowMarginRect = rect().adjusted(15, 15, -15, -15);
    path.addRoundedRect(shadowMarginRect, 16, 16);

    QLinearGradient gradient(shadowMarginRect.topLeft(), shadowMarginRect.bottomRight());
    gradient.setColorAt(0.0, QColor(17, 24, 39, 245)); // Slate 900 premium dark translucency
    gradient.setColorAt(1.0, QColor(3, 7, 18, 255));   // Slate 950 deep base

    painter.fillPath(path, gradient);

    // Minimal sharp inner perimeter glow
    painter.setPen(QPen(QColor(255, 255, 255, 22), 1));
    painter.drawPath(path);
}