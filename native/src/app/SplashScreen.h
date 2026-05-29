#pragma once

#include <QWidget>

class QFrame;
class QLabel;
class QProgressBar;
class QTimer;
class QVariantAnimation;

namespace mfinlogs::app {

// Modern frameless splash screen shown while the desktop application
// initialises. Uses a translucent top-level widget so the rounded-corner
// card floats on the desktop with a soft drop shadow. Tiles animate in
// with staggered slide+fade, the logo pulses gently, and the aura glows.
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
    void startEntryAnimations();
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
