#include "app/SplashScreen.h"

#include <QApplication>
#include <QColor>
#include <QFont>
#include <QFontDatabase>
#include <QGraphicsDropShadowEffect>
#include <QLabel>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QProgressBar>
#include <QPropertyAnimation>
#include <QRadialGradient>
#include <QRandomGenerator>
#include <QScreen>
#include <QShowEvent>
#ifdef HAS_QT_SVG
#include <QSvgRenderer>
#endif
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

// Brand palette (ported from the Aurora HTML splash)
const QColor kBgIndigo(0x31, 0x2E, 0x81);
const QColor kBgSlate(0x1B, 0x24, 0x40);
const QColor kBgDark(0x0B, 0x10, 0x20);
const QColor kAccentIndigo(0x63, 0x66, 0xF1);
const QColor kAccentViolet(0x8B, 0x5C, 0xF6);
const QColor kAccentSky(0x38, 0xBD, 0xF8);
const QColor kAccentBlueLogo(0x5B, 0x8C, 0xFA);
const QColor kTextTint(0x8E, 0xA2, 0xFF);

// Logo geometry: badge is 72x72 centred inside a 96x96 wrapper.
constexpr double kLogoCx = kSplashWidth / 2.0;          // 340
constexpr double kLogoCy = 150.0;                        // brand block sits above centre
constexpr double kBadge = 72.0;
constexpr double kRing = 96.0;

} // namespace

// Fully painted Aurora canvas: gradient backdrop, drifting blurred blobs,
// technical grid, frosted glass panel, pulsing logo glow, rotating orbit ring
// with three accent dots, and the gradient logo badge with the "M" mark.
class SplashCanvas final : public QWidget {
public:
    explicit SplashCanvas(QWidget* parent = nullptr) : QWidget(parent) {
#ifdef HAS_QT_SVG
        logo_.load(QStringLiteral(":/icons/logo.svg"));
#endif

        anim_ = new QVariantAnimation(this);
        anim_->setStartValue(0.0);
        anim_->setEndValue(1.0);
        anim_->setDuration(6000);          // one full orbit revolution
        anim_->setLoopCount(-1);
        anim_->setEasingCurve(QEasingCurve::Linear);
        connect(anim_, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
            phase_ = v.toDouble();
            update();
        });
        anim_->start();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const QRectF r = rect();

        // Rounded clip so everything respects the 24px corner radius.
        QPainterPath clip;
        clip.addRoundedRect(r, 24, 24);
        p.setClipPath(clip);

        paintBackground(p, r);
        paintBlobs(p, r);
        paintGrid(p, r);
        paintGlassPanel(p, r);
        paintLogoGlow(p);
        paintOrbit(p);
        paintLogoBadge(p);
        paintProgressTrack(p, r);
    }

