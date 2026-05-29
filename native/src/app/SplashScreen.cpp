#include "app/SplashScreen.h"

#include <QApplication>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QScreen>
#include <QShowEvent>
#include <QTimer>
#include <QVBoxLayout>

namespace mfinlogs::app {

namespace {

constexpr int kCardWidth = 460;
constexpr int kCardHeight = 280;
constexpr int kShadowMargin = 40;

QString splashQss() {
    return QStringLiteral(
        // Card — frosted dark gradient, subtle border, heavy rounded corners
        "QFrame#splashCard {"
        " background: qlineargradient(x1:0, y1:0, x2:1, y2:1,"
        "   stop:0 #1b2330, stop:0.55 #131a25, stop:1 #0b0f17);"
        " border: 1px solid #2a3342; border-radius: 16px; }"

        // Logo — bold brand + thin suffix
        "QLabel#splashBrand { color: #ffffff; font-size: 34px; font-weight: 800;"
        " letter-spacing: -0.5px; }"
        "QLabel#splashBrandThin { color: #8fb4e8; font-size: 34px; font-weight: 200;"
        " letter-spacing: -0.5px; }"

        // Tagline under the logo
        "QLabel#splashTagline { color: #8a94a6; font-size: 12px; font-weight: 500;"
        " letter-spacing: 0.5px; }"

        // Status caption above the progress bar
        "QLabel#splashStatus { color: #6b7688; font-size: 11px; font-weight: 500; }"

        // Version text (bottom right)
        "QLabel#splashVersion { color: #5b6576; font-size: 10px; font-weight: 500; }"

        // Minimalist progress bar — 4px, rounded, vibrant blue chunk
        "QProgressBar#splashProgress {"
        " background: rgba(255,255,255,0.08); border: none; border-radius: 2px;"
        " max-height: 4px; min-height: 4px; }"
        "QProgressBar#splashProgress::chunk {"
        " background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
        "   stop:0 #3b82f6, stop:1 #60a5fa); border-radius: 2px; }"
    );
}

} // namespace

SplashScreen::SplashScreen(QWidget* parent)
    : QWidget(parent) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::SplashScreen | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedSize(kCardWidth + kShadowMargin * 2, kCardHeight + kShadowMargin * 2);
    buildUi();

    // Centre on the primary screen.
    if (QScreen* screen = QApplication::primaryScreen()) {
        const QRect available = screen->availableGeometry();
        move(available.center() - rect().center());
    }
}

void SplashScreen::buildUi() {
    QVBoxLayout* outer = new QVBoxLayout(this);
    outer->setContentsMargins(kShadowMargin, kShadowMargin, kShadowMargin, kShadowMargin);

    card_ = new QFrame(this);
    card_->setObjectName(QStringLiteral("splashCard"));
    card_->setStyleSheet(splashQss());

    QGraphicsDropShadowEffect* shadow = new QGraphicsDropShadowEffect(card_);
    shadow->setBlurRadius(48);
    shadow->setOffset(0, 14);
    shadow->setColor(QColor(0, 0, 0, 160));
    card_->setGraphicsEffect(shadow);

    QVBoxLayout* cardLayout = new QVBoxLayout(card_);
    cardLayout->setContentsMargins(36, 36, 36, 28);
    cardLayout->setSpacing(0);

    // ── Logo row (centred) ──────────────────────────────────────────────────
    QWidget* logoRow = new QWidget(card_);
    QHBoxLayout* logoLayout = new QHBoxLayout(logoRow);
    logoLayout->setContentsMargins(0, 0, 0, 0);
    logoLayout->setSpacing(8);
    QLabel* brand = new QLabel(QStringLiteral("M-Finlogs"), logoRow);
    QLabel* brandThin = new QLabel(QStringLiteral("Native"), logoRow);
    brand->setObjectName(QStringLiteral("splashBrand"));
    brandThin->setObjectName(QStringLiteral("splashBrandThin"));
    logoLayout->addStretch(1);
    logoLayout->addWidget(brand);
    logoLayout->addWidget(brandThin);
    logoLayout->addStretch(1);

    QLabel* tagline = new QLabel(QStringLiteral("PREMIUM ACCOUNTING WORKSPACE"), card_);
    tagline->setObjectName(QStringLiteral("splashTagline"));
    tagline->setAlignment(Qt::AlignCenter);

    cardLayout->addStretch(1);
    cardLayout->addWidget(logoRow);
    cardLayout->addSpacing(8);
    cardLayout->addWidget(tagline);
    cardLayout->addStretch(1);

    // ── Status caption ──────────────────────────────────────────────────────
    statusLabel_ = new QLabel(QStringLiteral("Starting up..."), card_);
    statusLabel_->setObjectName(QStringLiteral("splashStatus"));
    cardLayout->addWidget(statusLabel_);
    cardLayout->addSpacing(8);

    // ── Progress bar ────────────────────────────────────────────────────────
    progress_ = new QProgressBar(card_);
    progress_->setObjectName(QStringLiteral("splashProgress"));
    progress_->setRange(0, 100);
    progress_->setValue(0);
    progress_->setTextVisible(false);
    cardLayout->addWidget(progress_);
    cardLayout->addSpacing(10);

    // ── Version footer ──────────────────────────────────────────────────────
    QLabel* version = new QLabel(QStringLiteral("Version 2.0  •  Native Runtime"), card_);
    version->setObjectName(QStringLiteral("splashVersion"));
    version->setAlignment(Qt::AlignRight);
    cardLayout->addWidget(version);

    outer->addWidget(card_);
}

void SplashScreen::setProgress(int value, const QString& message) {
    if (progress_) {
        progress_->setValue(qBound(0, value, 100));
    }
    if (statusLabel_ && !message.isEmpty()) {
        statusLabel_->setText(message);
    }
}

void SplashScreen::runIndeterminate(int durationMs) {
    tickTarget_ = 100;
    if (!tickTimer_) {
        tickTimer_ = new QTimer(this);
        connect(tickTimer_, &QTimer::timeout, this, [this]() {
            if (!progress_) {
                return;
            }
            const int next = progress_->value() + 2;
            progress_->setValue(next);
            if (next >= tickTarget_) {
                tickTimer_->stop();
                emit finished();
            }
        });
    }
    // Number of 2% steps remaining, spread evenly across durationMs.
    const int remainingSteps = qMax(1, (tickTarget_ - progress_->value()) / 2);
    tickStepMs_ = qMax(8, durationMs / remainingSteps);
    tickTimer_->start(tickStepMs_);
}

void SplashScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
}

} // namespace mfinlogs::app
