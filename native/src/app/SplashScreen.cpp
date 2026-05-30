#include "app/SplashScreen.h"

#include <QApplication>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QProgressBar>
#include <QPropertyAnimation>
#include <QScreen>
#include <QSequentialAnimationGroup>
#include <QShowEvent>
#include <QSvgRenderer>
#include <QTimer>
#include <QVariantAnimation>
#include <QVBoxLayout>

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace mfinlogs::app {

namespace {

constexpr int kSplashWidth = 680;
constexpr int kSplashHeight = 420;

} // namespace

// Painted aurora backdrop with floating gradient blobs and frosted glass panel
class SplashCanvas final : public QWidget {
public:
    explicit SplashCanvas(QWidget* parent = nullptr) : QWidget(parent) {
        // Continuous pulse animation driving the glow ring + blob drift
        pulse_ = new QVariantAnimation(this);
        pulse_->setStartValue(0.0);
        pulse_->setEndValue(1.0);
        pulse_->setDuration(2400);
        pulse_->setLoopCount(-1);
        pulse_->setEasingCurve(QEasingCurve::InOutSine);
        connect(pulse_, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
            phase_ = v.toDouble();
            update();
        });
        pulse_->start();
    }

    double logoCx = 96.0;
    double logoCy = 126.0;

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const QRect r = rect();

        // Deep aurora radial gradient
        QRadialGradient bg(r.center(), r.width() * 0.8);
        bg.setColorAt(0.0, QColor(0x31, 0x2e, 0x81));
        bg.setColorAt(0.4, QColor(0x1b, 0x24, 0x40));
        bg.setColorAt(1.0, QColor(0x0b, 0x10, 0x20));
        p.fillRect(r, bg);

        // Animated floating blobs (drift with phase)
        const double drift = std::sin(phase_ * 2.0 * M_PI) * 24.0;
        QRadialGradient blob1(QPointF(r.width() * 0.2 + drift, r.height() * 0.25), 240);
        blob1.setColorAt(0.0, QColor(99, 102, 241, 90));
        blob1.setColorAt(1.0, Qt::transparent);
        p.fillRect(r, blob1);

        QRadialGradient blob2(QPointF(r.width() * 0.8 - drift, r.height() * 0.7), 220);
        blob2.setColorAt(0.0, QColor(139, 92, 246, 70));
        blob2.setColorAt(1.0, Qt::transparent);
        p.fillRect(r, blob2);

        QRadialGradient blob3(QPointF(r.width() * 0.75, r.height() * 0.2 + drift * 0.5), 170);
        blob3.setColorAt(0.0, QColor(56, 189, 248, 50));
        blob3.setColorAt(1.0, Qt::transparent);
        p.fillRect(r, blob3);

        // Pulsing glow ring behind the logo
        const double glowR = 52.0 + phase_ * 14.0;
        const int glowAlpha = static_cast<int>(120 * (1.0 - phase_ * 0.6));
        QRadialGradient glow(QPointF(logoCx, logoCy), glowR);
        glow.setColorAt(0.0, QColor(124, 140, 255, glowAlpha));
        glow.setColorAt(0.7, QColor(124, 140, 255, glowAlpha / 3));
        glow.setColorAt(1.0, Qt::transparent);
        p.setBrush(glow);
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPointF(logoCx, logoCy), glowR, glowR);

        // Orbiting accent dots around the logo
        for (int i = 0; i < 3; ++i) {
            const double ang = phase_ * 2.0 * M_PI + i * (2.0 * M_PI / 3.0);
            const double ox = logoCx + std::cos(ang) * 58.0;
            const double oy = logoCy + std::sin(ang) * 58.0;
            const QColor dotColor = i == 0 ? QColor(0x5b, 0x8c, 0xfa)
                                  : i == 1 ? QColor(0x8b, 0x5c, 0xf6)
                                           : QColor(0x38, 0xbd, 0xf8);
            p.setBrush(dotColor);
            p.drawEllipse(QPointF(ox, oy), 3.5, 3.5);
        }

        // Frosted glass central panel
        QPainterPath glass;
        glass.addRoundedRect(QRectF(40, 60, r.width() - 80, r.height() - 120), 24, 24);
        p.fillPath(glass, QColor(255, 255, 255, 16));
        p.setPen(QPen(QColor(255, 255, 255, 45), 1.2));
        p.drawPath(glass);

        // Subtle grid pattern
        p.setPen(QPen(QColor(255, 255, 255, 7), 0.5));
        for (int x = 0; x < r.width(); x += 40) {
            p.drawLine(x, 0, x, r.height());
        }
        for (int y = 0; y < r.height(); y += 40) {
            p.drawLine(0, y, r.width(), y);
        }

        // Top sheen
        QLinearGradient sheen(0, 0, 0, r.height() * 0.3);
        sheen.setColorAt(0.0, QColor(255, 255, 255, 12));
        sheen.setColorAt(1.0, Qt::transparent);
        p.fillRect(r, sheen);
    }

