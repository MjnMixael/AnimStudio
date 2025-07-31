#pragma once

#include <QImage>
#include <QIODevice>

class DdsHandler {
public:
    void setDevice(const QString& device) { m_device = device; }

    bool read(QImage* image);
    bool write(const QImage& image);

private:
    QString m_device;
};
