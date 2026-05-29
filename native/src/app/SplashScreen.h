#pragma once

#include <QWidget>

class QFrame;
class QLabel;
class QProgressBar;
class QTimer;
class QVariantAnimation;
class QPropertyAnimation;

namespace mfinlogs::app {

// Cobalt-themed frameless splash screen with interlocking geometric design,
// gradient background, and progress bar. Matches the burnished cobalt HTML
// reference design with deep blue palette and metallic accents.
class SplashScreen final : public QWidget {
    Q_OBJECT

public:
    explicit SplashScreen(QWidget* parent = nullptr);

    void setProgress(int value, const QString& message = QString());
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
