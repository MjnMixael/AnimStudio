#pragma once

#include <QWidget>
#include <QTimer>

class SpinnerWidget : public QWidget {
    Q_OBJECT

public:
    explicit SpinnerWidget(QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QTimer timer;
    int angle = 0;
};
