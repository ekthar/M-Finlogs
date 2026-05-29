#pragma once

#include <QWidget>

class QFrame;
class QLabel;
class QProgressBar;
class QTimer;
class QVariantAnimation;
class QPropertyAnimation;

namespace mfinlogs::app {

// Modern frameless branded splash screen shown while the desktop application
// initialises. Features a dark gradient card with animated decorative tiles,
// company logo with breathe pulse, staggered text reveals, and a smooth
// progress bar. The window is translucent and borderless for a premium feel.
class SplashScreen final : public QWidget {
    Q_OBJECT

public:
    explicit SplashScreen(QWidget* parent = nullptr);

    // Advance the progress bar to an absolute value (0..100) and update the
    // status caption shown above the bar.
    void setProgress(int value, const QString& message = QString());

    // Animate the progress bar from its current value to 100 over the given
    // duration, then emit finished(). Useful when the real work is fast and
    // we still want a smooth, premium reveal.
    void runIndeterminate(int durationMs = 1600);

signals:
    void finished();

protected:
    void showEvent(QShowEvent* event) override;

private:
    void buildUi();
    void animateTileEntry(QWidget& tile, int delayMs, const QPoint& from, const QPoint& to);
    void animateFadeIn(QWidget& widget, int delayMs, int durationMs = 500);
    void animateLogoPulse(QWidget& logo);
    void animateAuraPulse(QWidget& aura);

    QFrame* card_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QProgressBar* progress_ = nullptr;
    QTimer* tickTimer_ = nullptr;
    QVariantAnimation* logoPulse_ = nullptr;
    int tickTarget_ = 100;
    int tickStepMs_ = 16;
};

} // namespace mfinlogs::app