private:
    void paintBackground(QPainter& p, const QRectF& r) {
        QRadialGradient bg(r.center(), r.width() * 0.62);
        bg.setColorAt(0.0, kBgIndigo);
        bg.setColorAt(0.45, kBgSlate);
        bg.setColorAt(1.0, kBgDark);
        p.fillRect(r, bg);
    }

    // Three large, heavily-blurred, slowly drifting colour blobs.
    void paintBlobs(QPainter& p, const QRectF& r) {
        const double t = phase_ * 2.0 * M_PI;
        auto blob = [&](QPointF c, double radius, QColor color, double op) {
            QRadialGradient g(c, radius);
            QColor inner = color; inner.setAlphaF(op);
            QColor outer = color; outer.setAlpha(0);
            g.setColorAt(0.0, inner);
            g.setColorAt(1.0, outer);
            p.setBrush(g);
            p.setPen(Qt::NoPen);
            p.drawEllipse(c, radius, radius);
        };
        blob(QPointF(120 + std::sin(t) * 26, 70 + std::cos(t) * 18), 200, kAccentIndigo, 0.35);
        blob(QPointF(r.width() - 110 - std::sin(t) * 26, r.height() - 60 + std::cos(t) * 20), 230, kAccentViolet, 0.32);
        blob(QPointF(r.width() / 2.0, r.height() / 2.0 + std::sin(t + 1.0) * 16), 170, kAccentSky, 0.20);
    }

    void paintGrid(QPainter& p, const QRectF& r) {
        p.setPen(QPen(QColor(255, 255, 255, 8), 1.0));
        for (int x = 0; x < r.width(); x += 40) {
            p.drawLine(QPointF(x, 0), QPointF(x, r.height()));
        }
        for (int y = 0; y < r.height(); y += 40) {
            p.drawLine(QPointF(0, y), QPointF(r.width(), y));
        }
    }

    // Frosted glass panel: translucent fill, hairline border, top sheen.
    void paintGlassPanel(QPainter& p, const QRectF& r) {
        QPainterPath panel;
        panel.addRoundedRect(r, 24, 24);
        p.fillPath(panel, QColor(255, 255, 255, 20));
        p.setPen(QPen(QColor(255, 255, 255, 46), 1.2));
        p.drawPath(panel);

        QLinearGradient sheen(0, 0, 0, r.height() * 0.4);
        sheen.setColorAt(0.0, QColor(255, 255, 255, 13));
        sheen.setColorAt(1.0, QColor(255, 255, 255, 0));
        QPainterPath sheenPath;
        sheenPath.addRoundedRect(QRectF(0, 0, r.width(), r.height() * 0.4), 24, 24);
        p.fillPath(sheenPath, sheen);
    }

    // Pulsing soft glow behind the logo (opacity + scale, ~3s cycle).
    void paintLogoGlow(QPainter& p) {
        const double pulse = (std::sin(phase_ * 4.0 * M_PI) + 1.0) / 2.0; // 0..1, 2 cycles per revolution
        const double radius = 42.0 + pulse * 16.0;
        const int alpha = static_cast<int>(90 + pulse * 60);
        QRadialGradient glow(QPointF(kLogoCx, kLogoCy), radius);
        QColor c = kAccentIndigo; c.setAlpha(alpha);
        QColor edge = kAccentIndigo; edge.setAlpha(0);
        glow.setColorAt(0.0, c);
        glow.setColorAt(1.0, edge);
        p.setBrush(glow);
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPointF(kLogoCx, kLogoCy), radius, radius);
    }

    // Rotating orbit ring + three accent dots fixed on it.
    void paintOrbit(QPainter& p) {
        const double ringR = kRing / 2.0;
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(255, 255, 255, 26), 1.0));
        p.drawEllipse(QPointF(kLogoCx, kLogoCy), ringR, ringR);

        const double base = phase_ * 2.0 * M_PI;
        struct Dot { double angleOffset; QColor color; };
        const Dot dots[] = {
            { -M_PI / 2.0, kAccentIndigo },               // top
            { M_PI / 6.0, kAccentViolet },                // lower-right
            { M_PI - M_PI / 6.0, kAccentSky },            // lower-left
        };
        for (const Dot& d : dots) {
            const double a = base + d.angleOffset;
            const QPointF pos(kLogoCx + std::cos(a) * ringR, kLogoCy + std::sin(a) * ringR);
            // Glow halo
            QColor halo = d.color; halo.setAlpha(120);
            p.setBrush(halo);
            p.setPen(Qt::NoPen);
            p.drawEllipse(pos, 6, 6);
            p.setBrush(d.color);
            p.drawEllipse(pos, 3, 3);
        }
    }

    // Gradient rounded-square badge with the M / ledger / sparkline mark.
    void paintLogoBadge(QPainter& p) {
        const QRectF badge(kLogoCx - kBadge / 2.0, kLogoCy - kBadge / 2.0, kBadge, kBadge);

        // Drop shadow for the badge
        QColor shadow = kAccentIndigo; shadow.setAlpha(90);
        p.setBrush(shadow);
        p.setPen(Qt::NoPen);
        QPainterPath sh;
        sh.addRoundedRect(badge.translated(0, 8), 18, 18);
        // (Soft-ish: draw without blur — keep it subtle)
        p.fillPath(sh, QColor(99, 102, 241, 40));

#ifdef HAS_QT_SVG
        if (logo_.isValid()) {
            logo_.render(&p, badge);
        } else
#endif
        {
            QLinearGradient g(badge.topLeft(), badge.bottomRight());
            g.setColorAt(0.0, kAccentBlueLogo);
            g.setColorAt(1.0, kAccentViolet);
            QPainterPath bp;
            bp.addRoundedRect(badge, 18, 18);
            p.fillPath(bp, g);
            QFont f(QStringLiteral("Inter Tight"), 30, QFont::Bold);
            p.setFont(f);
            p.setPen(Qt::white);
            p.drawText(badge, Qt::AlignCenter, QStringLiteral("M"));
        }
    }

    // Track is painted; the fill is a styled QProgressBar child for animation.
    void paintProgressTrack(QPainter& p, const QRectF& r) {
        Q_UNUSED(r);
        Q_UNUSED(p);
    }

#ifdef HAS_QT_SVG
    QSvgRenderer logo_;
