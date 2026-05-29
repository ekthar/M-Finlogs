#include "app/SplashScreen.h"

#include <QApplication>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QParallelAnimationGroup>
#include <QProgressBar>
#include <QPropertyAnimation>
#include <QScreen>
#include <QShowEvent>
#include <QTimer>
#include <QVariantAnimation>
#include <QVBoxLayout>

namespace mfinlogs::app {

namespace {

constexpr int kCardWidth = 540;
constexpr int kCardHeight = 340;
constexpr int kShadowMargin = 44;

QString splashQss() {
    return QStringLiteral(
        // Card background - dark gradient for premium feel
        "QFrame#splashCard {"
        " background: qlineargradient(x1:0,y1:0,x2:1,y2:1,"
        " stop:0 #0f172a, stop:0.5 #1e293b, stop:1 #0f172a);"
        " border: 1px solid rgba(255,255,255,0.08);"
        " border-radius: 20px; }"

        // Aura glow behind tiles
        "QFrame#splashAura {"
        " background: qradialgradient(cx:0.5, cy:0.5, radius:0.72,"
        " stop:0 rgba(61,120,201,115), stop:0.62 rgba(10,20,35,35), stop:1 rgba(10,20,35,0));"
        " border-radius: 120px; }"

        // Decorative tiles
        "QFrame#tileLeft { background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #f8bd53, stop:1 #e58f1f); border-radius: 16px; border: 2px solid rgba(255,255,255,0.3); }"
        "QFrame#tileMid { background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #63ddcf, stop:1 #36a6da); border-radius: 18px; border: 2px solid rgba(255,255,255,0.3); }"
        "QFrame#tileRight { background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #7a74ff, stop:1 #6156e9); border-radius: 16px; border: 2px solid rgba(255,255,255,0.3); }"
        "QFrame#tileCore { background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #fdfdfd, stop:1 #e7eef9); border-radius: 20px; border: 2px solid rgba(255,255,255,0.8); }"

        // Logo circle
        "QFrame#logoWrap { background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #66e2d4, stop:1 #55b5ea); border-radius: 18px; }"
        "QLabel#splashLogoText { color: #ffffff; font-size: 28px; font-weight: 800; }"

        // Brand text
        "QLabel#splashBrand { color: #f1f5f9; font-size: 38px; font-weight: 800; letter-spacing: 2px; }"
        "QLabel#splashTagline { color: rgba(203,213,225,0.85); font-size: 11px; font-weight: 600; letter-spacing: 1px; }"

        // Status text
        "QLabel#splashStatus { color: #94a3b8; font-size: 10px; font-weight: 700; letter-spacing: 1px; }"

        // Version chip
        "QLabel#splashVersion { color: rgba(148,163,184,0.6); font-size: 9px; }"

        // Progress bar
        "QProgressBar#splashProgress { background: rgba(255,255,255,0.1); border: none; border-radius: 2px; max-height: 4px; min-height: 4px; }"
        "QProgressBar#splashProgress::chunk { background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #56d6c6, stop:1 #7267f6); border-radius: 2px; }"
    );
}

QFrame* createSplashTile(const QString& objectName, QWidget* parent, const QRect& geometry) {
    QFrame* tile = new QFrame(parent);
    tile->setObjectName(objectName);
    tile->setGeometry(geometry);
    return tile;
}

} // namespace

SplashScreen::SplashScreen(QWidget* parent)
    : QWidget(parent) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::SplashScreen | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedSize(kCardWidth + kShadowMargin * 2, kCardHeight + kShadowMargin * 2);
    buildUi();

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
    card_->setFixedSize(kCardWidth, kCardHeight);
    card_->setStyleSheet(splashQss());

    // Drop shadow for floating effect
    QGraphicsDropShadowEffect* shadow = new QGraphicsDropShadowEffect(card_);
    shadow->setBlurRadius(44);
    shadow->setOffset(0, 20);
    shadow->setColor(QColor(4, 10, 23, 160));
    card_->setGraphicsEffect(shadow);

    // Aura glow background
    QFrame* aura = createSplashTile(QStringLiteral("splashAura"), card_, QRect(50, 10, 440, 200));

    // Decorative tiles - positioned in upper area
    QFrame* tileLeft = createSplashTile(QStringLiteral("tileLeft"), card_, QRect(150, 42, 88, 110));
    QFrame* tileRight = createSplashTile(QStringLiteral("tileRight"), card_, QRect(310, 72, 86, 106));
    QFrame* tileMid = createSplashTile(QStringLiteral("tileMid"), card_, QRect(218, 38, 100, 124));
    QFrame* core = createSplashTile(QStringLiteral("tileCore"), card_, QRect(212, 26, 106, 130));
    QFrame* logoWrap = createSplashTile(QStringLiteral("logoWrap"), core, QRect(21, 32, 64, 64));

    // Logo "M" letter
    QLabel* logo = new QLabel(QStringLiteral("M"), logoWrap);
    logo->setObjectName(QStringLiteral("splashLogoText"));
    logo->setAlignment(Qt::AlignCenter);
    logo->setGeometry(0, 0, 64, 64);

    // Brand name
    QLabel* brand = new QLabel(QStringLiteral("M-FINLOGS"), card_);
    brand->setObjectName(QStringLiteral("splashBrand"));
    brand->setAlignment(Qt::AlignCenter);
    brand->setGeometry(0, 176, kCardWidth, 44);

    // Tagline
    QLabel* tagline = new QLabel(QStringLiteral("NATIVE ACCOUNTING WORKSPACE"), card_);
    tagline->setObjectName(QStringLiteral("splashTagline"));
    tagline->setAlignment(Qt::AlignCenter);
    tagline->setGeometry(0, 220, kCardWidth, 18);

