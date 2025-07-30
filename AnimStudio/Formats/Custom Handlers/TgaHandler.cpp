#include "TgaHandler.h"
#include <QDebug>

#pragma pack(push, 1)
struct TGAHeader {
    quint8 idLength;
    quint8 colorMapType;
    quint8 imageType;
    quint16 colorMapFirstEntry;
    quint16 colorMapLength;
    quint8 colorMapEntrySize;
    quint16 xOrigin;
    quint16 yOrigin;
    quint16 width;
    quint16 height;
    quint8 pixelDepth;
    quint8 imageDescriptor;
};
#pragma pack(pop)

bool TgaHandler::read(QImage* outImage) {
    if (!m_device) {
        qWarning() << "TGA handler has no device";
        return false;
    }

    TGAHeader header;
    if (m_device->read(reinterpret_cast<char*>(&header), sizeof(header)) != sizeof(header)) {
        qWarning() << "Failed to read TGA header";
        return false;
    }

    if (header.imageType != 1 && header.imageType != 2 && header.imageType != 10) {
        qWarning() << "Unsupported TGA image type" << header.imageType;
        return false;
    }

    // Skip ID field if present
    if (header.idLength > 0)
        m_device->seek(m_device->pos() + header.idLength);

    QVector<QRgb> colorTable;
    if (header.imageType == 1 && header.colorMapType == 1) {
        const int colorMapSize = header.colorMapLength * (header.colorMapEntrySize / 8);
        QByteArray colorMapData = m_device->read(colorMapSize);
        if (colorMapData.size() != colorMapSize) {
            qWarning() << "Failed to read TGA color map";
            return false;
        }

        const uchar* data = reinterpret_cast<const uchar*>(colorMapData.constData());
        for (int i = 0; i < header.colorMapLength; ++i) {
            int index = i * (header.colorMapEntrySize / 8);
            uchar b = data[index];
            uchar g = data[index + 1];
            uchar r = data[index + 2];
            colorTable.append(qRgb(r, g, b));
        }
    }

    int width = header.width;
    int height = header.height;
    int bpp = header.pixelDepth;

    bool flipVertical = (header.imageDescriptor & 0x20) == 0;

    int yStart = flipVertical ? height - 1 : 0;
    int yEnd = flipVertical ? -1 : height;
    int yStep = flipVertical ? -1 : 1;

    QImage img;

    if (header.imageType == 1) {
        // Indexed color
        img = QImage(width, height, QImage::Format_Indexed8);
        img.setColorTable(colorTable);
        for (int y = yStart; y != yEnd; y += yStep) {
            uchar* scanline = img.scanLine(y);
            if (m_device->read(reinterpret_cast<char*>(scanline), width) != width) {
                qWarning() << "Failed to read TGA scanline";
                return false;
            }
        }
    } else if (header.imageType == 2) {
        // Uncompressed truecolor
        QImage::Format format = (bpp == 32) ? QImage::Format_RGBA8888 : QImage::Format_RGB888;
        img = QImage(width, height, format);

        for (int y = yStart; y != yEnd; y += yStep) {
            uchar* scanline = img.scanLine(y);
            int lineBytes = width * (bpp / 8);
            if (m_device->read(reinterpret_cast<char*>(scanline), lineBytes) != lineBytes) {
                qWarning() << "Failed to read TGA scanline";
                return false;
            }

            // Swap BGR(A) to RGB(A)
            for (int x = 0; x < width; ++x) {
                uchar* px = scanline + x * (bpp / 8);
                std::swap(px[0], px[2]);
            }
        }
    } else {
        qWarning() << "Unsupported or compressed TGA type";
        return false;
    }

    *outImage = img;
    return true;
}

bool TgaHandler::write(const QImage& image) {
    if (!m_device)
        return false;

    QImage img = image.convertToFormat(QImage::Format_RGBA8888);
    int width = img.width();
    int height = img.height();

    TGAHeader header = {};
    header.imageType = 2; // uncompressed truecolor
    header.width = width;
    header.height = height;
    header.pixelDepth = 32;
    header.imageDescriptor = 0x20; // top-left origin

    if (m_device->write(reinterpret_cast<const char*>(&header), sizeof(header)) != sizeof(header)) {
        qWarning() << "TGA: Failed to write header";
        return false;
    }

    for (int y = 0; y < height; ++y) {
        const uchar* src = img.constScanLine(y);
        for (int x = 0; x < width; ++x) {
            uchar r = src[x * 4 + 0];
            uchar g = src[x * 4 + 1];
            uchar b = src[x * 4 + 2];
            uchar a = src[x * 4 + 3];

            m_device->putChar(b);
            m_device->putChar(g);
            m_device->putChar(r);
            m_device->putChar(a);
        }
    }

    return true;
}
