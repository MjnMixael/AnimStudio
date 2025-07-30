#pragma once

#include <QImage>
#include <QIODevice>

class TgaHandler {
public:
    void setDevice(QIODevice* device) { m_device = device; }

    bool read(QImage* image);
    bool write(const QImage& image);

private:
    QIODevice* m_device = nullptr;
};
