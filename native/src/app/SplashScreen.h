#ifndef SPLASHSCREEN_H
#define SPLASHSCREEN_H

#include <QWidget>
#include <QProgressBar>
#include <QLabel>
#include <QPropertyAnimation>

class SplashScreen : public QWidget {
    Q_OBJECT

public:
    explicit SplashScreen(QWidget *parent = nullptr);
    ~SplashScreen() override = default;

    void setProgress(int value, const QString &message);
    void startFadeIn();
    void startFadeOut(std::function<void()> onFinished);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QProgressBar *progressBar;
    QLabel *statusLabel;
    QLabel *logoLabel;
    QLabel *versionLabel;
};

#endif // SPLASHSCREEN_H