    // Status message
    statusLabel_ = new QLabel(QStringLiteral("INITIALIZING"), card_);
    statusLabel_->setObjectName(QStringLiteral("splashStatus"));
    statusLabel_->setAlignment(Qt::AlignCenter);
    statusLabel_->setGeometry(0, 272, kCardWidth, 16);

    // Progress bar
    progress_ = new QProgressBar(card_);
    progress_->setObjectName(QStringLiteral("splashProgress"));
    progress_->setRange(0, 100);
    progress_->setValue(0);
    progress_->setTextVisible(false);
    progress_->setGeometry(170, 296, 200, 4);

    // Version label
    QLabel* version = new QLabel(QStringLiteral("v1.0.0 | Native Runtime"), card_);
    version->setObjectName(QStringLiteral("splashVersion"));
    version->setAlignment(Qt::AlignCenter);
    version->setGeometry(0, 316, kCardWidth, 14);

    outer->addWidget(card_);

    // --- Entry animations ---
    // Staggered tile slide-in from below with OutBack easing (subtle bounce)
    animateTileEntry(*tileLeft, 0, QPoint(150, 72), QPoint(150, 42));
    animateTileEntry(*tileMid, 120, QPoint(218, 68), QPoint(218, 38));
    animateTileEntry(*tileRight, 240, QPoint(310, 102), QPoint(310, 72));
    animateTileEntry(*core, 80, QPoint(212, 56), QPoint(212, 26));

    // Logo breathe pulse (starts after tiles settle)
    animateLogoPulse(*logoWrap);

    // Brand, tagline, status, progress fade in sequentially
    animateFadeIn(*brand, 380, 450);
    animateFadeIn(*tagline, 520, 400);
    animateFadeIn(*statusLabel_, 650, 350);
    animateFadeIn(*progress_, 700, 350);
    animateFadeIn(*version, 800, 350);

    // Aura glow pulse (continuous)
    animateAuraPulse(*aura);
}

void SplashScreen::animateTileEntry(QWidget& tile, int delayMs, const QPoint& from, const QPoint& to) {
    tile.move(from);

    QGraphicsOpacityEffect* opacity = new QGraphicsOpacityEffect(&tile);
    opacity->setOpacity(0.0);
    tile.setGraphicsEffect(opacity);

    QPropertyAnimation* fade = new QPropertyAnimation(opacity, "opacity", &tile);
    fade->setDuration(500);
    fade->setStartValue(0.0);
    fade->setEndValue(1.0);
    fade->setEasingCurve(QEasingCurve::OutCubic);

    QPropertyAnimation* slide = new QPropertyAnimation(&tile, "pos", &tile);
    slide->setDuration(600);
    slide->setStartValue(from);
    slide->setEndValue(to);
    slide->setEasingCurve(QEasingCurve::OutBack);

    QParallelAnimationGroup* group = new QParallelAnimationGroup(&tile);
    group->addAnimation(fade);
    group->addAnimation(slide);

    QTimer::singleShot(delayMs, group, [group]() {
        group->start(QAbstractAnimation::DeleteWhenStopped);
    });
}

void SplashScreen::animateFadeIn(QWidget& widget, int delayMs, int durationMs) {
    QGraphicsOpacityEffect* effect = new QGraphicsOpacityEffect(&widget);
    effect->setOpacity(0.0);
    widget.setGraphicsEffect(effect);

    QPropertyAnimation* fade = new QPropertyAnimation(effect, "opacity", &widget);
    fade->setDuration(durationMs);
    fade->setStartValue(0.0);
    fade->setEndValue(1.0);
    fade->setEasingCurve(QEasingCurve::OutCubic);

    QTimer::singleShot(delayMs, fade, [fade]() {
        fade->start(QAbstractAnimation::DeleteWhenStopped);
    });
}

void SplashScreen::animateLogoPulse(QWidget& logo) {
    const QRect baseRect = logo.geometry();

    logoPulse_ = new QVariantAnimation(this);
    logoPulse_->setDuration(2000);
    logoPulse_->setStartValue(1.0);
    logoPulse_->setKeyValueAt(0.5, 1.06);
    logoPulse_->setEndValue(1.0);
    logoPulse_->setEasingCurve(QEasingCurve::InOutSine);
    logoPulse_->setLoopCount(-1);

    connect(logoPulse_, &QVariantAnimation::valueChanged, &logo, [&logo, baseRect](const QVariant& value) {
        const double scale = value.toDouble();
        const int w = static_cast<int>(baseRect.width() * scale);
        const int h = static_cast<int>(baseRect.height() * scale);
        const int x = baseRect.x() - (w - baseRect.width()) / 2;
        const int y = baseRect.y() - (h - baseRect.height()) / 2;
        logo.setGeometry(x, y, w, h);
    });

    QTimer::singleShot(600, logoPulse_, [this]() { logoPulse_->start(); });
}

void SplashScreen::animateAuraPulse(QWidget& aura) {
    QGraphicsOpacityEffect* effect = new QGraphicsOpacityEffect(&aura);
    effect->setOpacity(0.35);
    aura.setGraphicsEffect(effect);

    QPropertyAnimation* pulse = new QPropertyAnimation(effect, "opacity", &aura);
    pulse->setDuration(2400);
    pulse->setStartValue(0.35);
    pulse->setKeyValueAt(0.5, 0.72);
    pulse->setEndValue(0.35);
    pulse->setEasingCurve(QEasingCurve::InOutSine);
    pulse->setLoopCount(-1);
    pulse->start();
}

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
    const int remainingSteps = qMax(1, (tickTarget_ - progress_->value()) / 2);
    tickStepMs_ = qMax(8, durationMs / remainingSteps);
    tickTimer_->start(tickStepMs_);
}

void SplashScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
}

} // namespace mfinlogs::app
