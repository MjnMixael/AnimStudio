#include "PcxHandler.h"
#include <QDebug>

// PCX magic number is 0x0A (first byte)
bool PcxHandler::canRead() const {
    if (!m_device) return false;
    QByteArray magic = m_device->peek(1);
    return magic.size() == 1 && static_cast<uchar>(magic[0]) == 0x0A;
}

bool PcxHandler::read(QImage* image) {
    if (!m_device || !image) return false;

    QByteArray rawData = m_device->readAll();
    const uchar* data = reinterpret_cast<const uchar*>(rawData.constData());
    int size = rawData.size();

    if (size < static_cast<int>(sizeof(PCXHeader) + 769)) {
        qWarning() << "PCX file too small";
        return false;
    }

    // Parse header
    const PCXHeader* header = reinterpret_cast<const PCXHeader*>(data);
    if (header->Manufacturer != 0x0A || header->Encoding != 1 || header->BitsPerPixel != 8 || header->Nplanes != 1) {
        qWarning() << "Unsupported PCX format";
        return false;
    }

    int width = header->Xmax - header->Xmin + 1;
    int height = header->Ymax - header->Ymin + 1;
    int bytesPerLine = header->BytesPerLine;

    // Locate image data and palette
    const uchar* imgData = data + sizeof(PCXHeader);
    const uchar* palette = data + size - 768;

    // Check extended palette marker
    if (*(palette - 1) != 0x0C) {
        qWarning() << "Missing extended palette in PCX file";
        return false;
    }

    QImage out(width, height, QImage::Format_Indexed8);

    // Build Qt palette
    QVector<QRgb> colorTable(256);
    for (int i = 0; i < 256; ++i) {
        int base = i * 3;
        colorTable[i] = qRgb(palette[base], palette[base + 1], palette[base + 2]);
    }
    out.setColorTable(colorTable);

    // Decode image data
    const uchar* ptr = imgData;
    int y = 0;

    for (int row = 0; row < height; ++row) {
        uchar* scan = out.scanLine(row);
        int written = 0;

        while (written < bytesPerLine && ptr < palette - 1) {
            uchar byte = *ptr++;
            int count = 1;
            if ((byte & 0xC0) == 0xC0) {
                count = byte & 0x3F;
                if (ptr >= palette - 1) break;
                byte = *ptr++;
            }

            for (int i = 0; i < count && written < bytesPerLine; ++i) {
                if (written < width) {
                    scan[written] = byte;
                }
                ++written;
            }
        }
    }

    *image = out;
    return true;
}

static void writeRleLine(QIODevice* dev, const uchar* data, int length) {
    int i = 0;
    while (i < length) {
        uchar val = data[i];
        int count = 1;
        while (i + count < length && data[i + count] == val && count < 63) {
            ++count;
        }

        if (count > 1 || (val & 0xC0) == 0xC0) {
            dev->putChar(0xC0 | count);
        }
        dev->putChar(val);
        i += count;
    }
}

bool PcxHandler::write(const QImage& image) {
    if (!m_device) {
        qWarning() << "PCX write: device is null";
        return false;
    }

    QImage indexed = image;

    if (image.format() != QImage::Format_Indexed8) {
        qWarning() << "PCX write: converting image to Indexed8";

        indexed = image.convertToFormat(QImage::Format_Indexed8);

        if (indexed.format() != QImage::Format_Indexed8) {
            qWarning() << "PCX write: failed to convert image to Indexed8";
            return false;
        }

        QVector<QRgb> pal = indexed.colorTable();
        while (pal.size() < 256) {
            pal.append(qRgb(0, 0, 0));
        }
        indexed.setColorTable(pal);
    }

    const int width = indexed.width();
    const int height = indexed.height();
    const int bytesPerLine = (width + 1) & ~1; // PCX requires even line size

    // Prepare header
    PCXHeader header = {};
    header.Manufacturer = 0x0A;
    header.Version = 5;
    header.Encoding = 1;
    header.BitsPerPixel = 8;
    header.Xmin = 0;
    header.Ymin = 0;
    header.Xmax = width - 1;
    header.Ymax = height - 1;
    header.Hdpi = width;
    header.Vdpi = height;
    header.Nplanes = 1;
    header.BytesPerLine = bytesPerLine;

    // Write header
    if (m_device->write(reinterpret_cast<const char*>(&header), sizeof(header)) != sizeof(header)) {
        qWarning() << "PCX write: failed to write header";
        return false;
    }

    // Write image data (RLE)
    for (int y = 0; y < height; ++y) {
        const uchar* scan = indexed.constScanLine(y);
        writeRleLine(m_device, scan, width);

        if (bytesPerLine > width) {
            uchar pad = 0;
            writeRleLine(m_device, &pad, bytesPerLine - width);
        }
    }

    // Write palette marker
    m_device->putChar(0x0C);

    // Write 768-byte palette
    const QVector<QRgb> palette = indexed.colorTable();
    if (palette.size() < 256) {
        qWarning() << "PCX write: palette too small after conversion";
        return false;
    }

    for (int i = 0; i < 256; ++i) {
        QRgb color = palette[i];
        m_device->putChar(qRed(color));
        m_device->putChar(qGreen(color));
        m_device->putChar(qBlue(color));
    }

    return true;
}