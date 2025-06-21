#include "SpinnerWidget.h"
#include <QPainter>
#include <QtMath>

SpinnerWidget::SpinnerWidget(QWidget* parent)
    : QWidget(parent)
{
    setFixedSize(64, 64); // Can be resized
    connect(&timer, &QTimer::timeout, this, [this]() {
        angle = (angle + 30) % 360;
        update();
        });
    timer.start(100); // Speed of spin
}

void SpinnerWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    int radius = qMin(width(), height()) / 2 - 4;
    QPoint center = rect().center();

    for (int i = 0; i < 12; ++i) {
        QColor color = Qt::black;
        color.setAlphaF(i / 12.0);
        p.setPen(Qt::NoPen);
        p.setBrush(color);

        double theta = qDegreesToRadians(static_cast<double>(angle + i * 30));
        int x = center.x() + qCos(theta) * radius;
        int y = center.y() + qSin(theta) * radius;

        p.drawEllipse(QPoint(x, y), 3, 3);
    }
}