#endif
    QVariantAnimation* anim_ = nullptr;
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
    card_->setStyleSheet(QStringLiteral("QFrame { border-radius: 24px; background: transparent; }"));

    QGraphicsDropShadowEffect* shadow = new QGraphicsDropShadowEffect(card_);
    shadow->setBlurRadius(80);
    shadow->setOffset(0, 30);
    shadow->setColor(QColor(0, 0, 20, 200));
    card_->setGraphicsEffect(shadow);

    SplashCanvas* canvas = new SplashCanvas(card_);
    canvas->setGeometry(0, 0, kSplashWidth, kSplashHeight);

    // Tagline (top-right)
    QLabel* tagline = new QLabel(QStringLiteral("Crafted by EKTHAR"), card_);
    tagline->setGeometry(kSplashWidth - 260, 34, 220, 16);
    tagline->setAlignment(Qt::AlignRight);
    tagline->setStyleSheet(QStringLiteral(
        "color: rgba(255,255,255,0.30); font-family: 'Inter'; font-size: 10px; background: transparent;"));

    // Title (centred under the logo)
    QLabel* title = new QLabel(QStringLiteral("M-FINLOGS"), card_);
    title->setGeometry(0, 196, kSplashWidth, 48);
    title->setAlignment(Qt::AlignHCenter);
    title->setStyleSheet(QStringLiteral(
        "color: #FFFFFF; font-family: 'Inter Tight'; font-size: 38px; font-weight: 700;"
        "letter-spacing: -1px; background: transparent;"));

    // Subtitle (tracked-out uppercase tint)
    QLabel* subtitle = new QLabel(QStringLiteral("N E X T - G E N   F I N A N C I A L   W O R K S P A C E"), card_);
    subtitle->setGeometry(0, 246, kSplashWidth, 18);
    subtitle->setAlignment(Qt::AlignHCenter);
    subtitle->setStyleSheet(QStringLiteral(
        "color: #8EA2FF; font-family: 'Inter'; font-size: 10px; font-weight: 500;"
        "letter-spacing: 2px; background: transparent;"));

    // Status line (bottom-left, monospace)
    statusLabel_ = new QLabel(QStringLiteral("Initializing runtime..."), card_);
    statusLabel_->setGeometry(40, kSplashHeight - 66, 420, 16);
    statusLabel_->setStyleSheet(QStringLiteral(
        "color: rgba(142,162,255,0.70); font-family: 'Space Mono','Consolas',monospace;"
        "font-size: 10px; background: transparent;"));

    // Version badge (bottom-right, monospace)
    QLabel* version = new QLabel(QStringLiteral("v2.0.0  \u2022  Aurora"), card_);
    version->setGeometry(kSplashWidth - 240, kSplashHeight - 66, 200, 16);
    version->setAlignment(Qt::AlignRight);
    version->setStyleSheet(QStringLiteral(
        "color: rgba(255,255,255,0.25); font-family: 'Space Mono','Consolas',monospace;"
        "font-size: 10px; background: transparent;"));

    // Progress bar (full width, bottom) with the tri-stop gradient fill
    progress_ = new QProgressBar(card_);
    progress_->setGeometry(40, kSplashHeight - 44, kSplashWidth - 80, 4);
    progress_->setRange(0, 100);
    progress_->setValue(0);
    progress_->setTextVisible(false);
    progress_->setStyleSheet(QStringLiteral(
        "QProgressBar { background: rgba(255,255,255,0.05); border: none; border-radius: 2px; }"
        "QProgressBar::chunk { border-radius: 2px; background: qlineargradient("
        "x1:0,y1:0,x2:1,y2:0, stop:0 #5B8CFA, stop:0.5 #8B5CF6, stop:1 #38BDF8); }"));

    root->addWidget(card_);

    // Window fade-in
    setWindowOpacity(0.0);
    QPropertyAnimation* fadeIn = new QPropertyAnimation(this, "windowOpacity", this);
    fadeIn->setDuration(550);
    fadeIn->setStartValue(0.0);
    fadeIn->setEndValue(1.0);
    fadeIn->setEasingCurve(QEasingCurve::OutCubic);
    fadeIn->start(QAbstractAnimation::DeleteWhenStopped);

    // Cycling status log lines (mirrors the HTML JS)
    static const QStringList kLogs = {
        QStringLiteral("Initializing engine..."),
        QStringLiteral("Connecting to encrypted storage..."),
        QStringLiteral("Authenticating with SQL Server..."),
        QStringLiteral("Loading financial ledgers..."),
        QStringLiteral("Syncing inventory assets..."),
        QStringLiteral("Optimizing workspace...")
    };
    int* logIndex = new int(0);
    QTimer* logTimer = new QTimer(this);
    connect(logTimer, &QTimer::timeout, this, [this, logIndex, logTimer]() {
        if (*logIndex >= kLogs.size()) {
            logTimer->stop();
            delete logIndex;
            return;
        }
        if (statusLabel_) {
            statusLabel_->setText(kLogs.at(*logIndex));
        }
        *logIndex += 1;
        logTimer->setInterval(700 + (QRandomGenerator::global()->bounded(900)));
    });
    QTimer::singleShot(400, this, [logTimer]() { logTimer->start(700); });
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
    const int remainingSteps = qMax(1, (tickTarget_ - progress_->value()) / 2);
    tickStepMs_ = qMax(8, durationMs / remainingSteps);
    tickTimer_->start(tickStepMs_);
}

void SplashScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
}

} // namespace mfinlogs::app
