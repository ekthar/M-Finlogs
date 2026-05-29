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

constexpr int kCardWidth = 500;
constexpr int kCardHeight = 300;
constexpr int kShadowMargin = 40;

QString splashQss() {
    return QStringLiteral(
        "QFrame#splashCard { background: transparent; border: none; }"
        "QFrame#splashAura {"
        " background: qradialgradient(cx:0.5, cy:0.5, radius:0.72,"
        " stop:0 rgba(61,120,201,115), stop:0.62 rgba(10,20,35,35), stop:1 rgba(10,20,35,0));"
        " border-radius: 120px; }"
        "QFrame#tileLeft { background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #f8bd53, stop:1 #e58f1f); border-radius: 18px; border: 2px solid rgba(255,255,255,0.35); }"
        "QFrame#tileMid { background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #63ddcf, stop:1 #36a6da); border-radius: 20px; border: 2px solid rgba(255,255,255,0.35); }"
        "QFrame#tileRight { background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #7a74ff, stop:1 #6156e9); border-radius: 18px; border: 2px solid rgba(255,255,255,0.35); }"
        "QFrame#tileCore { background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #fdfdfd, stop:1 #e7eef9); border-radius: 22px; border: 2px solid rgba(255,255,255,0.8); }"
        "QFrame#logoWrap { background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #66e2d4, stop:1 #55b5ea); border-radius: 18px; }"
        "QLabel#splashLogoText { color: #ffffff; font-size: 28px; font-weight: 800; }"
        "QLabel#splashBrand { color: #f3f7ff; font-size: 44px; font-weight: 800; }"
        "QLabel#splashStatus { color: #dbe7fb; font-size: 11px; font-weight: 700; letter-spacing: 1px; }"
        "QProgressBar#splashProgress { background: rgba(255,255,255,0.18); border: none; border-radius: 2px; max-height: 4px; min-height: 4px; }"
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

    QGraphicsDropShadowEffect* shadow = new QGraphicsDropShadowEffect(card_);
    shadow->setBlurRadius(38);
    shadow->setOffset(0, 18);
    shadow->setColor(QColor(4, 10, 23, 130));
    card_->setGraphicsEffect(shadow);

    // Aura glow background
    QFrame* aura = createSplashTile(QStringLiteral("splashAura"), card_, QRect(40, 20, 420, 220));

    // Decorative tiles - will animate in from below
    QFrame* tileLeft = createSplashTile(QStringLiteral("tileLeft"), card_, QRect(140, 52, 98, 122));
    QFrame* tileRight = createSplashTile(QStringLiteral("tileRight"), card_, QRect(276, 88, 96, 118));
    QFrame* tileMid = createSplashTile(QStringLiteral("tileMid"), card_, QRect(204, 48, 110, 136));
    QFrame* core = createSplashTile(QStringLiteral("tileCore"), card_, QRect(196, 34, 114, 142));
    QFrame* logoWrap = createSplashTile(QStringLiteral("logoWrap"), core, QRect(23, 36, 68, 68));

    QLabel* logo = new QLabel(QStringLiteral("M"), logoWrap);
    logo->setObjectName(QStringLiteral("splashLogoText"));
    logo->setAlignment(Qt::AlignCenter);
    logo->setGeometry(0, 0, 68, 68);

    QLabel* brand = new QLabel(QStringLiteral("FINLOGS"), card_);
    brand->setObjectName(QStringLiteral("splashBrand"));
    brand->setAlignment(Qt::AlignCenter);
    brand->setGeometry(0, 194, kCardWidth, 48);

    statusLabel_ = new QLabel(QStringLiteral("STARTING"), card_);
    statusLabel_->setObjectName(QStringLiteral("splashStatus"));
    statusLabel_->setAlignment(Qt::AlignCenter);
    statusLabel_->setGeometry(0, 248, kCardWidth, 18);

    progress_ = new QProgressBar(card_);
    progress_->setObjectName(QStringLiteral("splashProgress"));
    progress_->setRange(0, 100);
    progress_->setValue(0);
    progress_->setTextVisible(false);
    progress_->setGeometry(162, 274, 176, 4);

    outer->addWidget(card_);

    // --- Entry animations ---
    // Staggered tile slide-in from below with fade (OutBack for subtle bounce)
    animateTileEntry(*tileLeft, 0, QPoint(140, 82), QPoint(140, 52));
    animateTileEntry(*tileMid, 120, QPoint(204, 78), QPoint(204, 48));
    animateTileEntry(*tileRight, 240, QPoint(276, 118), QPoint(276, 88));
    animateTileEntry(*core, 80, QPoint(196, 64), QPoint(196, 34));

    // Logo breathe pulse
    animateLogoPulse(*logoWrap);

    // Brand and status text fade in after tiles settle
    animateFadeIn(*brand, 400);
    animateFadeIn(*statusLabel_, 550);
    animateFadeIn(*progress_, 650, 400);

    // Aura glow pulse (continuous)
    animateAuraPulse(*aura);
}

void SplashScreen::animateTileEntry(QWidget& tile, int delayMs, const QPoint& from, const QPoint& to) {
    // Start tile at the "from" position (below final)
    tile.move(from);

    // Opacity fade-in
    QGraphicsOpacityEffect* opacity = new QGraphicsOpacityEffect(&tile);
    opacity->setOpacity(0.0);
    tile.setGraphicsEffect(opacity);

    QPropertyAnimation* fade = new QPropertyAnimation(opacity, "opacity", &tile);
    fade->setDuration(500);
    fade->setStartValue(0.0);
    fade->setEndValue(1.0);
    fade->setEasingCurve(QEasingCurve::OutCubic);

    // Position slide upward with slight overshoot
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
    logoPulse_->setDuration(1800);
    logoPulse_->setStartValue(1.0);
    logoPulse_->setKeyValueAt(0.5, 1.08);
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

    // Start after tiles have mostly settled
    QTimer::singleShot(500, logoPulse_, [this]() { logoPulse_->start(); });
}

void SplashScreen::animateAuraPulse(QWidget& aura) {
    QGraphicsOpacityEffect* effect = new QGraphicsOpacityEffect(&aura);
    effect->setOpacity(0.4);
    aura.setGraphicsEffect(effect);

    QPropertyAnimation* pulse = new QPropertyAnimation(effect, "opacity", &aura);
    pulse->setDuration(2200);
    pulse->setStartValue(0.4);
    pulse->setKeyValueAt(0.5, 0.78);
    pulse->setEndValue(0.4);
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
