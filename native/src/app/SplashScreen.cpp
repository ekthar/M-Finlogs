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

    createSplashTile(QStringLiteral("splashAura"), card_, QRect(40, 20, 420, 220));
    createSplashTile(QStringLiteral("tileLeft"), card_, QRect(140, 52, 98, 122));
    createSplashTile(QStringLiteral("tileRight"), card_, QRect(276, 88, 96, 118));
    createSplashTile(QStringLiteral("tileMid"), card_, QRect(204, 48, 110, 136));
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
