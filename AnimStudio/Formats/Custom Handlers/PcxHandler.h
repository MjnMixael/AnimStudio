#pragma once

#include <QImageIOHandler>
#include <QImage>
#include <QIODevice>

class PcxHandler : public QImageIOHandler {
public:
    void setDevice(QIODevice* dev) { m_device = dev; }
    void setFormat(const QByteArray& fmt) { m_format = fmt; }

    bool canRead() const override;
    bool read(QImage* image) override;
    bool write(const QImage& image) override;

private:
    QIODevice* m_device = nullptr;
    QByteArray m_format;
};


#pragma pack(push, 1)
struct PCXHeader {
    quint8 Manufacturer;
    quint8 Version;
    quint8 Encoding;
    quint8 BitsPerPixel;
    quint16 Xmin;
    quint16 Ymin;
    quint16 Xmax;
    quint16 Ymax;
    quint16 Hdpi;
    quint16 Vdpi;
    quint8 ColorMap[48]; // legacy 16-color palette
    quint8 Reserved;
    quint8 Nplanes;
    quint16 BytesPerLine;
    quint8 filler[60];
};
#pragma pack(pop)
