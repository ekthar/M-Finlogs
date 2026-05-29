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
#include <QTimer>
#include <QVariantAnimation>
#include <QVBoxLayout>

namespace mfinlogs::app {

namespace {

constexpr int kSplashWidth = 620;
constexpr int kSplashHeight = 400;

} // namespace

// Custom painted widget for the cobalt splash background with geometric elements
class SplashCanvas final : public QWidget {
public:
    explicit SplashCanvas(QWidget* parent = nullptr) : QWidget(parent) {}

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const QRect r = rect();

        // Deep cobalt radial gradient background
        QRadialGradient bg(r.center(), r.width() * 0.7);
        bg.setColorAt(0.0, QColor(0, 43, 128));   // cobalt-core
        bg.setColorAt(1.0, QColor(0, 26, 77));    // cobalt-deep
        p.fillRect(r, bg);

        // Interlocking geometric layer 1 (top-left)
        p.save();
        p.translate(-120, -140);
        p.rotate(45);
        QLinearGradient g1(0, 0, 400, 400);
        g1.setColorAt(0.0, QColor(0, 68, 204, 50));
        g1.setColorAt(0.4, Qt::transparent);
        p.setBrush(g1);
        p.setPen(QPen(QColor(255, 255, 255, 12), 1));
        p.drawRect(0, 0, 500, 500);
        p.restore();

        // Interlocking geometric layer 2 (bottom-right)
        p.save();
        p.translate(r.width() - 200, r.height() - 100);
        p.rotate(45);
        QLinearGradient g2(0, 0, -400, -400);
        g2.setColorAt(0.0, QColor(0, 68, 204, 38));
        g2.setColorAt(0.4, Qt::transparent);
        p.setBrush(g2);
        p.setPen(QPen(QColor(255, 255, 255, 8), 1));
        p.drawRect(-400, -400, 500, 500);
        p.restore();

        // Glint sweep (static representation)
        QLinearGradient glint(r.width() * 0.3, 0, r.width() * 0.5, r.height());
        glint.setColorAt(0.0, Qt::transparent);
        glint.setColorAt(0.5, QColor(255, 255, 255, 8));
        glint.setColorAt(1.0, Qt::transparent);
        p.fillRect(r, glint);
    }
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
    // Root layout
    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

    // Card with rounded corners and shadow
    card_ = new QFrame(this);
    card_->setFixedSize(kSplashWidth, kSplashHeight);
    card_->setStyleSheet(QStringLiteral("QFrame { border-radius: 12px; }"));

    QGraphicsDropShadowEffect* shadow = new QGraphicsDropShadowEffect(card_);
    shadow->setBlurRadius(50);
    shadow->setOffset(0, 16);
    shadow->setColor(QColor(0, 10, 30, 180));
    card_->setGraphicsEffect(shadow);

    // Painted canvas background
    SplashCanvas* canvas = new SplashCanvas(card_);
    canvas->setGeometry(0, 0, kSplashWidth, kSplashHeight);

    // --- Brand container (left-aligned, vertically centered) ---
    // Logo mark: two interlocking squares
    QFrame* logoMark = new QFrame(card_);
    logoMark->setGeometry(44, 100, 64, 64);
    logoMark->setStyleSheet(QStringLiteral("background: transparent;"));

    QFrame* sq1 = new QFrame(logoMark);
    sq1->setGeometry(0, 0, 32, 32);
    sq1->setStyleSheet(QStringLiteral(
        "background: #001a4d; border: 3px solid #4d88ff;"
        "box-shadow: 0 0 20px rgba(77,136,255,0.4);"));

    QFrame* sq2 = new QFrame(logoMark);
    sq2->setGeometry(32, 32, 32, 32);
    sq2->setStyleSheet(QStringLiteral(
        "background: transparent; border: 3px solid #e6eeff;"));

