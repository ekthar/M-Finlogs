#ifndef AUTHWINDOW_H
#define AUTHWINDOW_H

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>

class AuthWindow : public QWidget {
    Q_OBJECT

public:
    explicit AuthWindow(QWidget *parent = nullptr);
    ~AuthWindow() override = default;

signals:
    void authenticated();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QLineEdit *usernameField;
    QLineEdit *passwordField;
    QPushButton *loginButton;
    QPushButton *closeButton;
    QLabel *errorLabel;

    void handleLogin();
};

#endif // AUTHWINDOW_H