private:
    QVariantAnimation* pulse_ = nullptr;
    double phase_ = 0.0;
};

SplashScreen::SplashScreen(QWidget* parent)
    : QWidget(parent) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::SplashScreen | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedSize(kSplashWidth, kSplashHeight);
    buildUi();

    if (QScreen* screen = QApplication::primaryScreen()) {
        const QRect available = screen->availableGeometry();
        move(available.center() - rect().center());
    }
}

void SplashScreen::buildUi() {
    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

    card_ = new QFrame(this);
    card_->setFixedSize(kSplashWidth, kSplashHeight);
    card_->setStyleSheet(QStringLiteral("QFrame { border-radius: 16px; }"));

    QGraphicsDropShadowEffect* shadow = new QGraphicsDropShadowEffect(card_);
    shadow->setBlurRadius(60);
    shadow->setOffset(0, 20);
    shadow->setColor(QColor(0, 0, 20, 200));
    card_->setGraphicsEffect(shadow);

    // Painted canvas background
    SplashCanvas* canvas = new SplashCanvas(card_);
    canvas->setGeometry(0, 0, kSplashWidth, kSplashHeight);

    // --- Logo SVG (rendered from qrc) ---
    QLabel* logoLabel = new QLabel(card_);
    logoLabel->setGeometry(60, 90, 72, 72);
    logoLabel->setStyleSheet(QStringLiteral("background: transparent;"));
    QSvgRenderer logoRenderer(QStringLiteral(":/icons/logo.svg"));
    if (logoRenderer.isValid()) {
        QPixmap logoPix(72, 72);
        logoPix.fill(Qt::transparent);
        QPainter logoPainter(&logoPix);
        logoRenderer.render(&logoPainter);
        logoLabel->setPixmap(logoPix);
    }

    // Brand title
    QLabel* title = new QLabel(QStringLiteral("M-FINLOGS"), card_);
    title->setGeometry(60, 175, 400, 48);
    title->setStyleSheet(QStringLiteral(
        "color: #ffffff; font-size: 38px; font-weight: 800;"
        "letter-spacing: -1px; background: transparent;"));

    // Subtitle
    QLabel* subtitle = new QLabel(QStringLiteral("NEXT-GEN FINANCIAL WORKSPACE"), card_);
    subtitle->setGeometry(60, 226, 400, 20);
    subtitle->setStyleSheet(QStringLiteral(
        "color: #8ea2ff; font-size: 11px; font-weight: 500;"
        "letter-spacing: 3px; background: transparent;"));

    // Tagline
    QLabel* tagline = new QLabel(QStringLiteral("Crafted by EKTHAR"), card_);
    tagline->setGeometry(60, 254, 300, 16);
    tagline->setStyleSheet(QStringLiteral(
        "color: rgba(255,255,255,0.4); font-size: 10px;"
        "letter-spacing: 1px; background: transparent;"));

    // --- Status section (bottom) ---
    statusLabel_ = new QLabel(QStringLiteral("INITIALIZING RUNTIME"), card_);
    statusLabel_->setGeometry(60, 330, 350, 16);
    statusLabel_->setStyleSheet(QStringLiteral(
        "color: #8ea2ff; font-size: 10px; font-weight: 600;"
        "letter-spacing: 2px; background: transparent;"));

    QLabel* statusLine1 = new QLabel(QStringLiteral("Connecting to SQL Server..."), card_);
    statusLine1->setGeometry(60, 350, 350, 14);
    statusLine1->setStyleSheet(QStringLiteral(
        "color: rgba(255,255,255,0.35); font-size: 10px; background: transparent;"));

    // Progress bar (full width, bottom)
    progress_ = new QProgressBar(card_);
    progress_->setGeometry(60, 378, kSplashWidth - 120, 4);
    progress_->setRange(0, 100);
    progress_->setValue(0);
    progress_->setTextVisible(false);
    progress_->setStyleSheet(QStringLiteral(
        "QProgressBar { background: rgba(255,255,255,0.08); border: none; border-radius: 2px; }"
        "QProgressBar::chunk { background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        " stop:0 #5b8cfa, stop:0.5 #8b5cf6, stop:1 #38bdf8); border-radius: 2px; }"));

    // Version (bottom-right)
    QLabel* version = new QLabel(QStringLiteral("v2.0.0 \u2022 Aurora"), card_);
    version->setGeometry(kSplashWidth - 180, kSplashHeight - 34, 160, 14);
    version->setAlignment(Qt::AlignRight);
    version->setStyleSheet(QStringLiteral(
        "color: rgba(255,255,255,0.25); font-size: 10px; background: transparent;"));

    root->addWidget(card_);

    // --- Window fade-in + scale animation ---
    setWindowOpacity(0.0);
    QPropertyAnimation* fadeIn = new QPropertyAnimation(this, "windowOpacity", this);
    fadeIn->setDuration(600);
    fadeIn->setStartValue(0.0);
    fadeIn->setEndValue(1.0);
    fadeIn->setEasingCurve(QEasingCurve::OutCubic);
    fadeIn->start(QAbstractAnimation::DeleteWhenStopped);

    // Animate logo entrance (fade + spring scale-in from center)
    QGraphicsOpacityEffect* logoEffect = new QGraphicsOpacityEffect(logoLabel);
    logoEffect->setOpacity(0.0);
    logoLabel->setGraphicsEffect(logoEffect);
    QPropertyAnimation* logoFade = new QPropertyAnimation(logoEffect, "opacity", this);
    logoFade->setDuration(700);
    logoFade->setStartValue(0.0);
    logoFade->setEndValue(1.0);
    logoFade->setEasingCurve(QEasingCurve::OutCubic);

    // Spring scale: grow from a small centered rect to the final 72x72
    QPropertyAnimation* logoScale = new QPropertyAnimation(logoLabel, "geometry", this);
    logoScale->setDuration(900);
    logoScale->setStartValue(QRect(84, 114, 24, 24));   // small, centered ~(96,126)
    logoScale->setEndValue(QRect(60, 90, 72, 72));       // final
    logoScale->setEasingCurve(QEasingCurve::OutBack);    // spring overshoot

    QTimer::singleShot(150, this, [logoFade, logoScale]() {
        logoFade->start(QAbstractAnimation::DeleteWhenStopped);
        logoScale->start(QAbstractAnimation::DeleteWhenStopped);
    });

    // Animate title entrance
    QGraphicsOpacityEffect* titleEffect = new QGraphicsOpacityEffect(title);
    titleEffect->setOpacity(0.0);
    title->setGraphicsEffect(titleEffect);
    QPropertyAnimation* titleFade = new QPropertyAnimation(titleEffect, "opacity", this);
    titleFade->setDuration(700);
    titleFade->setStartValue(0.0);
    titleFade->setEndValue(1.0);
    titleFade->setEasingCurve(QEasingCurve::OutCubic);
    QTimer::singleShot(500, this, [titleFade]() {
        titleFade->start(QAbstractAnimation::DeleteWhenStopped);
    });
}

void SplashScreen::animateTileEntry(QWidget&, int, const QPoint&, const QPoint&) {}
void SplashScreen::animateFadeIn(QWidget&, int, int) {}
void SplashScreen::animateLogoPulse(QWidget&) {}
void SplashScreen::animateAuraPulse(QWidget&) {}

void SplashScreen::setProgress(int value, const QString& message) {
    if (progress_) {
        progress_->setValue(qBound(0, value, 100));
    }
    if (statusLabel_ && !message.isEmpty()) {
        statusLabel_->setText(message.toUpper());
    }
}

void SplashScreen::runIndeterminate(int durationMs) {
    tickTarget_ = 100;
    if (!tickTimer_) {
        tickTimer_ = new QTimer(this);
        connect(tickTimer_, &QTimer::timeout, this, [this]() {
            if (!progress_) return;
            const int next = progress_->value() + 2;
            progress_->setValue(next);
            if (next >= tickTarget_) {
                tickTimer_->stop();
                emit finished();
            }
        });
    }
    const int remainingSteps = qMax(1, (tickTarget_ - progress_->value()) / 2);
    tickStepMs_ = qMax(8, durationMs / remainingSteps);
    tickTimer_->start(tickStepMs_);
}

void SplashScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
}

} // namespace mfinlogs::app