    // Title: FINLOGS
    QLabel* title = new QLabel(QStringLiteral("FINLOGS"), card_);
    title->setGeometry(44, 180, 400, 50);
    title->setStyleSheet(QStringLiteral(
        "color: #e6eeff; font-size: 42px; font-weight: 800;"
        "letter-spacing: -2px; background: transparent;"));

    // Subtitle
    QLabel* subtitle = new QLabel(QStringLiteral("NEXT-GEN LEDGER SYSTEMS"), card_);
    subtitle->setGeometry(44, 232, 400, 20);
    subtitle->setStyleSheet(QStringLiteral(
        "color: #4d88ff; font-size: 11px; font-weight: 300;"
        "letter-spacing: 3px; background: transparent;"));

    // --- Bottom meta section ---
    // Status lines (left)
    statusLabel_ = new QLabel(QStringLiteral("INITIALIZING RUNTIME"), card_);
    statusLabel_->setGeometry(44, 320, 300, 16);
    statusLabel_->setStyleSheet(QStringLiteral(
        "color: #4d88ff; font-size: 10px; font-weight: 500;"
        "letter-spacing: 2px; background: transparent;"));

    QLabel* statusLine1 = new QLabel(QStringLiteral("Loading financial schemas..."), card_);
    statusLine1->setGeometry(44, 340, 300, 14);
    statusLine1->setStyleSheet(QStringLiteral(
        "color: #708fcc; font-size: 10px; background: transparent;"));

    QLabel* statusLine2 = new QLabel(QStringLiteral("Connecting SQL Server registers..."), card_);
    statusLine2->setGeometry(44, 356, 300, 14);
    statusLine2->setStyleSheet(QStringLiteral(
        "color: #708fcc; font-size: 10px; background: transparent;"));

    // Progress bar (right side)
    QLabel* loadingLabel = new QLabel(QStringLiteral("INITIALIZING ASSETS"), card_);
    loadingLabel->setGeometry(420, 320, 180, 16);
    loadingLabel->setAlignment(Qt::AlignRight);
    loadingLabel->setStyleSheet(QStringLiteral(
        "color: #e6eeff; font-size: 10px; font-weight: 500;"
        "letter-spacing: 1px; background: transparent;"));

    progress_ = new QProgressBar(card_);
    progress_->setGeometry(380, 342, 200, 3);
    progress_->setRange(0, 100);
    progress_->setValue(0);
    progress_->setTextVisible(false);
    progress_->setStyleSheet(QStringLiteral(
        "QProgressBar { background: rgba(255,255,255,0.05); border: none; border-radius: 1px; }"
        "QProgressBar::chunk { background: #4d88ff; border-radius: 1px; }"));

    // Version tag (bottom-right, rotated)
    QLabel* version = new QLabel(QStringLiteral("STABLE_BUILD_v1.0.0_NATIVE"), card_);
    version->setGeometry(kSplashWidth - 180, kSplashHeight - 30, 170, 14);
    version->setAlignment(Qt::AlignRight);
    version->setStyleSheet(QStringLiteral(
        "color: rgba(255,255,255,0.2); font-size: 9px; background: transparent;"));

    root->addWidget(card_);

    // --- Window fade-in animation ---
    setWindowOpacity(0.0);
    QPropertyAnimation* fadeIn = new QPropertyAnimation(this, "windowOpacity", this);
    fadeIn->setDuration(500);
    fadeIn->setStartValue(0.0);
    fadeIn->setEndValue(1.0);
    fadeIn->setEasingCurve(QEasingCurve::OutCubic);
    fadeIn->start(QAbstractAnimation::DeleteWhenStopped);
}

void SplashScreen::animateTileEntry(QWidget&, int, const QPoint&, const QPoint&) {
    // Not used in cobalt design
}

void SplashScreen::animateFadeIn(QWidget&, int, int) {
    // Not used in cobalt design
}

void SplashScreen::animateLogoPulse(QWidget&) {
    // Not used in cobalt design
}

void SplashScreen::animateAuraPulse(QWidget&) {
    // Not used in cobalt design
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
